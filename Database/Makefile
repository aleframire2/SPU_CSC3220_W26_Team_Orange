# NarrativeLink CLI and DB utilities
# Use database.db (same as load_wordpool.sh). Ensure WORDPOOL is populated.

CC = gcc
CFLAGS = -std=c11 -O2 -Wall

narrativelink: narrativelink.c sqlite3.c sqlite3.h
	$(CC) $(CFLAGS) -o narrativelink narrativelink.c sqlite3.c -lpthread -ldl

cmudict_to_sql: cmudict_to_sql.c
	$(CC) $(CFLAGS) -o cmudict_to_sql cmudict_to_sql.c

main: main.c sqlite3.c sqlite3.h
	$(CC) $(CFLAGS) -o main main.c sqlite3.c -lpthread -ldl

.PHONY: clean
clean:
	rm -f narrativelink cmudict_to_sql main
