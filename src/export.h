// export.h -- story export to .txt or .md
#pragma once
#include "sqlite3.h"

// Interactive export menu — lets user pick a story and format (.md or .txt).
void run_export_menu(sqlite3* db, int user_id);
