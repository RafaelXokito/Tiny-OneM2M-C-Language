#include "Common.h"

extern int DAYS_PLUS_ET;

char init_ae(AEStruct * ae, cJSON *content, char* response) {
    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response,500,"Internal Server Error","Could not open the database");
		return FALSE;
	}

    // Convert the JSON object to a C structure
    ae->ty = AE;
    strcpy(ae->ri, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring);
    strcpy(ae->rn, cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    strcpy(ae->pi, cJSON_GetObjectItemCaseSensitive(content, "pi")->valuestring);
    strcpy(ae->et, get_datetime_days_later(DAYS_PLUS_ET));
    strcpy(ae->ct, getCurrentTime());
    strcpy(ae->lt, getCurrentTime());


    // Free the cJSON object
    cJSON_Delete(content);
    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, et, ct, lt) VALUES (?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        closeDatabase(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, ae->ty);
    sqlite3_bind_text(stmt, 2, ae->ri, strlen(ae->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ae->rn, strlen(ae->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, ae->pi, strlen(ae->pi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, ae->et, strlen(ae->et), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, ae->ct, strlen(ae->ct), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, ae->lt, strlen(ae->lt), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    closeDatabase(db);

    printf("AE data inserted successfully.\n");

    return TRUE;
}

cJSON *ae_to_json(const AEStruct *ae) {
    cJSON *innerObject = cJSON_CreateObject();
    cJSON_AddNumberToObject(innerObject, "ty", ae->ty);
    cJSON_AddStringToObject(innerObject, "ri", ae->ri);
    cJSON_AddStringToObject(innerObject, "rn", ae->rn);
    cJSON_AddStringToObject(innerObject, "pi", ae->pi);
    cJSON_AddStringToObject(innerObject, "et", ae->et);
    cJSON_AddStringToObject(innerObject, "ct", ae->ct);
    cJSON_AddStringToObject(innerObject, "lt", ae->lt);

    // Create the outer JSON object with the key "m2m:ae" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:ae", innerObject);

    return root;
}