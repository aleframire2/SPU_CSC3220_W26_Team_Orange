#pragma once
#include "sqlite3.h"

// Interactive archive browser and CRUD menu for the current user.
void run_archive(sqlite3* db, int user_id);
