-- DDL for NarrativeLink Database (SQLite)
-- Schema v2.2 — matches Final ERD (Crow's Foot Notation)

PRAGMA foreign_keys = ON;

-- ============================================================
-- TABLE: USER
-- ============================================================
CREATE TABLE IF NOT EXISTS USER (
    user_id        INTEGER  PRIMARY KEY AUTOINCREMENT,
    password       TEXT     NOT NULL,
    createdOn      DATETIME DEFAULT CURRENT_TIMESTAMP,
    firstName      TEXT,
    lastName       TEXT,
    fleschScoreAvg REAL
);

-- ============================================================
-- TABLE: STORY
-- Belongs to one USER (many-to-one).
-- ============================================================
CREATE TABLE IF NOT EXISTS STORY (
    story_id        INTEGER  PRIMARY KEY AUTOINCREMENT,
    user_id         INTEGER  NOT NULL,
    title           TEXT,
    content         TEXT,
    created_at      DATETIME DEFAULT CURRENT_TIMESTAMP,
    total_word_count INTEGER,
    total_time      INTEGER,
    flesch_score    REAL,
    FOREIGN KEY (user_id) REFERENCES USER(user_id) ON DELETE CASCADE
);

-- ============================================================
-- TABLE: WORDCHAIN
-- One-to-one with STORY (UNIQUE enforces the 1:1 relationship).
-- ============================================================
CREATE TABLE IF NOT EXISTS WORDCHAIN (
    chain_id     INTEGER  PRIMARY KEY AUTOINCREMENT,
    story_id     INTEGER  NOT NULL UNIQUE,
    session_date DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (story_id) REFERENCES STORY(story_id) ON DELETE CASCADE
);

-- ============================================================
-- TABLE: WORDPOOL
-- Local dictionary of valid words.
-- ============================================================
CREATE TABLE IF NOT EXISTS WORDPOOL (
    word_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    word_text TEXT    UNIQUE NOT NULL,
    syllable  INTEGER
);

-- ============================================================
-- TABLE: CHAINWORD
-- Composite PK: (chain_id, sequence_order) ensures no two words
-- share the same position in a single game session.
-- ============================================================
CREATE TABLE IF NOT EXISTS CHAINWORD (
    chain_id       INTEGER NOT NULL,
    sequence_order INTEGER NOT NULL,
    word_id        INTEGER NOT NULL,
    time_taken_ms  INTEGER,
    PRIMARY KEY (chain_id, sequence_order),
    FOREIGN KEY (chain_id) REFERENCES WORDCHAIN(chain_id) ON DELETE CASCADE,
    FOREIGN KEY (word_id)  REFERENCES WORDPOOL(word_id)   ON DELETE CASCADE
);
