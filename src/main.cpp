// main.cpp -- CLI entry point and menu flow for NarrativeLink
#include <iostream>
#include <string>
#include <limits>
#ifdef _WIN32
#include <windows.h>
#endif
#include "colors.h"
#include "database.h"
#include "chain_engine.h"
#include "narrative.h"
#include "archive.h"
#include "analytics.h"
#include "wordpool.h"
#include "flesch.h"
#include "export.h"

// ── helpers ───────────────────────────────────────────────
// Reads a line from stdin after printing the label.
static std::string prompt(const std::string& label) {
    std::cout << "  " << label << ": ";
    std::string s;
    std::getline(std::cin, s);
    return s;
}

// Clears terminal (cls on Windows, clear elsewhere).
static void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// ── auth ──────────────────────────────────────────────────
// Prompts for credentials and populates user on success.
static bool do_login(sqlite3* db, User& user) {
    std::cout << "\n  First name: "; std::string fn; std::getline(std::cin, fn);
    std::cout << "  Last name:  "; std::string ln; std::getline(std::cin, ln);
    std::cout << "  Password:   "; std::string pw; std::getline(std::cin, pw);
    if (user_login(db, fn, ln, pw, user)) {
        std::cout << "  " CLR_GREEN "Welcome back, " CLR_CYAN << user.firstName << CLR_RESET "!\n";
        return true;
    }
    std::cout << "  " CLR_RED "[!] Invalid credentials." CLR_RESET "\n";
    return false;
}

// Prompts for name and password, creates account, and logs in.
static bool do_register(sqlite3* db, User& user) {
    std::cout << "\n  First name: "; std::string fn; std::getline(std::cin, fn);
    std::cout << "  Last name:  "; std::string ln; std::getline(std::cin, ln);
    std::cout << "  Password:   "; std::string pw; std::getline(std::cin, pw);
    std::cout << "  Confirm:    "; std::string pw2; std::getline(std::cin, pw2);
    if (pw != pw2) { std::cout << "  " CLR_RED "[!] Passwords do not match." CLR_RESET "\n"; return false; }
    if (fn.empty() || ln.empty() || pw.empty()) { std::cout << "  " CLR_RED "[!] All fields required." CLR_RESET "\n"; return false; }

    int id = user_create(db, fn, ln, pw);
    if (!user_login(db, fn, ln, pw, user)) {
        std::cout << "  " CLR_RED "[!] Registration failed." CLR_RESET "\n";
        return false;
    }
    std::cout << "  " CLR_GREEN "Account created! Welcome, " CLR_CYAN << user.firstName << CLR_RESET ".\n";
    return true;
}

// ── new session ───────────────────────────────────────────
// Runs chain reaction timer, then optionally narrative synthesis.
static void new_session(sqlite3* db, const User& user) {
    std::cout << "\n  Timer seconds per word [default 10]: ";
    std::string t_str;
    std::getline(std::cin, t_str);
    int timer = 10;
    if (!t_str.empty()) { try { timer = std::stoi(t_str); } catch (...) {} }
    if (timer < 3)  timer = 3;
    if (timer > 60) timer = 60;

    std::cout << "  Minimum chain length [default 5]: ";
    std::string m_str;
    std::getline(std::cin, m_str);
    int min_chain = 5;
    if (!m_str.empty()) { try { min_chain = std::stoi(m_str); } catch (...) {} }
    if (min_chain < 1)  min_chain = 1;

    auto chain = run_chain_reaction(db, timer, min_chain);

    if (chain.empty()) {
        std::cout << "  Session ended with no words.\n";
        return;
    }

    std::cout << "\n  Proceed to Narrative Synthesis? (y/n): ";
    std::string ans;
    std::getline(std::cin, ans);
    if (ans == "y" || ans == "Y")
        run_narrative_session(db, user.user_id, chain);
}

// ── main menu ─────────────────────────────────────────────
// Main app loop: session, archive, analytics, word pool, export.
static void main_menu(sqlite3* db, User& user) {
    while (true) {
        std::cout << "\n" CLR_BLUE CLR_BOLD "=== NarrativeLink ===" CLR_RESET "\n";
        std::cout << "  Hello, " CLR_CYAN << user.firstName << CLR_RESET "\n";
        std::cout << CLR_BLUE "---" CLR_RESET "\n";
        std::cout << "  " CLR_GREEN "1." CLR_RESET " New Session\n";
        std::cout << "  " CLR_GREEN "2." CLR_RESET " Local Archive (CRUD)\n";
        std::cout << "  " CLR_GREEN "3." CLR_RESET " Session Analytics\n";
        std::cout << "  " CLR_GREEN "4." CLR_RESET " Word Pool Manager\n";
        std::cout << "  " CLR_GREEN "5." CLR_RESET " Export Story\n";
        std::cout << "  " CLR_RED "q." CLR_RESET " Quit\n";
        std::cout << CLR_CYAN "  > " CLR_RESET;

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "1") new_session(db, user);
        else if (cmd == "2") run_archive(db, user.user_id);
        else if (cmd == "3") run_analytics(db, user.user_id);
        else if (cmd == "4") run_wordpool_menu(db);
        else if (cmd == "5") run_export_menu(db, user.user_id);
        else if (cmd == "q" || cmd == "Q") {
            std::cout << "  Goodbye!\n";
            break;
        }
    }
}

// ── entry point ───────────────────────────────────────────
// Opens DB, login/register loop, then main menu.
int main() {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    SetConsoleMode(hOut, mode | 0x0004);
#endif

    const std::string db_path = "narrativelink.db";

    sqlite3* db = nullptr;
    try {
        db = db_open(db_path);
        db_init_schema(db);
    } catch (const std::exception& e) {
        std::cerr << CLR_RED "[FATAL] " << e.what() << CLR_RESET "\n";
        return 1;
    }

    std::cout << "\n" CLR_BLUE CLR_BOLD "=== NarrativeLink ===" CLR_RESET "\n";
    std::cout << CLR_DIM "    Gamified Creative Writing CLI" CLR_RESET "\n";

    User user{};
    bool logged_in = false;

    while (!logged_in) {
        std::cout << "\n  " CLR_GREEN "1." CLR_RESET " Login\n";
        std::cout << "  " CLR_GREEN "2." CLR_RESET " Register\n";
        std::cout << "  " CLR_RED "q." CLR_RESET " Quit\n";
        std::cout << CLR_CYAN "  > " CLR_RESET;
        std::string opt;
        std::getline(std::cin, opt);

        if (opt == "1")       logged_in = do_login(db, user);
        else if (opt == "2")  logged_in = do_register(db, user);
        else if (opt == "q" || opt == "Q") { db_close(db); return 0; }
    }

    main_menu(db, user);
    db_close(db);
    return 0;
}
