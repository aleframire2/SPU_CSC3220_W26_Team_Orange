#include "wordpool.h"
#include "database.h"
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>
#include <cctype>

static std::string trim(std::string s) {
    while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
    while (!s.empty() && std::isspace((unsigned char)s.back()))  s.pop_back();
    return s;
}

static std::string to_lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

int import_wordpool_file(sqlite3* db, const std::string& filepath, const std::string& category) {
    std::ifstream f(filepath);
    if (!f.is_open()) {
        std::cout << "  [!] Cannot open file: " << filepath << "\n";
        return 0;
    }

    int added = 0;
    std::string line;
    while (std::getline(f, line)) {
        std::string word = to_lower(trim(line));
        if (word.empty()) continue;
        // Skip lines that look like comments
        if (word[0] == '#') continue;

        sqlite3_stmt* s;
        sqlite3_prepare_v2(db,
            "INSERT OR IGNORE INTO WORDPOOL(word_text,category) VALUES(?,?);",
            -1, &s, nullptr);
        sqlite3_bind_text(s, 1, word.c_str(),     -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(s, 2, category.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(s);
        if (sqlite3_changes(db) > 0) added++;
        sqlite3_finalize(s);
    }
    return added;
}

void run_wordpool_menu(sqlite3* db) {
    while (true) {
        int count = wordpool_count(db);
        std::cout << "\n╔══════════════════════════════════════╗\n";
        std::cout << "║         WORD POOL                    ║\n";
        std::cout << "╚══════════════════════════════════════╝\n";
        std::cout << "  Current pool size: " << count << " words\n";
        std::cout << "  [i] Import .txt file\n";
        std::cout << "  [l] List all words\n";
        std::cout << "  [b] Back\n  > ";

        std::string cmd;
        std::getline(std::cin, cmd);

        if (cmd == "b" || cmd == "B") break;

        if (cmd == "i" || cmd == "I") {
            std::cout << "  File path: ";
            std::string path;
            std::getline(std::cin, path);
            std::cout << "  Category label (optional): ";
            std::string cat;
            std::getline(std::cin, cat);
            if (cat.empty()) cat = "imported";
            int n = import_wordpool_file(db, trim(path), cat);
            std::cout << "  Added " << n << " new words.\n";
        } else if (cmd == "l" || cmd == "L") {
            auto words = wordpool_all(db);
            std::cout << "\n  Total: " << words.size() << " words\n";
            int shown = 0;
            for (auto& w : words) {
                std::cout << "  " << w.word_text;
                if (!w.category.empty()) std::cout << " [" << w.category << "]";
                std::cout << "\n";
                if (++shown >= 100) {
                    std::cout << "  ... (showing first 100)\n";
                    break;
                }
            }
        }
    }
}

void seed_default_words(sqlite3* db) {
    // Only seed if pool is empty
    if (wordpool_count(db) > 0) return;

    static const char* words[] = {
        // Nature
        "ocean","mountain","forest","river","storm","cloud","fire","stone","wind","rain",
        "leaf","shadow","light","darkness","moon","star","sun","earth","flower","tree",
        // Emotions
        "hope","fear","joy","sorrow","anger","calm","dream","dread","wonder","love",
        "grief","envy","pride","shame","trust","doubt","courage","despair","peace","rage",
        // Actions
        "run","fall","rise","sing","whisper","shout","think","remember","forget","discover",
        "search","create","destroy","build","break","escape","return","vanish","survive","begin",
        // Abstract
        "time","memory","truth","lie","secret","power","chaos","silence","echo","fate",
        "journey","war","freedom","pain","beauty","death","life","soul","mind","void",
        // Objects
        "sword","door","mirror","clock","key","book","map","ship","tower","cage",
        "bridge","road","wall","throne","crown","mask","lantern","letter","coin","blade",
        nullptr
    };

    for (int i = 0; words[i] != nullptr; ++i)
        wordpool_insert(db, words[i], "default");
}
