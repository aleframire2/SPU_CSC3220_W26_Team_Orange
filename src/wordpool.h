#pragma once
#include <string>
#include "sqlite3.h"

// Import words from a .txt file (one word per line).
// Returns number of new words added.
int import_wordpool_file(sqlite3* db, const std::string& filepath);

// Interactive word pool management menu.
void run_wordpool_menu(sqlite3* db);
