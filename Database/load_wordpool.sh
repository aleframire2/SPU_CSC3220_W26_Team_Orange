#!/bin/bash
# Load WORDPOOL from generated SQL into database.db.
# Usage: ./load_wordpool.sh [sqlfile] [dbfile]
# Defaults: out.sql, database.db
#
# IMPORTANT: Close any other program using the database (other sqlite3,
# IDE database viewer, or your app) before running, or you'll get "database is locked".

SQL_FILE="${1:-out.sql}"
DB_FILE="${2:-database.db}"

if [[ ! -f "$SQL_FILE" ]]; then
    echo "Error: $SQL_FILE not found. Run: ./cmudict_to_sql cmudict.dict out.sql" >&2
    exit 1
fi
if [[ ! -f "$DB_FILE" ]]; then
    echo "Error: $DB_FILE not found." >&2
    exit 1
fi

echo "Clearing WORDPOOL and loading from $SQL_FILE into $DB_FILE ..."
sqlite3 "$DB_FILE" "DELETE FROM WORDPOOL;"
sqlite3 "$DB_FILE" ".read $SQL_FILE"
echo "Done. Row count:"
sqlite3 "$DB_FILE" "SELECT COUNT(*) FROM WORDPOOL;"
