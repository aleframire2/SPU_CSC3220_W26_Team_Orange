/*
 * NarrativeLink CLI - Local single-user app.
 * Implements: Chain Reaction, Narrative Synthesis, Flesch score,
 * Archive browser, Draft CRUD, Session analytics.
 * Compile: gcc -std=c11 -O2 -o narrativelink narrativelink.c sqlite3.c -lpthread -ldl
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "sqlite3.h"

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#define strcasecmp _stricmp
#else
#include <strings.h>
#include <sys/time.h>
#include <sys/select.h>
#include <unistd.h>
#endif

/* ANSI colors (most terminals and WSL support these) */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_DIM     "\033[2m"
#define CLR_RED     "\033[31m"
#define CLR_GREEN   "\033[32m"
#define CLR_YELLOW  "\033[33m"
#define CLR_BLUE    "\033[34m"
#define CLR_MAGENTA "\033[35m"
#define CLR_CYAN    "\033[36m"

#define DB_PATH "database.db"
#define DEFAULT_TIMER_SEC 10
#define LINE_MAX 1024
#define WORD_MAX 64

static sqlite3 *db;

static void trim_newline(char *s) {
    size_t n = strlen(s);
    if (n && s[n - 1] == '\n') s[n - 1] = '\0';
}

static int ensure_default_user(void) {
    char *err = 0;
    const char *sql = "INSERT OR IGNORE INTO USER (user_id, password) VALUES (1, 'local');";
    int r = sqlite3_exec(db, sql, 0, 0, &err);
    if (r != SQLITE_OK) { if (err) { fprintf(stderr, "%s\n", err); sqlite3_free(err); } return -1; }
    return 0;
}

static int ensure_anchor_column(void) {
    char *err = 0;
    int r = sqlite3_exec(db, "ALTER TABLE CHAINWORD ADD COLUMN anchor_word_id INTEGER;", 0, 0, &err);
    if (r != SQLITE_OK && strstr(sqlite3_errmsg(db), "duplicate") == 0) {
        if (err) { fprintf(stderr, "%s\n", err); sqlite3_free(err); }
        return -1;
    }
    if (err) sqlite3_free(err);
    return 0;
}

/* Get a random word from WORDPOOL. Returns word_id (0 on error), writes word_text into buf. */
static int get_random_word(char *buf, size_t buf_size) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT word_id, word_text FROM WORDPOOL ORDER BY RANDOM() LIMIT 1;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    int word_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        word_id = sqlite3_column_int(stmt, 0);
        const unsigned char *t = sqlite3_column_text(stmt, 1);
        if (t) snprintf(buf, buf_size, "%s", (const char *)t);
    }
    sqlite3_finalize(stmt);
    return word_id;
}

/* Look up word (case-insensitive). Returns word_id or 0, optional syllable in *syl. */
static int lookup_word(const char *word, int *syl) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT word_id, syllable FROM WORDPOOL WHERE LOWER(word_text) = LOWER(?1);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return 0;
    sqlite3_bind_text(stmt, 1, word, -1, SQLITE_TRANSIENT);
    int word_id = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        word_id = sqlite3_column_int(stmt, 0);
        if (syl) *syl = sqlite3_column_int(stmt, 1);
    }
    sqlite3_finalize(stmt);
    return word_id;
}

/* Heuristic syllable count when word not in WORDPOOL (vowel groups). */
static int heuristic_syllables(const char *word) {
    int n = 0;
    int in_vowel = 0;
    for (const char *p = word; *p; p++) {
        int v = (*p == 'a' || *p == 'e' || *p == 'i' || *p == 'o' || *p == 'u' ||
                 *p == 'A' || *p == 'E' || *p == 'I' || *p == 'O' || *p == 'U');
        if (v && !in_vowel) n++;
        in_vowel = v;
    }
    return n > 0 ? n : 1;
}

/* --- 1–2: Chain Reaction + Success/Failure --- */
struct link { int anchor_word_id; int word_id; int time_ms; };
static struct link *chain = 0;
static int chain_len = 0;
static int chain_cap = 0;

static void chain_push(int anchor_id, int word_id, int time_ms) {
    if (chain_len >= chain_cap) {
        chain_cap = chain_cap ? chain_cap * 2 : 32;
        chain = realloc(chain, (size_t)chain_cap * sizeof(struct link));
    }
    chain[chain_len].anchor_word_id = anchor_id;
    chain[chain_len].word_id = word_id;
    chain[chain_len].time_ms = time_ms;
    chain_len++;
}

