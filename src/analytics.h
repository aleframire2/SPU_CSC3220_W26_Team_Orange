#pragma once
#include "sqlite3.h"

// Prints a session analytics report for the user.
void run_analytics(sqlite3* db, int user_id);
