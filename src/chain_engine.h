// chain_engine.h -- word-association chain game engine interface
#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"

struct ChainEntry {
    std::string anchor_text;
    std::string word;
    int         word_id;
    int         time_ms;
};

// Returns the completed word chain.
// timer_sec: seconds the user has per word.
// min_chain: minimum links required before they can choose to stop.
std::vector<ChainEntry> run_chain_reaction(sqlite3* db, int timer_sec, int min_chain);
