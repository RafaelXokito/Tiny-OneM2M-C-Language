#include <stdio.h>
#include <sqlite3.h>

#define true 1
#define false 0

int callback(void *NotUsed, int argc, char **argv, char **azColName) {
    int i;
    for (i = 0; i < argc; i++) {
        printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    printf("\n");
    return 0;
}

// Define an init function that returns an sqlite3 pointer
struct sqlite3 *initDatabase(char* databasename) {
  sqlite3 *db;
  char *err_msg = 0;
  int rc = sqlite3_open(databasename, &db);
  if (rc != SQLITE_OK) {
    fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return NULL;
  }
  return db;
}

short execDatabaseScript(char* query, struct sqlite3 *db, short isCallback) {
    char *err_msg = 0;
    short rc = -1;
    if (isCallback == false) {
        rc = sqlite3_exec(db, query, NULL, NULL, &err_msg);
    } else {
        rc = sqlite3_exec(db, query, callback, NULL, &err_msg);
    }
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    return 0;
}

void closeDatabase(struct sqlite3 *db) {
    sqlite3_close(db);
}
