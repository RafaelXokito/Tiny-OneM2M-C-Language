#include "Common.h"

char init_cse_base(CSEBase * csebase, struct sqlite3 * db, char isTableCreated) {

    // {
    //     "ty": 5,
    //     "ri": "id-in",
    //     "rn": "cse-in",
    //     "pi": "",
    //     "ct": "20230309T111952,126300",
    //     "lt": "20230309T111952,126300"
    // }

	// Parse the JSON object
    cJSON *json = cJSON_Parse("{\"ty\": 5, \"ri\": \"id-in\", \"rn\": \"cse-in\", \"pi\": \"\", \"ct\": \"20230309T111952,126300\", \"lt\": \"20230309T111952,126300\"}");
    if (json == NULL) {
        printf("Failed to parse JSON.\n");
        return false;
    }

    // Convert the JSON object to a C structure
    csebase->ty = cJSON_GetObjectItemCaseSensitive(json, "ty")->valueint;
    strcpy(csebase->ri, cJSON_GetObjectItemCaseSensitive(json, "ri")->valuestring);
    strcpy(csebase->rn, cJSON_GetObjectItemCaseSensitive(json, "rn")->valuestring);
    strcpy(csebase->pi, cJSON_GetObjectItemCaseSensitive(json, "pi")->valuestring);
    strcpy(csebase->ct, cJSON_GetObjectItemCaseSensitive(json, "ct")->valuestring);
    strcpy(csebase->lt, cJSON_GetObjectItemCaseSensitive(json, "lt")->valuestring);

    // Free the cJSON object
    cJSON_Delete(json);

    if (isTableCreated == false) {
        // Create the table if it doesn't exist
        const char *createTableSQL = "CREATE TABLE IF NOT EXISTS csebase (ty INTEGER, ri TEXT, rn TEXT, pi TEXT, ct TEXT, lt TEXT)";
        short rc = sqlite3_exec(db, createTableSQL, NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            printf("Failed to create table: %s\n", sqlite3_errmsg(db));
            sqlite3_close(db);
            return false;
        }
    }

    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO csebase (ty, ri, rn, pi, ct, lt) VALUES (?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
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
        return false;
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("CSE_Base data inserted successfully.\n");
    return true;
}