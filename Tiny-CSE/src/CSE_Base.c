#include "Common.h"

extern char BASE_RI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_RN[MAX_CONFIG_LINE_LENGTH];

char init_cse_base(CSEBaseStruct * csebase, char isTableCreated) {

    // {
    //     "ty": 5,
    //     "ri": "id-in",
    //     "rn": "cse-in",
    //     "pi": "",
    //     "ct": "20230309T111952,126300",
    //     "lt": "20230309T111952,126300"
    // }

    // Sqlite3 initialization opening/creating database
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return FALSE;
	}

	// Parse the JSON object
    char jsonString[256]; // Adjust the size of the buffer as needed to fit the largest possible jsonString
    snprintf(jsonString, sizeof(jsonString), "{\"ty\": 5, \"ri\": \"%s\", \"rn\": \"%s\", \"pi\": \"\"}", BASE_RI, BASE_RN);
    cJSON *json = cJSON_Parse(jsonString);
    if (json == NULL) {
        printf("Failed to parse JSON.\n");
        return FALSE;
    }

    // Convert the JSON object to a C structure
    csebase->ty = cJSON_GetObjectItemCaseSensitive(json, "ty")->valueint;
    strcpy(csebase->ri, cJSON_GetObjectItemCaseSensitive(json, "ri")->valuestring);
    strcpy(csebase->rn, cJSON_GetObjectItemCaseSensitive(json, "rn")->valuestring);
    strcpy(csebase->pi, cJSON_GetObjectItemCaseSensitive(json, "pi")->valuestring);
    strcpy(csebase->ct, getCurrentTime());
    strcpy(csebase->lt, getCurrentTime());

    // Free the cJSON object
    cJSON_Delete(json);

    if (isTableCreated == FALSE) {
        // Create the table if it doesn't exist
        const char *createTableSQL = "CREATE TABLE IF NOT EXISTS mtc (ty INTEGER, ri TEXT PRIMARY KEY, rn TEXT, pi TEXT, aei TEXT, api TEXT, rr TEXT, et TEXT, ct TEXT, lt TEXT)";
        short rc = sqlite3_exec(db, createTableSQL, NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            printf("Failed to create table: %s\n", sqlite3_errmsg(db));
            closeDatabase(db);
            return FALSE;
        }

        // Create the multivalue table
        rc = create_multivalue_table(db);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Can't create multivalue table\n");
            closeDatabase(db);
            return FALSE;
        }
    }

    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, ct, lt) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, csebase->ty);
    sqlite3_bind_text(stmt, 2, csebase->ri, strlen(csebase->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, csebase->rn, strlen(csebase->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, csebase->pi, strlen(csebase->pi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, csebase->ct, strlen(csebase->ct), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, csebase->lt, strlen(csebase->lt), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return FALSE;
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    closeDatabase(db);

    printf("CSE_Base data inserted successfully.\n");

    return TRUE;
}

char getLastCSEBaseStruct(CSEBaseStruct * csebase, sqlite3 *db) {

    // Prepare the SQL statement to retrieve the last row from the table
    
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, ct, lt FROM mtc WHERE ty = %d ORDER BY ROWID DESC LIMIT 1;", CSEBASE);
    sqlite3_stmt *stmt;
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    sqlite3_free(sql);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare getLastCSEBaseStruct query: %s\n", sqlite3_errmsg(db));
        return FALSE;
    }

    // Execute the query
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // Retrieve the data from the row
        csebase->ty = sqlite3_column_int(stmt, 0);
        strncpy(csebase->ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(csebase->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(csebase->pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(csebase->ct, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(csebase->lt, (char *)sqlite3_column_text(stmt, 5), 25);
    }

    sqlite3_finalize(stmt);

    return TRUE;
}

cJSON *csebase_to_json(const CSEBaseStruct *csebase) {
    cJSON *innerObject = cJSON_CreateObject();
    cJSON_AddNumberToObject(innerObject, "ty", csebase->ty);
    cJSON_AddStringToObject(innerObject, "ri", csebase->ri);
    cJSON_AddStringToObject(innerObject, "rn", csebase->rn);
    cJSON_AddStringToObject(innerObject, "pi", csebase->pi);
    cJSON_AddStringToObject(innerObject, "ct", csebase->ct);
    cJSON_AddStringToObject(innerObject, "lt", csebase->lt);

    // Create the outer JSON object with the key "m2m:cse" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:cb", innerObject);    
    return root;
}