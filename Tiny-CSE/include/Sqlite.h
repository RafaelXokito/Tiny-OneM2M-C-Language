/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

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