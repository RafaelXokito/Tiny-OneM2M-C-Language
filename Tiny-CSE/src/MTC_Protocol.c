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

char retrieve_csebase(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, ct, lt FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    printf("Creating the json object\n");
    CSEBaseStruct csebase;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        csebase.ty = sqlite3_column_int(stmt, 0);
        strncpy(csebase.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(csebase.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(csebase.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(csebase.ct, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(csebase.lt, (char *)sqlite3_column_text(stmt, 5), 25);
        break;
    }

    cJSON *root = csebase_to_json(&csebase);

    // Print the resulting JSON string
    char* jsonString = cJSON_Print(root);
    printf("%s", jsonString);

    printf("Coonvert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);

    char * response_data = json_str;
    
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(json_str);

    return true;
}

char create_ae(struct Route* route, struct Route* destination, cJSON *content, char* response) {
    
    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    printf("Creating AE\n");

    AEStruct ae;

    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);
    
    char rs = init_ae(&ae, content, db);
    if (rs == false) {
        return false;
    }

    // Add New Routes
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s", ae.ri);
    addRoute(route, uri, ae.ri, ae.ty, ae.rn);
    inorder(route);

    // Convert the AE struct to json and the Json Object to Json String
    cJSON *root = ae_to_json(&ae);
    char *str = cJSON_Print(root);
    strcat(response, str);

    return true;
}

char retrieve_ae(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
    }

    printf("Creating the json object\n");
        AEStruct ae;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae.ty = sqlite3_column_int(stmt, 0);
        strncpy(ae.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(ae.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(ae.et, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.ct, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.lt, (char *)sqlite3_column_text(stmt, 5), 25);
        break;
    }

    cJSON* root = ae_to_json(&ae);

    printf("Coonvert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);

    char * response_data = json_str;
    
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(json_str);

    return true;
}