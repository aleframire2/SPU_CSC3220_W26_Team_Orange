#include "database.h"
#include <iostream>
#include <stdexcept>

// ── helpers ───────────────────────────────────────────────
static void exec(sqlite3* db, const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "unknown";
        sqlite3_free(err);
        throw std::runtime_error("SQL error: " + msg);
    }
}

// ── DB lifecycle ──────────────────────────────────────────
sqlite3* db_open(const std::string& path) {
    sqlite3* db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK)
        throw std::runtime_error("Cannot open database: " + std::string(sqlite3_errmsg(db)));
    exec(db, "PRAGMA foreign_keys = ON;");
    exec(db, "PRAGMA journal_mode = WAL;");
    return db;
}

void db_close(sqlite3* db) { sqlite3_close(db); }

void db_init_schema(sqlite3* db) {
    exec(db, R"(
CREATE TABLE IF NOT EXISTS USER (
    user_id        INTEGER PRIMARY KEY AUTOINCREMENT,
    password       TEXT    NOT NULL,
    createdOn      DATETIME DEFAULT CURRENT_TIMESTAMP,
    firstName      TEXT,
    lastName       TEXT,
    fleschScoreAvg REAL DEFAULT 0
);)");
    exec(db, R"(
CREATE TABLE IF NOT EXISTS STORY (
    story_id         INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id          INTEGER NOT NULL,
    title            TEXT,
    content          TEXT,
    created_at       DATETIME DEFAULT CURRENT_TIMESTAMP,
    total_word_count INTEGER DEFAULT 0,
    total_time       INTEGER DEFAULT 0,
    flesch_score     REAL    DEFAULT 0,
    FOREIGN KEY (user_id) REFERENCES USER(user_id) ON DELETE CASCADE
);)");
    exec(db, R"(
CREATE TABLE IF NOT EXISTS WORDCHAIN (
    chain_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    story_id     INTEGER NOT NULL UNIQUE,
    session_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (story_id) REFERENCES STORY(story_id) ON DELETE CASCADE
);)");
    exec(db, R"(
CREATE TABLE IF NOT EXISTS WORDPOOL (
    word_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    word_text TEXT    UNIQUE NOT NULL,
    syllable  INTEGER DEFAULT 0
);)");
    exec(db, R"(
CREATE TABLE IF NOT EXISTS CHAINWORD (
    chain_id       INTEGER NOT NULL,
    sequence_order INTEGER NOT NULL,
    word_id        INTEGER NOT NULL,
    time_taken_ms  INTEGER,
    PRIMARY KEY (chain_id, sequence_order),
    FOREIGN KEY (chain_id) REFERENCES WORDCHAIN(chain_id) ON DELETE CASCADE,
    FOREIGN KEY (word_id)  REFERENCES WORDPOOL(word_id)   ON DELETE CASCADE
);)");
}

// ── USER ──────────────────────────────────────────────────
int user_create(sqlite3* db, const std::string& first, const std::string& last, const std::string& pw) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT INTO USER(firstName,lastName,password) VALUES(?,?,?);", -1, &s, nullptr);
    sqlite3_bind_text(s, 1, first.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, last.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, pw.c_str(),    -1, SQLITE_TRANSIENT);
    sqlite3_step(s);
    sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db);
}

bool user_login(sqlite3* db, const std::string& first, const std::string& last,
                const std::string& pw, User& out) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT user_id,firstName,lastName,password,fleschScoreAvg "
        "FROM USER WHERE firstName=? AND lastName=? AND password=? LIMIT 1;",
        -1, &s, nullptr);
    sqlite3_bind_text(s, 1, first.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 2, last.c_str(),  -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s, 3, pw.c_str(),    -1, SQLITE_TRANSIENT);
    bool found = false;
    if (sqlite3_step(s) == SQLITE_ROW) {
        out.user_id        = sqlite3_column_int(s, 0);
        out.firstName      = (const char*)sqlite3_column_text(s, 1);
        out.lastName       = (const char*)sqlite3_column_text(s, 2);
        out.password       = (const char*)sqlite3_column_text(s, 3);
        out.fleschScoreAvg = sqlite3_column_double(s, 4);
        found = true;
    }
    sqlite3_finalize(s);
    return found;
}

void user_update_flesch_avg(sqlite3* db, int user_id) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "UPDATE USER SET fleschScoreAvg = "
        "(SELECT AVG(flesch_score) FROM STORY WHERE user_id=? AND flesch_score > 0) "
        "WHERE user_id=?;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, user_id);
    sqlite3_bind_int(s, 2, user_id);
    sqlite3_step(s);
    sqlite3_finalize(s);
}

