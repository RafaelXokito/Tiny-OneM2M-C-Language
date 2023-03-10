#include "Common.h"

char init_protocol(struct sqlite3 * db, struct Route* route) {

    char rs = init_types();
    if (rs == false) {
        return false;
    }

    // Sqlite3 initialization opening/creating database
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    // Check if the cse_base exists
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='mtc';", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    CSEBaseStruct * csebase = malloc(sizeof(CSEBaseStruct));
    
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
        char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM mtc WHERE ty = %d ORDER BY ROWID DESC LIMIT 1;", CSEBASE);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_free(sql);
        if (rc != SQLITE_OK) {
            printf("Failed to prepare 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            printf("Failed to execute 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            sqlite3_close(db);
            return false;
        }

        int rowCount = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);

        if (rowCount == 0) {
            printf("The mtc table dont have CSE_Base resources.\n");

            char rs = init_cse_base(csebase, db, true);
            if (rs == false) {
                return false;
            }
        } else {
            printf("CSE_Base already inserted.\n");

            // In case of the table and data exists, get the 
            char rs = getLastCSEBaseStruct(csebase, db);
            if (rs == false) {
                return false;
            }
        }
    }

    // Add New Routes
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s", csebase->ri);
    addRoute(route, uri, csebase->ri, csebase->ty, csebase->rn);

    return true;
}

char create_ae(struct Route* route, struct Route* destination, cJSON *content, char* response) {
    
    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    printf("Creating AE\n");

    AEStruct * ae = malloc(sizeof(AEStruct));

    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);
    
    char rs = init_ae(ae, content, db);
    if (rs == false) {
        return false;
    }

    // Add New Routes
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s", ae->ri);
    addRoute(route, uri, ae->ri, ae->ty, ae->rn);
    inorder(route);

    // Convert the AE struct to json and the Json Object to Json String
    char *str = cJSON_Print(ae_to_json(ae));
    strcat(response, str);
    free(str);

    return true;
}