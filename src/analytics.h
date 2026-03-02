// analytics.h -- Session analytics report for the user
#pragma once
#include "sqlite3.h"

// Prints session analytics: story stats, chain reaction times, top anchor words.
void run_analytics(sqlite3* db, int user_id);
