#include "archive.h"
#include "database.h"
#include "flesch.h"
#include <iostream>
#include <string>
#include <iomanip>

static void print_story_list(const std::vector<Story>& stories) {
    if (stories.empty()) {
        std::cout << "  (no stories yet)\n";
        return;
    }
    std::cout << "\n  " << std::left
              << std::setw(5)  << "ID"
              << std::setw(30) << "Title"
              << std::setw(8)  << "Words"
              << std::setw(8)  << "Time(s)"
              << std::setw(8)  << "Flesch"
              << "Created\n";
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
        std::cout << "  [!] Story not found.\n";
        return;
    }
    std::cout << "\n  ══ " << s.title << " ══\n";
    std::cout << "  Created: " << s.created_at << "  |  Words: " << s.total_word_count
              << "  |  Time: " << s.total_time << "s  |  Flesch: "
              << std::fixed << std::setprecision(1) << s.flesch_score << "\n\n";
    std::cout << s.content << "\n";

    // Show word chain if it exists
    WordChain wc;
    if (wordchain_get_by_story(db, story_id, wc)) {
        auto words = chainword_list(db, wc.chain_id);
        if (!words.empty()) {
            std::cout << "\n  Chain: ";
            for (size_t i = 0; i < words.size(); ++i) {
                if (i > 0) std::cout << " → ";
                std::cout << words[i].word_text;
            }
            std::cout << "\n";
        }
    }
}

static void edit_story(sqlite3* db, int story_id) {
    Story s;
    if (!story_get(db, story_id, s)) {
        std::cout << "  [!] Story not found.\n";
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
    double fl = flesch_score(new_content);

    story_update(db, story_id, new_title, new_content, wc, s.total_time, fl);
    user_update_flesch_avg(db, s.user_id);
    std::cout << "  Story updated. New Flesch score: " << std::fixed << std::setprecision(1) << fl << "\n";
}

void run_archive(sqlite3* db, int user_id) {
    while (true) {
        std::cout << "\n╔══════════════════════════════════════╗\n";
        std::cout << "║         LOCAL ARCHIVE                ║\n";
        std::cout << "╚══════════════════════════════════════╝\n";

        auto stories = story_list(db, user_id);
        print_story_list(stories);

        std::cout << "\n  [v] View  [e] Edit  [d] Delete  [b] Back\n  > ";
        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "b" || cmd == "B") break;

        if (cmd == "v" || cmd == "e" || cmd == "d") {
            std::cout << "  Story ID: ";
            std::string id_str;
            std::getline(std::cin, id_str);
            int id = 0;
            try { id = std::stoi(id_str); } catch (...) { std::cout << "  Invalid ID.\n"; continue; }

            if (cmd == "v") view_story(db, id);
            else if (cmd == "e") edit_story(db, id);
            else if (cmd == "d") {
                std::cout << "  Delete story " << id << "? (y/n): ";
                std::string conf;
                std::getline(std::cin, conf);
                if (conf == "y" || conf == "Y") {
                    story_delete(db, id);
                    std::cout << "  Deleted.\n";
                }
            }
        }
    }
}
