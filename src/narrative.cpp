// narrative.cpp -- multi-line story editor session, saves to DB with Flesch stats
#include "narrative.h"
#include "database.h"
#include "flesch.h"
#include "colors.h"
#include <iostream>
#include <string>
#include <chrono>

// Opens multi-line editor, accepts :done, :title, saves story + chain to DB, returns story_id.
int run_narrative_session(sqlite3* db, int user_id, const std::vector<ChainEntry>& chain) {
    std::cout << "\n" CLR_BLUE CLR_BOLD "=== NARRATIVE SYNTHESIS ===" CLR_RESET "\n";
    std::cout << "  Write your story incorporating the words below.\n";
    std::cout << "  Type  :done  on its own line when finished.\n";
    std::cout << "  Type  :title <text>  to set the title.\n\n";

    std::cout << "  Word chain: ";
    for (size_t i = 0; i < chain.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        std::cout << chain[i].word;
    }
    std::cout << "\n\n";

    std::string title = "Untitled";
    std::string content;
    auto start = std::chrono::steady_clock::now();

    std::string line;
    while (true) {
        std::cout << CLR_CYAN "> " CLR_RESET;
        if (!std::getline(std::cin, line)) break;

        if (line == ":done") break;

        if (line.rfind(":title ", 0) == 0) {  // starts-with check
            title = line.substr(7);
            std::cout << "  " CLR_DIM "[Title set to: " << title << "]" CLR_RESET "\n";
            continue;
        }

        if (!content.empty()) content += "\n";
        content += line;
    }

    auto end = std::chrono::steady_clock::now();
    int elapsed_sec = (int)std::chrono::duration_cast<std::chrono::seconds>(end - start).count();

    if (content.empty()) {
        std::cout << "  " CLR_RED "[No content entered -- session discarded.]" CLR_RESET "\n";
        return -1;
    }

    int   wc    = count_words(content);
    double fl   = flesch_score(content, db);

    std::cout << "\n" CLR_BLUE CLR_BOLD "--- Session Stats ---" CLR_RESET "\n";
    std::cout << "  " CLR_DIM "Words:" CLR_RESET "        " << wc << "\n";
    std::cout << "  " CLR_DIM "Time:" CLR_RESET "         " << elapsed_sec << "s\n";
    std::cout << "  " CLR_DIM "Flesch score:" CLR_RESET " " << fl << "\n";

    int story_id = story_create(db, user_id, title, content, wc, elapsed_sec, fl);

    int chain_time_ms = 0;
    for (auto& e : chain) chain_time_ms += e.time_ms;

    int chain_id = wordchain_create(db, story_id);

    int seq = 1;
    for (auto& e : chain) {
        chainword_insert(db, chain_id, seq, e.word_id, e.time_ms);
        seq++;
    }

    user_update_flesch_avg(db, user_id);

    std::cout << "  " CLR_GREEN "Story saved! (ID " << story_id << ")" CLR_RESET "\n";
    return story_id;
}