static int get_time_ms(void) {
#if defined(_WIN32) || defined(_WIN64)
    return (int)(GetTickCount());
#else
    struct timeval tv;
    gettimeofday(&tv, 0);
    return (int)((long)tv.tv_sec * 1000 + tv.tv_usec / 1000);
#endif
}

static void play_chain_reaction(int timer_sec) {
    chain_len = 0;
    char prompt[WORD_MAX];
    char input[LINE_MAX];
    int anchor_id = get_random_word(prompt, sizeof(prompt));
    if (!anchor_id) {
        printf(CLR_RED "No words in WORDPOOL. Load cmudict first." CLR_RESET "\n");
        return;
    }
    printf("\n" CLR_CYAN CLR_BOLD "--- Chain Reaction ---" CLR_RESET " " CLR_DIM "(timer: %d sec)" CLR_RESET "\n", timer_sec);
    printf(CLR_DIM "Type a word and press Enter. Type 'quit' to end early." CLR_RESET "\n\n");

    while (1) {
        printf(CLR_BOLD "Word: " CLR_RESET CLR_CYAN "%s" CLR_RESET "\n", prompt);
#if !defined(_WIN32) && !defined(_WIN64)
        printf("  " CLR_DIM "Time left:" CLR_RESET " %d s\n  ", timer_sec);
#else
        printf("  " CLR_DIM "Time limit: %d s" CLR_RESET " — type your word and press Enter:\n  ", timer_sec);
#endif
        fflush(stdout);
        int start_ms = get_time_ms();
        int timed_out = 0;
#if !defined(_WIN32) && !defined(_WIN64)
        for (;;) {
            int elapsed = get_time_ms() - start_ms;
            int remaining_ms = timer_sec * 1000 - elapsed;
            int remaining_sec = (remaining_ms + 999) / 1000;
            if (remaining_ms <= 0) {
                printf("\033[1A\r" CLR_RED CLR_BOLD "Time's up! " CLR_RESET "                    \n");
                timed_out = 1;
                break;
            }
            const char *tcolor = remaining_sec <= 2 ? CLR_RED : (remaining_sec <= 5 ? CLR_YELLOW : CLR_GREEN);
            /* Save cursor (on input line where user types), update countdown line, restore cursor */
            printf("\033[s\033[1A\r  " CLR_DIM "Time left:" CLR_RESET " %s%2d s" CLR_RESET "   \033[u", tcolor, remaining_sec);
            fflush(stdout);
            fd_set fds;
            struct timeval tv;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds);
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            if (select(STDIN_FILENO + 1, &fds, 0, 0, &tv) > 0 && FD_ISSET(STDIN_FILENO, &fds))
                break;
        }
        if (timed_out) {
            fd_set f;
            struct timeval z = {0, 0};
            char c;
            for (;;) {
                FD_ZERO(&f); FD_SET(STDIN_FILENO, &f);
                if (select(STDIN_FILENO + 1, &f, 0, 0, &z) <= 0 || !FD_ISSET(STDIN_FILENO, &f)) break;
                if (read(STDIN_FILENO, &c, 1) != 1) break;
            }
            printf(CLR_RED "Session ended." CLR_RESET "\n");
            break;
        }
#endif
        if (!fgets(input, sizeof(input), stdin)) break;
        trim_newline(input);
        int end_ms = get_time_ms();
        int elapsed_ms = end_ms - start_ms;
        if (strcasecmp(input, "quit") == 0) {
            printf(CLR_YELLOW "Session ended (quit)." CLR_RESET "\n");
            break;
        }
        if (elapsed_ms > timer_sec * 1000) {
            printf(CLR_RED "Too slow! Session ended." CLR_RESET "\n");
            break;
        }

        int word_id = lookup_word(input, 0);
        if (!word_id) {
            printf(CLR_YELLOW "Word not in dictionary. Try again." CLR_RESET "\n");
            continue;
        }
        printf(CLR_GREEN "Link forged!" CLR_RESET " " CLR_DIM "(%d ms)" CLR_RESET "\n", elapsed_ms);
        chain_push(anchor_id, word_id, elapsed_ms);

        anchor_id = get_random_word(prompt, sizeof(prompt));
        if (!anchor_id) break;
    }

    if (chain_len == 0) {
        printf(CLR_DIM "No links. No story this time." CLR_RESET "\n");
        return;
    }
    printf("\n" CLR_GREEN "Chain length: %d." CLR_RESET " Proceed to write story.\n", chain_len);
}

/* --- 3–4: Narrative Synthesis + Flesch --- */
static int count_sentences(const char *text) {
    int n = 0;
    for (const char *p = text; *p; p++)
        if (*p == '.' || *p == '!' || *p == '?') n++;
    return n > 0 ? n : 1;
}