// ── STORY ─────────────────────────────────────────────────
int story_create(sqlite3* db, int user_id, const std::string& title,
                 const std::string& content, int wc, int ts, double fl) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT INTO STORY(user_id,title,content,total_word_count,total_time,flesch_score) "
        "VALUES(?,?,?,?,?,?);", -1, &s, nullptr);
    sqlite3_bind_int(s,    1, user_id);
    sqlite3_bind_text(s,   2, title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s,   3, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s,    4, wc);
    sqlite3_bind_int(s,    5, ts);
    sqlite3_bind_double(s, 6, fl);
    sqlite3_step(s);
    sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db);
}

bool story_get(sqlite3* db, int story_id, Story& out) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT story_id,user_id,title,content,created_at,total_word_count,total_time,flesch_score "
        "FROM STORY WHERE story_id=? LIMIT 1;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, story_id);
    bool found = false;
    if (sqlite3_step(s) == SQLITE_ROW) {
        out.story_id        = sqlite3_column_int(s, 0);
        out.user_id         = sqlite3_column_int(s, 1);
        out.title           = (const char*)sqlite3_column_text(s, 2);
        out.content         = sqlite3_column_text(s, 3) ? (const char*)sqlite3_column_text(s, 3) : "";
        out.created_at      = sqlite3_column_text(s, 4) ? (const char*)sqlite3_column_text(s, 4) : "";
        out.total_word_count= sqlite3_column_int(s, 5);
        out.total_time      = sqlite3_column_int(s, 6);
        out.flesch_score    = sqlite3_column_double(s, 7);
        found = true;
    }
    sqlite3_finalize(s);
    return found;
}

std::vector<Story> story_list(sqlite3* db, int user_id) {
    std::vector<Story> v;
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT story_id,user_id,title,content,created_at,total_word_count,total_time,flesch_score "
        "FROM STORY WHERE user_id=? ORDER BY created_at DESC;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, user_id);
    while (sqlite3_step(s) == SQLITE_ROW) {
        Story st;
        st.story_id         = sqlite3_column_int(s, 0);
        st.user_id          = sqlite3_column_int(s, 1);
        st.title            = sqlite3_column_text(s, 2) ? (const char*)sqlite3_column_text(s, 2) : "(untitled)";
        st.content          = sqlite3_column_text(s, 3) ? (const char*)sqlite3_column_text(s, 3) : "";
        st.created_at       = sqlite3_column_text(s, 4) ? (const char*)sqlite3_column_text(s, 4) : "";
        st.total_word_count = sqlite3_column_int(s, 5);
        st.total_time       = sqlite3_column_int(s, 6);
        st.flesch_score     = sqlite3_column_double(s, 7);
        v.push_back(st);
    }
    sqlite3_finalize(s);
    return v;
}

bool story_update(sqlite3* db, int story_id, const std::string& title,
                  const std::string& content, int wc, int ts, double fl) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "UPDATE STORY SET title=?,content=?,total_word_count=?,total_time=?,flesch_score=?,"
        "updated_at=CURRENT_TIMESTAMP WHERE story_id=?;", -1, &s, nullptr);
    // Note: updated_at column is set literally; SQLite ignores unknown column gracefully
    // but we keep the schema simple — created_at doubles as last-modified here
    sqlite3_finalize(s);

    sqlite3_prepare_v2(db,
        "UPDATE STORY SET title=?,content=?,total_word_count=?,total_time=?,flesch_score=? "
        "WHERE story_id=?;", -1, &s, nullptr);
    sqlite3_bind_text(s,   1, title.c_str(),   -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(s,   2, content.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s,    3, wc);
    sqlite3_bind_int(s,    4, ts);
    sqlite3_bind_double(s, 5, fl);
    sqlite3_bind_int(s,    6, story_id);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return rc == SQLITE_DONE;
}

bool story_delete(sqlite3* db, int story_id) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db, "DELETE FROM STORY WHERE story_id=?;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, story_id);
    int rc = sqlite3_step(s);
    sqlite3_finalize(s);
    return rc == SQLITE_DONE;
}

// ── WORDCHAIN ─────────────────────────────────────────────
int wordchain_create(sqlite3* db, int story_id) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db, "INSERT INTO WORDCHAIN(story_id) VALUES(?);", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, story_id);
    sqlite3_step(s);
    sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db);
}

bool wordchain_get_by_story(sqlite3* db, int story_id, WordChain& out) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT chain_id,story_id,session_date FROM WORDCHAIN WHERE story_id=? LIMIT 1;",
        -1, &s, nullptr);
    sqlite3_bind_int(s, 1, story_id);
    bool found = false;
    if (sqlite3_step(s) == SQLITE_ROW) {
        out.chain_id     = sqlite3_column_int(s, 0);
        out.story_id     = sqlite3_column_int(s, 1);
        out.session_date = sqlite3_column_text(s, 2) ? (const char*)sqlite3_column_text(s, 2) : "";
        found = true;
    }
    sqlite3_finalize(s);
    return found;
}

