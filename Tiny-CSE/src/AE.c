/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *
 * Copyright (c) 2023 IPLeiria
 */

#include "Common.h"

extern int DAYS_PLUS_ET;

AEStruct *init_ae() {
    AEStruct *ae = (AEStruct *) malloc(sizeof(AEStruct));
    if (ae) {
        ae->apn[0] = '\0';
        ae->ct[0] = '\0';
        ae->ty = 0;
        ae->json_acpi = NULL;
        ae->et[0] = '\0';
        ae->json_lbl = NULL;
        ae->pi[0] = '\0';
        ae->json_daci = NULL;
        ae->json_poa = NULL;
        ae->json_ch = NULL;
        ae->aa[0] = '\0';
        ae->aei[0] = '\0';
        ae->rn[0] = '\0';
        ae->api[0] = '\0';
        ae->rr[0] = '\0';
        ae->csz[0] = '\0';
        ae->ri[0] = '\0';
        ae->nl[0] = '\0';
        ae->json_at = NULL;
        ae->or[0] = '\0';
        ae->lt[0] = '\0';
    }
    return ae;
}

char create_ae(AEStruct * ae, cJSON *content, char* response) {
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
    strcpy(ae->aei, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring); // AEI igual ao RI
    strcpy(ae->api, cJSON_GetObjectItemCaseSensitive(content, "api")->valuestring);
    strcpy(ae->rr, cJSON_GetObjectItemCaseSensitive(content, "rr")->valuestring);
    strcpy(ae->et, get_datetime_days_later(DAYS_PLUS_ET));
    strcpy(ae->ct, getCurrentTime());
    strcpy(ae->lt, getCurrentTime());

    const char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    char **json_strings[] = {&ae->json_acpi, &ae->json_lbl, &ae->json_daci, &ae->json_poa, &ae->json_ch, &ae->json_at};

    for (int i = 0; i < num_keys; i++) {
        cJSON *json_array = cJSON_GetObjectItemCaseSensitive(content, keys[i]);
        if (json_array) {
            char *json_str = cJSON_Print(json_array);
            if (json_str) {
                *json_strings[i] = strdup(json_str);
                if (*json_strings[i] == NULL) {
                    fprintf(stderr, "Failed to allocate memory for the JSON %s string\n", keys[i]);
                    responseMessage(response, 400, "Bad Request", "Verify the request body");
                    closeDatabase(db);
                    return FALSE;
                }
                free(json_str);
            }
        }
    }

    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }

    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, aei, api, rr, et, ct, lt) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr,"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        closeDatabase(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, ae->ty);
    sqlite3_bind_text(stmt, 2, ae->ri, strlen(ae->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, ae->rn, strlen(ae->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, ae->pi, strlen(ae->pi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, ae->aei, strlen(ae->aei), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, ae->api, strlen(ae->api), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, ae->rr, strlen(ae->rr), SQLITE_STATIC);
    struct tm ct_tm, lt_tm, et_tm;
    strptime(ae->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(ae->lt, "%Y%m%dT%H%M%S", &lt_tm);
    strptime(ae->et, "%Y%m%dT%H%M%S", &et_tm);
    char ct_iso[30], lt_iso[30], et_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
    sqlite3_bind_text(stmt, 8, et_iso, strlen(et_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, lt_iso, strlen(lt_iso), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr,"Failed to execute statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        rollback_transaction(db); // Rollback transaction
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Keys to check in the JSON object
    const char *keys_to_check[] = {"acpi", "lbl", "daci", "poa"};
    num_keys = sizeof(keys_to_check) / sizeof(keys_to_check[0]);

    // Initialize an array of strings to store the keys that are arrays
    const char *array_keys[num_keys];
    int count = 0;

    for (int i = 0; i < num_keys; i++) {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(content, keys_to_check[i]);
        if (cJSON_IsArray(item)) {
            array_keys[count++] = keys_to_check[i];
        }
    }

    for (int i = 0; i < count; i++) {
        cJSON *atr_array = cJSON_GetObjectItemCaseSensitive(content, array_keys[i]);
        char *str = cJSON_Print(atr_array);
        if (cJSON_IsArray(atr_array)) {
            if (insert_multivalue_elements(db, ae->ri, array_keys[i], array_keys[i], atr_array) == FALSE) {
                rollback_transaction(db); // Rollback transaction
                closeDatabase(db);
                cJSON_Delete(content);
                return FALSE;
            }
        }
    }

    // Free the cJSON object
    cJSON_Delete(content);

    // Commit transaction
    rc = commit_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't commit transaction\n");
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
    cJSON_AddStringToObject(innerObject, "apn", ae->apn);
    cJSON_AddStringToObject(innerObject, "ct", ae->ct);
    cJSON_AddNumberToObject(innerObject, "ty", ae->ty);
    cJSON_AddStringToObject(innerObject, "ri", ae->ri);
    cJSON_AddStringToObject(innerObject, "rn", ae->rn);
    cJSON_AddStringToObject(innerObject, "pi", ae->pi);
    cJSON_AddStringToObject(innerObject, "aa", ae->aa);
    cJSON_AddStringToObject(innerObject, "aei", ae->aei);
    cJSON_AddStringToObject(innerObject, "api", ae->api);
    cJSON_AddStringToObject(innerObject, "csz", ae->csz);
    cJSON_AddStringToObject(innerObject, "et", ae->et);
    cJSON_AddStringToObject(innerObject, "nl", ae->nl);
    cJSON_AddStringToObject(innerObject, "or", ae->or);
    cJSON_AddStringToObject(innerObject, "lt", ae->lt);

    short rr_bool = 0;
    if (strcmp(ae->rr, "true") == 0) {
        rr_bool = 1;
    } else {
        rr_bool = 0;
    }
    cJSON_AddBoolToObject(innerObject, "rr", rr_bool);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {ae->json_acpi, ae->json_lbl, ae->json_daci, ae->json_poa, ae->json_ch, ae->json_at};

    for (int i = 0; i < num_keys; i++) {
        if (json_strings[i] != NULL) {
            cJSON *json_object = cJSON_Parse(json_strings[i]);
            if (json_object) {
                cJSON_AddItemToObject(innerObject, keys[i], json_object);
            } else {
                fprintf(stderr, "Failed to parse JSON string for key '%s' back into cJSON object\n", keys[i]);
            }
        } else {
            cJSON *empty_array = cJSON_CreateArray();
            cJSON_AddItemToObject(innerObject, keys[i], empty_array);
        }
    }

    // Create the outer JSON object with the key "m2m:ae" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:ae", innerObject);

    return root;
}