static int count_words_and_syllables(const char *text, int *total_syllables) {
    int words = 0;
    *total_syllables = 0;
    char buf[WORD_MAX];
    const char *p = text;
    while (*p) {
        while (*p && !isalnum((unsigned char)*p)) p++;
        if (!*p) break;
        size_t i = 0;
        while (*p && isalnum((unsigned char)*p) && i < sizeof(buf) - 1) buf[i++] = (char)*p++;
        buf[i] = '\0';
        if (i) {
            words++;
            int syl;
            if (lookup_word(buf, &syl)) *total_syllables += syl;
            else *total_syllables += heuristic_syllables(buf);
        }
    }
    return words;
}

static double flesch_score(int words, int sentences, int syllables) {
    if (words <= 0) return 0.0;
    return 206.835
           - 1.015 * ((double)words / (double)(sentences > 0 ? sentences : 1))
           - 84.6 * ((double)syllables / (double)words);
}

static void write_story_and_save(void) {
    if (chain_len == 0) {
        printf("No chain. Play Chain Reaction first.\n");
        return;
    }

    char *err = 0;
    sqlite3_exec(db, "BEGIN TRANSACTION;", 0, 0, &err);

    /* Create STORY placeholder (user_id 1, no content yet) */
    const char *ins_story = "INSERT INTO STORY (user_id, title, content) VALUES (1, 'Untitled', '');";
    if (sqlite3_exec(db, ins_story, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "Story insert: %s\n", err); sqlite3_free(err); sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return;
    }
    sqlite3_int64 story_id = sqlite3_last_insert_rowid(db);

    /* Create WORDCHAIN */
    char sql[256];
    snprintf(sql, sizeof(sql), "INSERT INTO WORDCHAIN (story_id) VALUES (%lld);", (long long)story_id);
    if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) {
        fprintf(stderr, "Wordchain: %s\n", err); sqlite3_free(err); sqlite3_exec(db, "ROLLBACK;", 0, 0, 0);
        return;
    }
    sqlite3_int64 chain_id = sqlite3_last_insert_rowid(db);

    /* Insert CHAINWORD rows */
    sqlite3_stmt *stmt;
    const char *ins_cw = "INSERT INTO CHAINWORD (chain_id, sequence_order, word_id, anchor_word_id, time_taken_ms) VALUES (?1, ?2, ?3, ?4, ?5);";
    if (sqlite3_prepare_v2(db, ins_cw, -1, &stmt, 0) != SQLITE_OK) {
        sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); return;
    }
    for (int i = 0; i < chain_len; i++) {
        sqlite3_bind_int64(stmt, 1, chain_id);
        sqlite3_bind_int(stmt, 2, i + 1);
        sqlite3_bind_int(stmt, 3, chain[i].word_id);
        sqlite3_bind_int(stmt, 4, chain[i].anchor_word_id);
        sqlite3_bind_int(stmt, 5, chain[i].time_ms);
        if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); sqlite3_exec(db, "ROLLBACK;", 0, 0, 0); return; }
        sqlite3_reset(stmt);
    }
    sqlite3_finalize(stmt);

    sqlite3_exec(db, "COMMIT;", 0, 0, 0);

    printf("\n" CLR_CYAN CLR_BOLD "--- Write your story ---" CLR_RESET " " CLR_DIM "(use the chain words; end with a line 'END')" CLR_RESET "\n");
    size_t content_cap = 8192;
    size_t content_len = 0;
    char *content = malloc(content_cap);
    if (!content) return;
    content[0] = '\0';

    while (1) {
        char line[LINE_MAX];
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);
        if (strcmp(line, "END") == 0) break;
        size_t need = content_len + strlen(line) + 2;
        if (need >= content_cap) {
            content_cap *= 2;
            char *n = realloc(content, content_cap);
            if (!n) break;
            content = n;
        }
        strcat(content, line);
        strcat(content, "\n");
        content_len = strlen(content);
    }

    int sentences = count_sentences(content);
    int total_syllables;
    int words = count_words_and_syllables(content, &total_syllables);
    double score = flesch_score(words, sentences, total_syllables);
    int total_time = 0;
    for (int i = 0; i < chain_len; i++) total_time += chain[i].time_ms;

    snprintf(sql, sizeof(sql),
             "UPDATE STORY SET content = ?1, total_word_count = %d, total_time = %d, flesch_score = %f WHERE story_id = %lld;",
             words, total_time, score, (long long)story_id);
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(content);

    printf(CLR_GREEN "Story saved." CLR_RESET " Flesch Reading Ease: " CLR_BOLD "%.1f" CLR_RESET "\n", score);
}

