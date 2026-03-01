#include "narrative.h"
#include "database.h"
#include "flesch.h"
#include <iostream>
#include <string>
#include <chrono>

int run_narrative_session(sqlite3* db, int user_id, const std::vector<ChainEntry>& chain) {
    std::cout << "\n╔══════════════════════════════════════╗\n";
    std::cout << "║     NARRATIVE SYNTHESIS              ║\n";
    std::cout << "╚══════════════════════════════════════╝\n";
    std::cout << "  Write your story incorporating the words below.\n";
    std::cout << "  Type  :done  on its own line when finished.\n";
    std::cout << "  Type  :title <text>  to set the title.\n\n";

    // Show the word chain
    std::cout << "  Word chain: ";
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i > 0) std::cout << " → ";
        std::cout << chain[i].word;
    }
    std::cout << "\n\n";

    std::string title = "Untitled";
    std::string content;
    auto start = std::chrono::steady_clock::now();

    std::string line;
    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) break;

        if (line == ":done") break;

        if (line.rfind(":title ", 0) == 0) {
            title = line.substr(7);
            std::cout << "  [Title set to: " << title << "]\n";
            continue;
        }

        if (!content.empty()) content += "\n";
        content += line;
    }

    auto end = std::chrono::steady_clock::now();
    int elapsed_sec = (int)std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

    if (content.empty()) {
        std::cout << "  [No content entered — session discarded.]\n";
        return -1;
    }

    int   wc    = count_words(content);
    double fl   = flesch_score(content);

    std::cout << "\n  ── Session Stats ──\n";
    std::cout << "  Words:        " << wc << "\n";
    std::cout << "  Time:         " << elapsed_sec << "s\n";
    std::cout << "  Flesch score: " << fl << "\n";

    // Save story
    int story_id = story_create(db, user_id, title, content, wc, elapsed_sec, fl);

    // Compute the total chain time
    int chain_time_ms = 0;
    for (auto& e : chain) chain_time_ms += e.time_ms;

    // Save word chain
    int chain_id = wordchain_create(db, story_id);

    // Save individual chain words
    int seq = 1;
    for (auto& e : chain) {
        chainword_insert(db, chain_id, seq, e.word_id, e.time_ms);
        seq++;
    }

    // Update user's average flesch score
    user_update_flesch_avg(db, user_id);

    std::cout << "  Story saved! (ID " << story_id << ")\n";
    return story_id;
}
