#include <stdio.h>
#include <sqlite3.h>

int callback(void *NotUsed, int argc, char **argv, char **azColName);

sqlite3 *initDatabase(const char* databasename);

short execDatabaseScript(char* query, struct sqlite3 *db, short isCallback);

int closeDatabase(sqlite3 *db);

int create_multivalue_table(sqlite3 *db);

int begin_transaction(sqlite3 *db);
int commit_transaction(sqlite3 *db);
int rollback_transaction(sqlite3 *db);