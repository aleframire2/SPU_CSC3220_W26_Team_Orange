#include <stdio.h>
#include "sqlite3.h"

static int list_tables_callback(void *data, int cols, char **values, char **names) {
    (void)data;
    (void)names;
    if (cols > 0 && values[0]) printf("  - %s\n", values[0]);
    return 0;
}

int main() {
    sqlite3 *db;
    char *error_message = 0;
    int exit_code;

    //creates 'narrativelink.db' as a file
    exit_code = sqlite3_open("narrativelink.db", &db);
    
    if (exit_code) {
        printf("Error opening DB: %s\n", sqlite3_errmsg(db));
        return 1;
    } else {
        printf("Database opened successfully!\n");
    }

    // 2. DDL formatted as a giant C string
    const char *sql_create_tables = 
        "CREATE TABLE IF NOT EXISTS USER ("
        "    user_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    password TEXT NOT NULL,"
        "    createdOn DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    firstName TEXT,"
        "    lastName TEXT,"
        "    fleschScoreAvg REAL"
        ");"
        "CREATE TABLE IF NOT EXISTS STORY ("
        "    story_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    user_id INTEGER NOT NULL,"
        "    title TEXT,"
        "    content TEXT,"
        "    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    total_word_count INTEGER,"
        "    total_time INTEGER,"
        "    flesch_score REAL,"
        "    FOREIGN KEY (user_id) REFERENCES USER(user_id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS WORDCHAIN ("
        "    chain_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    story_id INTEGER NOT NULL,"
        "    session_date DATETIME DEFAULT CURRENT_TIMESTAMP,"
        "    FOREIGN KEY (story_id) REFERENCES STORY(story_id) ON DELETE CASCADE"
        ");"
        "CREATE TABLE IF NOT EXISTS WORDPOOL ("
        "    word_id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    word_text TEXT UNIQUE NOT NULL,"
        "    syllable INTEGER"
        ");"
        "CREATE TABLE IF NOT EXISTS CHAINWORD ("
        "    chain_id INTEGER NOT NULL,"
        "    sequence_order INTEGER NOT NULL,"
        "    word_id INTEGER NOT NULL,"
        "    time_taken_ms INTEGER,"
        "    PRIMARY KEY (chain_id, sequence_order),"
        "    FOREIGN KEY (chain_id) REFERENCES WORDCHAIN(chain_id) ON DELETE CASCADE,"
        "    FOREIGN KEY (word_id) REFERENCES WORDPOOL(word_id) ON DELETE CASCADE"
        ");";

    // executes the sql
    exit_code = sqlite3_exec(db, sql_create_tables, 0, 0, &error_message);

    if (exit_code != SQLITE_OK) {
        printf("SQL Error: %s\n", error_message);
        sqlite3_free(error_message);
    } else {
        printf("NarrativeLink tables created successfully!\n");
    }

    // list tables
    printf("Tables in database:\n");
    sqlite3_exec(db, "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%';",
                 list_tables_callback, 0, &error_message);

    // close
    sqlite3_close(db);

    return 0;
}