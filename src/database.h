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
    std::string category;
};

// ── DB lifecycle ───────────────────────────────────────────
sqlite3* db_open(const std::string& path);
void     db_close(sqlite3* db);
void     db_init_schema(sqlite3* db);

// ── USER ──────────────────────────────────────────────────
int  user_create(sqlite3* db, const std::string& first, const std::string& last, const std::string& password);
bool user_login(sqlite3* db, const std::string& first, const std::string& last, const std::string& password, User& out);
void user_update_flesch_avg(sqlite3* db, int user_id);

// ── STORY ─────────────────────────────────────────────────
int  story_create(sqlite3* db, int user_id, const std::string& title,
                  const std::string& content, int word_count, int time_sec, double flesch);
bool story_get(sqlite3* db, int story_id, Story& out);
std::vector<Story> story_list(sqlite3* db, int user_id);
bool story_update(sqlite3* db, int story_id, const std::string& title, const std::string& content,
                  int word_count, int time_sec, double flesch);
bool story_delete(sqlite3* db, int story_id);

// ── WORDCHAIN ─────────────────────────────────────────────
int  wordchain_create(sqlite3* db, int story_id);
bool wordchain_get_by_story(sqlite3* db, int story_id, WordChain& out);

// ── CHAINWORD ─────────────────────────────────────────────
void chainword_insert(sqlite3* db, int chain_id, int seq, int word_id, int ms);
std::vector<ChainWord> chainword_list(sqlite3* db, int chain_id);

// ── WORDPOOL ──────────────────────────────────────────────
int  wordpool_insert(sqlite3* db, const std::string& word, const std::string& category);
int  wordpool_get_or_insert(sqlite3* db, const std::string& word, const std::string& category);
int  wordpool_random_id(sqlite3* db);
std::string wordpool_get_text(sqlite3* db, int word_id);
std::vector<WordPool> wordpool_all(sqlite3* db);
int  wordpool_count(sqlite3* db);
