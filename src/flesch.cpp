// flesch.cpp -- Flesch score and syllable counting implementation
#include "flesch.h"
#include "database.h"
#include <cctype>
#include <sstream>
#include <algorithm>

// ── syllable heuristic ────────────────────────────────────
// Rules:
//  1. Count vowel groups (consecutive vowels = 1 syllable).
//  2. Subtract 1 if word ends in silent 'e'.
//  3. Ensure minimum of 1 syllable.
//  4. If the word is not in any recognisable pattern (result still 0), return 2.
int count_syllables(const std::string& raw) {
    if (raw.empty()) return 2;

    std::string w;
    for (char c : raw)
        if (std::isalpha((unsigned char)c)) w += std::tolower((unsigned char)c);

    if (w.empty()) return 2;

    // Very short words: treat as 1 syllable
    if (w.size() <= 2) return 1;

    const std::string vowels = "aeiouy";
    int count = 0;
    bool prev_vowel = false;

    for (char c : w) {
        bool is_vowel = (vowels.find(c) != std::string::npos);
        if (is_vowel && !prev_vowel) count++;
        prev_vowel = is_vowel;
    }

    // Silent trailing 'e'
    if (w.size() > 2 && w.back() == 'e' && vowels.find(w[w.size() - 2]) == std::string::npos)
        count--;

    if (count <= 0) return 2;   // unknown pattern → default 2
    return count;
}

// DB lookup with heuristic fallback when db is null or word not in pool
int count_syllables_db(sqlite3* db, const std::string& word) {
    if (db) {
        int syl = wordpool_get_syllable(db, word);
        if (syl > 0) return syl;
    }
    return count_syllables(word);
}

// Splits on . ! ?; returns 1 if none (avoids div-by-zero in Flesch)
int count_sentences(const std::string& text) {
    int n = 0;
    for (char c : text)
        if (c == '.' || c == '!' || c == '?') n++;
    return n > 0 ? n : 1;
}

// Whitespace-delimited token count
int count_words(const std::string& text) {
    std::istringstream ss(text);
    std::string tok;
    int n = 0;
    while (ss >> tok) n++;
    return n;
}

// 206.835 - 1.015*ASL - 84.6*ASW; clamped to 0–100
double flesch_score(const std::string& text, sqlite3* db) {
    int words = count_words(text);
    if (words == 0) return 0.0;

    int sentences = count_sentences(text);
    int syllables = 0;

    std::istringstream ss(text);
    std::string tok;
    while (ss >> tok) syllables += count_syllables_db(db, tok);

    double asl = (double)words / sentences;        // avg sentence length
    double asw = (double)syllables / words;        // avg syllables per word

    double score = 206.835 - 1.015 * asl - 84.6 * asw;

    // Clamp to 0–100 (raw formula can exceed range)
    if (score < 0)   score = 0;
    if (score > 100) score = 100;
    return score;
}
