// database.h -- SQLite schema and CRUD for users, stories, word chains, word pool
#pragma once
#include <string>
#include <vector>
#include "sqlite3.h"

// ── Structs ────────────────────────────────────────────────
struct User {
    int         user_id;
    std::string firstName;
    std::string lastName;
    std::string password;
    double      fleschScoreAvg;
};

struct Story {
    int         story_id;
    int         user_id;
    std::string title;
    std::string content;
    std::string created_at;
    int         total_word_count;
    int         total_time;
    double      flesch_score;
};

struct WordChain {
    int         chain_id;
    int         story_id;
    std::string session_date;
};

struct ChainWord {
    int         chain_id;
    int         sequence_order;
    int         word_id;
    int         time_taken_ms;
    std::string word_text;   // joined from WORDPOOL — convenience
};

struct WordPool {
    int         word_id;
    std::string word_text;
    int         syllable;
};

// ── DB lifecycle ───────────────────────────────────────────
// Opens database at path, enables WAL and foreign keys
sqlite3* db_open(const std::string& path);
void     db_close(sqlite3* db);
// Creates USER, STORY, WORDCHAIN, WORDPOOL, CHAINWORD tables if missing
void     db_init_schema(sqlite3* db);

// ── USER ──────────────────────────────────────────────────
// Inserts user, returns user_id
int  user_create(sqlite3* db, const std::string& first, const std::string& last, const std::string& password);
// Authenticates by first+last+password, fills User on success
bool user_login(sqlite3* db, const std::string& first, const std::string& last, const std::string& password, User& out);
// Recomputes user's fleschScoreAvg from their stories (excludes 0 scores)
void user_update_flesch_avg(sqlite3* db, int user_id);

// ── STORY ─────────────────────────────────────────────────
// Inserts story, returns story_id
int  story_create(sqlite3* db, int user_id, const std::string& title,
                  const std::string& content, int word_count, int time_sec, double flesch);
// Fetches story by id
bool story_get(sqlite3* db, int story_id, Story& out);
// Lists user's stories, newest first
std::vector<Story> story_list(sqlite3* db, int user_id);
bool story_update(sqlite3* db, int story_id, const std::string& title, const std::string& content,
                  int word_count, int time_sec, double flesch);
// Deletes story (cascades to WORDCHAIN, CHAINWORD)
bool story_delete(sqlite3* db, int story_id);

// ── WORDCHAIN ─────────────────────────────────────────────
// One chain per story (story_id UNIQUE); returns chain_id
int  wordchain_create(sqlite3* db, int story_id);
bool wordchain_get_by_story(sqlite3* db, int story_id, WordChain& out);

// ── CHAINWORD ─────────────────────────────────────────────
// Composite key (chain_id, sequence_order); INSERT OR IGNORE on conflict
void chainword_insert(sqlite3* db, int chain_id, int seq, int word_id, int ms);
// Returns chain words with word_text joined from WORDPOOL
std::vector<ChainWord> chainword_list(sqlite3* db, int chain_id);

// ── WORDPOOL ──────────────────────────────────────────────
// Inserts word (OR IGNORE if exists), returns word_id
int  wordpool_insert(sqlite3* db, const std::string& word, int syllable);
// Returns existing word_id or inserts and returns new id
int  wordpool_get_or_insert(sqlite3* db, const std::string& word, int syllable);
// Returns random word_id, or -1 if pool empty
int  wordpool_random_id(sqlite3* db);
std::string wordpool_get_text(sqlite3* db, int word_id);
// Case-insensitive lookup
int  wordpool_get_syllable(sqlite3* db, const std::string& word);
std::vector<WordPool> wordpool_all(sqlite3* db);
int  wordpool_count(sqlite3* db);
