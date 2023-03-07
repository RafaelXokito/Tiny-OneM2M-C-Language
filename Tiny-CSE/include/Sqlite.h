#include <stdio.h>
#include <sqlite3.h>

int callback(void *NotUsed, int argc, char **argv, char **azColName);

struct sqlite3 *initDatabase(char* database_name);