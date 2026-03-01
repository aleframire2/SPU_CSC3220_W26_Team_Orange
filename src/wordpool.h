#pragma once
#include <string>
#include "sqlite3.h"

// Import words from a .txt file (one word per line).
// category: optional label for all imported words.
// Returns number of new words added.
int import_wordpool_file(sqlite3* db, const std::string& filepath, const std::string& category);

// Interactive word pool management menu.
void run_wordpool_menu(sqlite3* db);

// Seed the default built-in word list (called on first run).
void seed_default_words(sqlite3* db);
