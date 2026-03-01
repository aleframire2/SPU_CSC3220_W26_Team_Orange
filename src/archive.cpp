#include "archive.h"
#include "database.h"
#include "flesch.h"
#include "colors.h"
#include <iostream>
#include <string>
#include <iomanip>

static void print_story_list(const std::vector<Story>& stories) {
    if (stories.empty()) {
        std::cout << "  " CLR_DIM "(no stories yet)" CLR_RESET "\n";
        return;
    }
    std::cout << "\n  " CLR_DIM << std::left
              << std::setw(5)  << "ID"
              << std::setw(30) << "Title"
              << std::setw(8)  << "Words"
              << std::setw(8)  << "Time(s)"
              << std::setw(8)  << "Flesch"
              << "Created" CLR_RESET "\n";
    std::cout << "  " << std::string(70, '-') << "\n";
    for (auto& s : stories) {
        std::cout << "  "
                  << std::setw(5)  << s.story_id
                  << std::setw(30) << s.title.substr(0, 28)
                  << std::setw(8)  << s.total_word_count
                  << std::setw(8)  << s.total_time
                  << std::setw(8)  << std::fixed << std::setprecision(1) << s.flesch_score
                  << s.created_at << "\n";
    }
}

static void view_story(sqlite3* db, int story_id) {
    Story s;
    if (!story_get(db, story_id, s)) {
        std::cout << "  " CLR_RED "[!] Story not found." CLR_RESET "\n";
        return;
    }
    std::cout << "\n  " CLR_BLUE CLR_BOLD "== " << s.title << " ==" CLR_RESET "\n";
    std::cout << "  " CLR_DIM "Created: " << s.created_at << "  |  Words: " << s.total_word_count
              << "  |  Time: " << s.total_time << "s  |  Flesch: "
              << std::fixed << std::setprecision(1) << s.flesch_score << CLR_RESET "\n\n";
    std::cout << s.content << "\n";

    WordChain wc;
    if (wordchain_get_by_story(db, story_id, wc)) {
        auto words = chainword_list(db, wc.chain_id);
        if (!words.empty()) {
            std::cout << "\n  " CLR_DIM "Chain: ";
            for (size_t i = 0; i < words.size(); ++i) {
                if (i > 0) std::cout << " -> ";
                std::cout << words[i].word_text;
            }
            std::cout << CLR_RESET "\n";
        }
    }
}

static void edit_story(sqlite3* db, int story_id) {
    Story s;
    if (!story_get(db, story_id, s)) {
        std::cout << "  " CLR_RED "[!] Story not found." CLR_RESET "\n";
        return;
    }

    std::cout << "  Current title: " << s.title << "\n";
    std::cout << "  New title (Enter to keep): ";
    std::string new_title;
    std::getline(std::cin, new_title);
    if (new_title.empty()) new_title = s.title;

    std::cout << "  Current content:\n" << s.content << "\n\n";
    std::cout << "  Enter new content (type :done on its own line to finish):\n";

    std::string new_content;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line == ":done") break;
        if (!new_content.empty()) new_content += "\n";
        new_content += line;
    }
    if (new_content.empty()) new_content = s.content;

    int    wc = count_words(new_content);
    double fl = flesch_score(new_content, db);

    story_update(db, story_id, new_title, new_content, wc, s.total_time, fl);
    user_update_flesch_avg(db, s.user_id);
    std::cout << "  " CLR_GREEN "Story updated." CLR_RESET " New Flesch score: " << std::fixed << std::setprecision(1) << fl << "\n";
}

void run_archive(sqlite3* db, int user_id) {
    while (true) {
        std::cout << "\n" CLR_BLUE CLR_BOLD "=== LOCAL ARCHIVE ===" CLR_RESET "\n";

        auto stories = story_list(db, user_id);
        print_story_list(stories);

        std::cout << "\n";
        std::cout << "  " CLR_GREEN "v." CLR_RESET " View  ";
        std::cout << CLR_GREEN "e." CLR_RESET " Edit  ";
        std::cout << CLR_GREEN "d." CLR_RESET " Delete  ";
        std::cout << CLR_RED "b." CLR_RESET " Back\n";
        std::cout << CLR_CYAN "  > " CLR_RESET;
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "b" || cmd == "B") break;

        if (cmd == "v" || cmd == "e" || cmd == "d") {
            std::cout << "  Story ID: ";
            std::string id_str;
            std::getline(std::cin, id_str);
            int id = 0;
            try { id = std::stoi(id_str); } catch (...) { std::cout << "  " CLR_RED "Invalid ID." CLR_RESET "\n"; continue; }

            if (cmd == "v") view_story(db, id);
            else if (cmd == "e") edit_story(db, id);
            else if (cmd == "d") {
                std::cout << "  Delete story " << id << "? (y/n): ";
                std::string conf;
                std::getline(std::cin, conf);
                if (conf == "y" || conf == "Y") {
                    story_delete(db, id);
                    std::cout << "  " CLR_GREEN "Deleted." CLR_RESET "\n";
                }
            }
        }
    }
}
