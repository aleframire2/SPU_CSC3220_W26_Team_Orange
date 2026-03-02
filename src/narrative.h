// narrative.h -- multi-line story editor session interface
#pragma once
#include <string>
#include <vector>
#include "chain_engine.h"
#include "sqlite3.h"

// Opens a multi-line story editor in the terminal.
// Displays the word chain as reference.
// Returns when the user types a line containing only ":done".
// Saves the story to the DB and returns the new story_id.
int run_narrative_session(sqlite3* db, int user_id, const std::vector<ChainEntry>& chain);
