/*
 * Reads cmudict.dict line by line, extracts word and syllable count,
 * and outputs SQL INSERT statements for the WORDPOOL table.
 *
 * Syllable count = number of vowel phonemes (phonemes ending in 0, 1, or 2).
 *
 * Usage: ./cmudict_to_sql [cmudict.dict] [wordpool_inserts.sql]
 *        Defaults: cmudict.dict -> wordpool_inserts.sql
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define LINE_MAX  1024
#define WORD_MAX  128
#define OUT_PATH  "wordpool_inserts.sql"
#define DICT_PATH "cmudict.dict"

/* Count syllables: phonemes ending in '0', '1', or '2' are vowel sounds. */
static int count_syllables(const char *rest, size_t rest_len) {
    int count = 0;
    const char *p = rest;
    const char *end = rest + rest_len;
    while (p < end) {
        while (p < end && isspace((unsigned char)*p)) p++;
        if (p >= end) break;
        const char *start = p;
        while (p < end && !isspace((unsigned char)*p)) p++;
        if (p > start) {
            char last = *(p - 1);
            if (last == '0' || last == '1' || last == '2')
                count++;
        }
    }
    return count;
}

/* Escape single quotes for SQL: ' -> ''. Writes into out, returns length written. */
static size_t sql_escape(const char *word, char *out, size_t out_size) {
    size_t j = 0;
    for (const char *p = word; *p && j + 3 < out_size; p++) {
        if (*p == '\'') {
            out[j++] = '\'';
            out[j++] = '\'';
        } else
            out[j++] = *p;
    }
    out[j] = '\0';
    return j;
}

int main(int argc, char **argv) {
    const char *dict_path = DICT_PATH;
    const char *out_path  = OUT_PATH;
    if (argc >= 2) dict_path = argv[1];
    if (argc >= 3) out_path  = argv[2];

    FILE *fp = fopen(dict_path, "r");
    if (!fp) {
        fprintf(stderr, "Error: cannot open %s\n", dict_path);
        return 1;
    }

    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "Error: cannot open %s for writing\n", out_path);
        fclose(fp);
        return 1;
    }

    fprintf(out, "-- WORDPOOL inserts from cmudict.dict\n");
    fprintf(out, "-- Run against your SQLite database.\n\n");
    fprintf(out, "BEGIN TRANSACTION;\n");

    char line[LINE_MAX];
    char word[WORD_MAX];
    char escaped[WORD_MAX * 2]; /* enough for ' -> '' */
    int num_inserts = 0;

    while (fgets(line, sizeof line, fp)) {
        /* trim newline */
        size_t len = strlen(line);
        if (len && line[len - 1] == '\n') { line[--len] = '\0'; }

        /* strip comment */
        char *hash = strchr(line, '#');
        if (hash) { *hash = '\0'; len = (size_t)(hash - line); }

        /* first token = word */
        const char *p = line;
        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) continue;

        size_t wlen = 0;
        while (p[wlen] && !isspace((unsigned char)p[wlen]) && wlen < WORD_MAX - 1)
            wlen++;
        if (wlen == 0) continue;

        memcpy(word, p, wlen);
        word[wlen] = '\0';

        p += wlen;
        while (*p && isspace((unsigned char)*p)) p++;
        size_t rest_len = strlen(p);
        int syllables = count_syllables(p, rest_len);
        if (syllables == 0) continue;

        sql_escape(word, escaped, sizeof escaped);
        fprintf(out, "INSERT INTO WORDPOOL (word_text, syllable) VALUES ('%s', %d);\n",
                escaped, syllables);
        num_inserts++;
    }

    fprintf(out, "COMMIT;\n");
    fclose(fp);
    fclose(out);
    fprintf(stderr, "Wrote %d INSERT statements to %s\n", num_inserts, out_path);
    return 0;
}
