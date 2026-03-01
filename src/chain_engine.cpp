#include "chain_engine.h"
#include "colors.h"
#include "database.h"
#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include <atomic>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#endif

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

// Cross-platform timed line input with live countdown.
// Prints its own timer + prompt lines, returns typed string or "" on timeout.
static std::string timed_input(int seconds) {
    std::string result;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    // Print initial timer line and prompt line
    std::cout << "  " CLR_DIM "Time left:" CLR_RESET " " CLR_GREEN << seconds << "s  " CLR_RESET "\n";
    std::cout << "  Your word: " << std::flush;

#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD  mode;
    GetConsoleMode(hStdin, &mode);
    SetConsoleMode(hStdin, mode & ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT));

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            std::cout << "\n";
            SetConsoleMode(hStdin, mode);
            return "";
        }
        auto ms_left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count();
        DWORD wait = WaitForSingleObject(hStdin, (DWORD)std::min((long long)100, ms_left));
        if (wait == WAIT_OBJECT_0) {
            INPUT_RECORD rec;
            DWORD        read;
            ReadConsoleInput(hStdin, &rec, 1, &read);
            if (rec.EventType == KEY_EVENT && rec.Event.KeyEvent.bKeyDown) {
                char c = rec.Event.KeyEvent.uChar.AsciiChar;
                if (c == '\r' || c == '\n') {
                    std::cout << "\n";
                    break;
                } else if (c == '\b' || c == 127) {
                    if (!result.empty()) {
                        result.pop_back();
                        std::cout << "\b \b" << std::flush;
                    }
                } else if (c >= 32) {
                    result += c;
                    std::cout << c << std::flush;
                }
            }
        } else {
            // No key — update the countdown display
            now = std::chrono::steady_clock::now();
            int secs_left = (int)std::chrono::duration_cast<std::chrono::seconds>(deadline - now).count();
            if (secs_left < 0) secs_left = 0;
            const char* color = secs_left > 5 ? CLR_GREEN : (secs_left > 2 ? CLR_YELLOW : CLR_RED);
            std::cout << "\033[s\033[1A\r  " CLR_DIM "Time left:" CLR_RESET " "
                      << color << secs_left << "s  " CLR_RESET "\033[u" << std::flush;
        }
    }
    SetConsoleMode(hStdin, mode);
#else
    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    newt.c_cc[VMIN]  = 0;
    newt.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    while (true) {
        auto now = std::chrono::steady_clock::now();
        if (now >= deadline) {
            std::cout << "\n";
            break;
        }
        char c = 0;
        ssize_t n = read(STDIN_FILENO, &c, 1);
        if (n > 0) {
            if (c == '\n' || c == '\r') { std::cout << "\n"; break; }
            else if (c == 127 || c == '\b') {
                if (!result.empty()) { result.pop_back(); std::cout << "\b \b" << std::flush; }
            } else if (c >= 32) {
                result += c; std::cout << c << std::flush;
            }
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            now = std::chrono::steady_clock::now();
            int secs_left = (int)std::chrono::duration_cast<std::chrono::seconds>(deadline - now).count();
            if (secs_left < 0) secs_left = 0;
            const char* color = secs_left > 5 ? CLR_GREEN : (secs_left > 2 ? CLR_YELLOW : CLR_RED);
            std::cout << "\033[s\033[1A\r  " CLR_DIM "Time left:" CLR_RESET " "
                      << color << secs_left << "s  " CLR_RESET "\033[u" << std::flush;
        }
    }
    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
    return result;
}

// Main chain reaction loop
std::vector<ChainEntry> run_chain_reaction(sqlite3* db, int timer_sec, int min_chain) {
    std::vector<ChainEntry> chain;

    if (wordpool_count(db) == 0) {
        std::cout << "[!] Word pool is empty. Please import words first.\n";
        return chain;
    }

    std::cout << "\n" CLR_BLUE CLR_BOLD "=== Chain Reaction Engine ===" CLR_RESET "\n";
    std::cout << "  Type a word associated with the prompt.\n";
    std::cout << "  You have " << timer_sec << " seconds per word.\n";
    std::cout << "  After " << min_chain << " links, press Enter on an empty line to stop.\n\n";

    int anchor_id = wordpool_random_id(db);
    std::string anchor = wordpool_get_text(db, anchor_id);

    int seq = 1;
    bool running = true;

    while (running) {
        std::cout << "\n  " CLR_BLUE "[" << seq << "]" CLR_RESET " Anchor: "
                  CLR_CYAN CLR_BOLD << anchor << CLR_RESET "\n";

        auto start = std::chrono::steady_clock::now();
        std::string input = timed_input(timer_sec);
        auto end = std::chrono::steady_clock::now();
        int elapsed_ms = (int)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // Time ran out
        if (input.empty() && elapsed_ms >= timer_sec * 1000 - 200) {
            std::cout << "\n  " CLR_RED "X" CLR_RESET " Time's up! Chain ends at " << seq - 1 << " links.\n";
            break;
        }

        // User chose to stop
        if (input.empty()) {
            if ((int)chain.size() >= min_chain) {
                std::cout << "  Chain complete! " << chain.size() << " links forged.\n";
            } else {
                std::cout << "  [!] You need at least " << min_chain << " links before stopping.\n";
                std::cout << "  Anchor: " CLR_CYAN CLR_BOLD << anchor << CLR_RESET "\n";
                input = timed_input(timer_sec);
                if (input.empty()) {
                    std::cout << "\n  " CLR_RED "X" CLR_RESET " Time's up!\n";
                    break;
                }
            }
            if (input.empty()) break;
        }

        std::string word = to_lower(input);
        while (!word.empty() && std::isspace((unsigned char)word.front())) word.erase(word.begin());
        while (!word.empty() && std::isspace((unsigned char)word.back()))  word.pop_back();

        if (word.empty()) {
            if ((int)chain.size() >= min_chain) break;
            std::cout << "  [!] Empty input, try again.\n";
            continue;
        }

        int word_id = wordpool_get_or_insert(db, word, 0);

        ChainEntry entry;
        entry.anchor_text = anchor;
        entry.word    = word;
        entry.word_id = word_id;
        entry.time_ms = elapsed_ms;
        chain.push_back(entry);

        std::cout << "  " CLR_GREEN "+" CLR_RESET " Link forged in " << elapsed_ms << " ms\n";

        anchor_id = wordpool_random_id(db);
        anchor = wordpool_get_text(db, anchor_id);
        seq++;
    }

    if (!chain.empty()) {
        std::cout << "\n  " CLR_BLUE CLR_BOLD "-- Word Chain --" CLR_RESET "\n  ";
        for (size_t i = 0; i < chain.size(); ++i) {
            if (i > 0) std::cout << " -> ";
            std::cout << CLR_DIM << chain[i].anchor_text << CLR_RESET " -> " CLR_BOLD << chain[i].word << CLR_RESET;
        }
        std::cout << "\n";
    }

    return chain;
}