/* --- 5: Archive --- */
static void archive_list(void) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT story_id, COALESCE(title,'(untitled)'), created_at, total_word_count, total_time, flesch_score FROM STORY ORDER BY created_at DESC LIMIT 50;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return;
    printf("\n" CLR_CYAN CLR_BOLD "--- Recent stories ---" CLR_RESET "\n");
    printf(CLR_DIM "%-6s %-24s %-22s %6s %8s %6s" CLR_RESET "\n", "ID", "Title", "Date", "Words", "Time(ms)", "Flesch");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        printf("%-6d %-24s %-22s %6d %8d %6.1f\n",
               sqlite3_column_int(stmt, 0),
               sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "",
               sqlite3_column_text(stmt, 2) ? (const char *)sqlite3_column_text(stmt, 2) : "",
               sqlite3_column_int(stmt, 3),
               sqlite3_column_int(stmt, 4),
               sqlite3_column_double(stmt, 5));
    }
    sqlite3_finalize(stmt);
}

static void archive_view(int story_id) {
    sqlite3_stmt *stmt;
    char sql[256];
    snprintf(sql, sizeof(sql), "SELECT title, content, total_word_count, total_time, flesch_score FROM STORY WHERE story_id = %d;", story_id);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return;
    if (sqlite3_step(stmt) != SQLITE_ROW) {
        printf("Story not found.\n");
        sqlite3_finalize(stmt);
        return;
    }
    printf("\n" CLR_CYAN CLR_BOLD "--- %s ---" CLR_RESET "\n", sqlite3_column_text(stmt, 0) ? (const char *)sqlite3_column_text(stmt, 0) : "(untitled)");
    printf("Words: %d  Time: %d ms  Flesch: %.1f\n", sqlite3_column_int(stmt, 2), sqlite3_column_int(stmt, 3), sqlite3_column_double(stmt, 4));
    printf("Content:\n%s\n", sqlite3_column_text(stmt, 1) ? (const char *)sqlite3_column_text(stmt, 1) : "");
    sqlite3_finalize(stmt);

    snprintf(sql, sizeof(sql), "SELECT c.sequence_order, w.word_text FROM CHAINWORD c JOIN WORDPOOL w ON c.word_id = w.word_id WHERE c.chain_id = (SELECT chain_id FROM WORDCHAIN WHERE story_id = %d) ORDER BY c.sequence_order;", story_id);
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return;
    printf(CLR_DIM "Word chain: " CLR_RESET);
    int first = 1;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (!first) printf(" -> ");
        printf("%s", (const char *)sqlite3_column_text(stmt, 1));
        first = 0;
    }
    printf("\n");
    sqlite3_finalize(stmt);
}

/* --- 6: Draft CRUD --- */
static void crud_update_content(int story_id) {
    printf("Paste new content; end with a line 'END'\n");
    size_t cap = 8192;
    char *content = malloc(cap);
    if (!content) return;
    content[0] = '\0';
    while (1) {
        char line[LINE_MAX];
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);
        if (strcmp(line, "END") == 0) break;
        if (strlen(content) + strlen(line) + 2 >= cap) { cap *= 2; char *n = realloc(content, cap); if (!n) break; content = n; }
        strcat(content, line);
        strcat(content, "\n");
    }
    int syl;
    int words = count_words_and_syllables(content, &syl);
    int sent = count_sentences(content);
    double score = flesch_score(words, sent, syl);

    sqlite3_stmt *stmt;
    const char *sql = "UPDATE STORY SET content = ?1, total_word_count = ?2, flesch_score = ?3 WHERE story_id = ?4;";
    sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, content, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, words);
    sqlite3_bind_double(stmt, 3, score);
    sqlite3_bind_int(stmt, 4, story_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(content);
    printf(CLR_GREEN "Updated." CLR_RESET " Flesch: %.1f\n", score);
}

static void crud_rename(int story_id) {
    char title[256];
    printf("New title: ");
    if (!fgets(title, sizeof(title), stdin)) return;
    trim_newline(title);
    sqlite3_stmt *stmt;
    sqlite3_prepare_v2(db, "UPDATE STORY SET title = ?1 WHERE story_id = ?2;", -1, &stmt, 0);
    sqlite3_bind_text(stmt, 1, title, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, story_id);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    printf(CLR_GREEN "Renamed." CLR_RESET "\n");
}

