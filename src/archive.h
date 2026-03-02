// archive.h -- Archive browser and CRUD interface for user stories
#pragma once
#include "sqlite3.h"

// Starts the interactive archive menu for the given user.
void run_archive(sqlite3* db, int user_id);