// ── CHAINWORD ─────────────────────────────────────────────
void chainword_insert(sqlite3* db, int chain_id, int seq, int word_id, int ms) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO CHAINWORD(chain_id,sequence_order,word_id,time_taken_ms) VALUES(?,?,?,?);",
        -1, &s, nullptr);
    sqlite3_bind_int(s, 1, chain_id);
    sqlite3_bind_int(s, 2, seq);
    sqlite3_bind_int(s, 3, word_id);
    sqlite3_bind_int(s, 4, ms);
    sqlite3_step(s);
    sqlite3_finalize(s);
}

std::vector<ChainWord> chainword_list(sqlite3* db, int chain_id) {
    std::vector<ChainWord> v;
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT cw.chain_id, cw.sequence_order, cw.word_id, cw.time_taken_ms, wp.word_text "
        "FROM CHAINWORD cw JOIN WORDPOOL wp ON cw.word_id=wp.word_id "
        "WHERE cw.chain_id=? ORDER BY cw.sequence_order;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, chain_id);
    while (sqlite3_step(s) == SQLITE_ROW) {
        ChainWord cw;
        cw.chain_id       = sqlite3_column_int(s, 0);
        cw.sequence_order = sqlite3_column_int(s, 1);
        cw.word_id        = sqlite3_column_int(s, 2);
        cw.time_taken_ms  = sqlite3_column_int(s, 3);
        cw.word_text      = sqlite3_column_text(s, 4) ? (const char*)sqlite3_column_text(s, 4) : "";
        v.push_back(cw);
    }
    sqlite3_finalize(s);
    return v;
}

// ── WORDPOOL ──────────────────────────────────────────────
int wordpool_insert(sqlite3* db, const std::string& word, int syllable) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "INSERT OR IGNORE INTO WORDPOOL(word_text,syllable) VALUES(?,?);", -1, &s, nullptr);
    sqlite3_bind_text(s, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(s,  2, syllable);
    sqlite3_step(s);
    sqlite3_finalize(s);
    return (int)sqlite3_last_insert_rowid(db);
}

int wordpool_get_or_insert(sqlite3* db, const std::string& word, int syllable) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT word_id FROM WORDPOOL WHERE word_text=? LIMIT 1;", -1, &s, nullptr);
    sqlite3_bind_text(s, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    int id = -1;
    if (sqlite3_step(s) == SQLITE_ROW) id = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    if (id != -1) return id;
    return wordpool_insert(db, word, syllable);
}

int wordpool_random_id(sqlite3* db) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT word_id FROM WORDPOOL ORDER BY RANDOM() LIMIT 1;", -1, &s, nullptr);
    int id = -1;
    if (sqlite3_step(s) == SQLITE_ROW) id = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return id;
}

std::string wordpool_get_text(sqlite3* db, int word_id) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT word_text FROM WORDPOOL WHERE word_id=? LIMIT 1;", -1, &s, nullptr);
    sqlite3_bind_int(s, 1, word_id);
    std::string txt;
    if (sqlite3_step(s) == SQLITE_ROW && sqlite3_column_text(s, 0))
        txt = (const char*)sqlite3_column_text(s, 0);
    sqlite3_finalize(s);
    return txt;
}

int wordpool_get_syllable(sqlite3* db, const std::string& word) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db,
        "SELECT syllable FROM WORDPOOL WHERE LOWER(word_text) = LOWER(?) LIMIT 1;",
        -1, &s, nullptr);
    sqlite3_bind_text(s, 1, word.c_str(), -1, SQLITE_TRANSIENT);
    int syl = 0;
    if (sqlite3_step(s) == SQLITE_ROW) syl = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return syl;
}

std::vector<WordPool> wordpool_all(sqlite3* db) {
    std::vector<WordPool> v;
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db, "SELECT word_id,word_text,syllable FROM WORDPOOL ORDER BY word_text;",
        -1, &s, nullptr);
    while (sqlite3_step(s) == SQLITE_ROW) {
        WordPool w;
        w.word_id   = sqlite3_column_int(s, 0);
        w.word_text = (const char*)sqlite3_column_text(s, 1);
        w.syllable  = sqlite3_column_int(s, 2);
        v.push_back(w);
    }
    sqlite3_finalize(s);
    return v;
}

int wordpool_count(sqlite3* db) {
    sqlite3_stmt* s;
    sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM WORDPOOL;", -1, &s, nullptr);
    int c = 0;
    if (sqlite3_step(s) == SQLITE_ROW) c = sqlite3_column_int(s, 0);
    sqlite3_finalize(s);
    return c;
}