static void crud_delete(int story_id) {
    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", 0, 0, 0);
    char sql[128];
    snprintf(sql, sizeof(sql), "DELETE FROM STORY WHERE story_id = %d;", story_id);
    char *err = 0;
    if (sqlite3_exec(db, sql, 0, 0, &err) != SQLITE_OK) { fprintf(stderr, "%s\n", err); sqlite3_free(err); return; }
    printf(CLR_RED "Deleted." CLR_RESET "\n");
}

/* --- 7: Session analytics --- */
static void analytics(void) {
    sqlite3_stmt *stmt;
    const char *sql = "SELECT AVG(time_taken_ms) FROM CHAINWORD WHERE time_taken_ms IS NOT NULL;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return;
    double avg = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) avg = sqlite3_column_double(stmt, 0);
    sqlite3_finalize(stmt);
    printf("\n" CLR_CYAN CLR_BOLD "--- Session analytics ---" CLR_RESET "\n");
    printf("Average linking speed: " CLR_GREEN "%.0f ms" CLR_RESET " per word\n", avg);

    sql = "SELECT anchor_word_id, COUNT(*) AS cnt FROM CHAINWORD WHERE anchor_word_id IS NOT NULL GROUP BY anchor_word_id ORDER BY cnt DESC LIMIT 10;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, 0) != SQLITE_OK) return;
    printf(CLR_DIM "Most frequently used anchor words:" CLR_RESET "\n");
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int aid = sqlite3_column_int(stmt, 0);
        int cnt = sqlite3_column_int(stmt, 1);
        sqlite3_stmt *s2;
        sqlite3_prepare_v2(db, "SELECT word_text FROM WORDPOOL WHERE word_id = ?1;", -1, &s2, 0);
        sqlite3_bind_int(s2, 1, aid);
        const char *w = "(?)";
        if (sqlite3_step(s2) == SQLITE_ROW && sqlite3_column_text(s2, 0)) w = (const char *)sqlite3_column_text(s2, 0);
        printf("  %s (%d times)\n", w, cnt);
        sqlite3_finalize(s2);
    }
    sqlite3_finalize(stmt);
}

static int prompt_story_id(const char *msg) {
    int id;
    printf("%s", msg);
    if (scanf("%d", &id) != 1) return 0;
    while (getchar() != '\n');
    return id;
}

int main(void) {
    if (sqlite3_open(DB_PATH, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open %s\n", sqlite3_errmsg(db));
        return 1;
    }
    ensure_default_user();
    ensure_anchor_column();

    for (;;) {
        printf("\n" CLR_BOLD CLR_CYAN "═══ NarrativeLink ═══" CLR_RESET "\n");
        printf("  " CLR_GREEN "1." CLR_RESET " Chain Reaction (new game)\n");
        printf("  " CLR_GREEN "2." CLR_RESET " Write story (from last chain)\n");
        printf("  " CLR_GREEN "3." CLR_RESET " Archive (browse stories)\n");
        printf("  " CLR_GREEN "4." CLR_RESET " Draft management (view/update/rename/delete)\n");
        printf("  " CLR_GREEN "5." CLR_RESET " Session analytics\n");
        printf("  " CLR_RED "6." CLR_RESET " Quit\n");
        printf(CLR_DIM "Choice: " CLR_RESET);
        fflush(stdout);
        char line[64];
        if (!fgets(line, sizeof(line), stdin)) break;
        trim_newline(line);
        int choice = atoi(line);

        if (choice == 1) {
            printf(CLR_DIM "Timer in seconds [%d]: " CLR_RESET, DEFAULT_TIMER_SEC);
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) continue;
            trim_newline(line);
            int sec = atoi(line);
            if (sec <= 0) sec = DEFAULT_TIMER_SEC;
            play_chain_reaction(sec);
            if (chain_len > 0) write_story_and_save();
        } else if (choice == 2) {
            write_story_and_save();
        } else if (choice == 3) {
            archive_list();
            int id = prompt_story_id("Story ID to view (0 to skip): ");
            if (id > 0) archive_view(id);
        } else if (choice == 4) {
            archive_list();
            int id = prompt_story_id("Story ID: ");
            if (id <= 0) continue;
            printf("v=view, u=update content, r=rename, d=delete: ");
            fflush(stdout);
            if (!fgets(line, sizeof(line), stdin)) continue;
            trim_newline(line);
            if (line[0] == 'v') archive_view(id);
            else if (line[0] == 'u') crud_update_content(id);
            else if (line[0] == 'r') crud_rename(id);
            else if (line[0] == 'd') crud_delete(id);
        } else if (choice == 5) {
            analytics();
        } else if (choice == 6) {
            break;
        }
    }

    free(chain);
    sqlite3_close(db);
    return 0;
}
