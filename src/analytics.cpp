// analytics.cpp -- Aggregates and displays session stats and chain metrics
#include "analytics.h"
#include "database.h"
#include "colors.h"
#include <iostream>
#include <iomanip>
#include <map>
#include <vector>
#include <algorithm>
#include <string>

// Prints session analytics: story counts, chain links, avg linking speed, top 10 anchor words.
void run_analytics(sqlite3* db, int user_id) {
    std::cout << "\n" CLR_BLUE CLR_BOLD "=== SESSION ANALYTICS ===" CLR_RESET "\n";

    auto stories = story_list(db, user_id);
    if (stories.empty()) {
        std::cout << "  No sessions yet.\n";
        return;
    }

    int    total_stories  = (int)stories.size();
    int    total_words    = 0;
    int    total_time     = 0;
    double total_flesch   = 0;

    for (auto& s : stories) {
        total_words  += s.total_word_count;
        total_time   += s.total_time;
        total_flesch += s.flesch_score;
    }

    std::cout << "\n" CLR_BLUE CLR_BOLD "--- Story Stats ---" CLR_RESET "\n";
    std::cout << "  " CLR_DIM "Total stories:" CLR_RESET "       " << total_stories << "\n";
    std::cout << "  " CLR_DIM "Total words written:" CLR_RESET " " << total_words << "\n";
    std::cout << "  " CLR_DIM "Total writing time:" CLR_RESET "  " << total_time << "s\n";
    std::cout << "  " CLR_DIM "Avg Flesch score:" CLR_RESET "    "
              << std::fixed << std::setprecision(1) << (total_flesch / total_stories) << "\n";

    std::map<std::string, int> word_freq;
    std::vector<int>           all_times_ms;

    for (auto& s : stories) {
        WordChain wc;
        if (!wordchain_get_by_story(db, s.story_id, wc)) continue;
        auto cwords = chainword_list(db, wc.chain_id);
        for (auto& cw : cwords) {
            if (!cw.word_text.empty()) word_freq[cw.word_text]++;
            if (cw.time_taken_ms > 0)  all_times_ms.push_back(cw.time_taken_ms);
        }
    }

    if (!all_times_ms.empty()) {
        long long sum = 0;
        for (int t : all_times_ms) sum += t;
        double avg_ms = (double)sum / all_times_ms.size();

        std::cout << "\n" CLR_BLUE CLR_BOLD "--- Chain Reaction Stats ---" CLR_RESET "\n";
        std::cout << "  " CLR_DIM "Total links forged:" CLR_RESET "  " << all_times_ms.size() << "\n";
        std::cout << "  " CLR_DIM "Avg linking speed:" CLR_RESET "   "
                  << std::fixed << std::setprecision(0) << avg_ms << " ms\n";
    }

    if (!word_freq.empty()) {
        std::vector<std::pair<std::string, int>> sorted(word_freq.begin(), word_freq.end());
        std::sort(sorted.begin(), sorted.end(),
                  [](auto& a, auto& b){ return a.second > b.second; });

        std::cout << "\n" CLR_BLUE CLR_BOLD "--- Top 10 Anchor Words ---" CLR_RESET "\n";
        int show = std::min((int)sorted.size(), 10);
        for (int i = 0; i < show; ++i) {
            std::cout << "  " << std::setw(3) << i + 1 << ".  "
                      << std::left << std::setw(20) << sorted[i].first
                      << "x" << sorted[i].second << "\n";
        }
    }

    std::cout << "\n";
}
