-- DML for NarrativeLink Database (SQLite)
-- Sample queries covering SELECT, INSERT, UPDATE, and DELETE.

PRAGMA foreign_keys = ON;

-- ============================================================
-- SELECT: Stories with a Flesch score above 60
-- Returns the author's first name, story title, and score.
-- ============================================================
SELECT u.firstName,
       s.title,
       s.flesch_score
FROM   USER u
JOIN   STORY s ON u.user_id = s.user_id
WHERE  s.flesch_score > 60.0;


-- ============================================================
-- INSERT: Create a new story with its linked word chain
-- Uses last_insert_rowid() to chain the dependent inserts.
-- ============================================================
BEGIN TRANSACTION;

    -- 1. Create the base story record.
    INSERT INTO STORY (user_id, title, content, total_word_count, total_time, flesch_score)
    VALUES (1, 'The Silent Terminal', 'The screen blinked...', 150, 300, 75.2);

    -- 2. Create the associated word chain (1:1 with the story just inserted).
    INSERT INTO WORDCHAIN (story_id)
    VALUES (last_insert_rowid());

    -- 3. Insert the first linked word for that chain session.
    INSERT INTO CHAINWORD (chain_id, sequence_order, word_id, time_taken_ms)
    VALUES (last_insert_rowid(), 1, 42, 1200);

COMMIT;


-- ============================================================
-- UPDATE: Recalculate word count, time, and user average score
-- Wraps two related updates in a single transaction.
-- ============================================================
BEGIN TRANSACTION;

    -- 1. Increment word count and time for two specific stories.
    UPDATE STORY
    SET    total_word_count = total_word_count + 50,
           total_time       = total_time + 120
    WHERE  user_id = 1
      AND  story_id IN (101, 102);

    -- 2. Refresh the user's overall average Flesch score.
    UPDATE USER
    SET    fleschScoreAvg = 68.5
    WHERE  user_id = 1;

COMMIT;


-- ============================================================
-- DELETE: Remove a story and all its dependent records
-- ON DELETE CASCADE handles WORDCHAIN and CHAINWORD automatically,
-- but the explicit deletions below are included for clarity.
-- ============================================================
BEGIN TRANSACTION;

    -- 1. Remove chain words belonging to the story's word chain.
    DELETE FROM CHAINWORD
    WHERE  chain_id IN (
               SELECT chain_id
               FROM   WORDCHAIN
               WHERE  story_id = 5
           );

    -- 2. Remove the word chain itself.
    DELETE FROM WORDCHAIN
    WHERE  story_id = 5;

    -- 3. Remove the story record.
    DELETE FROM STORY
    WHERE  story_id = 5;

COMMIT;


-- ============================================================
-- WORDPOOL: Verify dictionary is populated (after loading cmudict)
-- ============================================================

-- Total number of words in the pool
SELECT COUNT(*) AS total_words FROM WORDPOOL;

-- Sample rows (first 10)
SELECT word_id, word_text, syllable FROM WORDPOOL LIMIT 10;

-- Look up a specific word (e.g. abacus = 3 syllables)
SELECT word_id, word_text, syllable FROM WORDPOOL WHERE word_text = 'abacus';

-- Words by syllable count (summary)
SELECT syllable, COUNT(*) AS count FROM WORDPOOL GROUP BY syllable ORDER BY syllable;
