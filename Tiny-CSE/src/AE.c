#include "Common.h"

char init_ae(AEStruct * ae, cJSON *content, struct sqlite3 * db) {

    // Convert the JSON object to a C structure
    ae->ty = AE;
    strcpy(ae->ri, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring);
    strcpy(ae->rn, cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    strcpy(ae->pi, cJSON_GetObjectItemCaseSensitive(content, "pi")->valuestring);
    strcpy(ae->et, get_datetime_one_month_later());
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
        sqlite3_close(db);
        return false;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, ae->ty);
    to_lowercase(ae->ri);
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
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return false;
    }

    // Finalize the statement and close the database
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("AE data inserted successfully.\n");

    return true;
}

cJSON *ae_to_json(const AEStruct *ae) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "ty", ae->ty);
    cJSON_AddStringToObject(root, "ri", ae->ri);
    cJSON_AddStringToObject(root, "rn", ae->rn);
    cJSON_AddStringToObject(root, "pi", ae->pi);
    cJSON_AddStringToObject(root, "et", ae->et);
    cJSON_AddStringToObject(root, "ct", ae->ct);
    cJSON_AddStringToObject(root, "lt", ae->lt);
    return root;
}