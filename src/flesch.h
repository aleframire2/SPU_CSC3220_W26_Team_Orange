#pragma once
#include <string>

// Counts syllables in a single word using a vowel-group heuristic.
// Unknown / short words default to 2 syllables.
int count_syllables(const std::string& word);

// Counts the number of sentences in text (split on . ! ?).
int count_sentences(const std::string& text);

// Counts total words (whitespace-delimited tokens).
int count_words(const std::string& text);

// Full Flesch Reading Ease score.
// Formula: 206.835 - 1.015*(words/sentences) - 84.6*(syllables/words)
// Returns 0 if text is empty.
double flesch_score(const std::string& text);
