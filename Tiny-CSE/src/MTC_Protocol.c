#include "Common.h"

char init_protocol(struct sqlite3 * db, struct Route* route) {

    // Sqlite3 initialization opening/creating database
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    // Check if the cse_base exists
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='csebase';", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    CSEBase * csebase = malloc(sizeof(CSEBase));
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        // If table dosnt exist we create it and populate it
        printf("The table does not exist.\n");
        sqlite3_finalize(stmt);
        
        char rs = init_cse_base(csebase, db, false);
        if (rs == false) {
            return false;
        }
    } else {

        sqlite3_finalize(stmt);

        // Check if the table has any data
        sqlite3_stmt *stmt;
        rc = sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM csebase;", -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            printf("Failed to prepare query: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            printf("Failed to execute query: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        int rowCount = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);

        if (rowCount == 0) {
            printf("The csebase table is empty.\n");

            char rs = init_cse_base(csebase, db, true);
            if (rs == false) {
                return false;
            }
        } else {
            printf("CSE_Base already inserted.\n");

            char rs = getLastCSEBase(csebase, db);
            if (rs == false) {
                return false;
            }
        }

    }

    // TODO - Creation and Last Modification Time
    // TODO - Quando a BD já está criada, o root node deve ser carregado para um jsonObject e criar as rotas

    // Add New Routes
    char uri[50];
    snprintf(uri, sizeof(uri), "/%s", csebase->ri);
    addRoute(route, uri, csebase->ty, csebase->rn);

    return true;
}