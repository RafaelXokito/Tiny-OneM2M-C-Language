/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include "Common.h"

extern int PORT;
extern char BASE_RI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_RN[MAX_CONFIG_LINE_LENGTH];
extern char BASE_CSI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_POA[MAX_CONFIG_LINE_LENGTH];

CSEBaseStruct *init_cse_base() {
    CSEBaseStruct *ae = (CSEBaseStruct *) malloc(sizeof(CSEBaseStruct));
    if (ae) {
        ae->ty = 0;
        ae->ri[0] = '\0';
        ae->rn[0] = '\0';
        ae->pi[0] = '\0';
        ae->cst = 0;
        ae->json_srt = NULL;
        ae->json_lbl = NULL;
        ae->csi[0] = '\0';
        ae->nl[0] = '\0';
        ae->json_poa = NULL;
        ae->json_acpi = NULL;
        ae->ct[0] = '\0';
        ae->lt[0] = '\0';
    }
    return ae;
}

char create_cse_base(CSEBaseStruct * csebase, char isTableCreated) {

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

    char jsonString[512]; // Adjust the size of the buffer as needed to fit the largest possible jsonString
    snprintf(jsonString, sizeof(jsonString), "{\"rn\":\"%s\",\"cst\":2,\"srt\":[4,1,24,16,23,3,5,2],\"lbl\":[],\"csi\":\"/mn-cse-1\",\"nl\":null,\"ri\":\"%s\",\"poa\":[\"http://127.0.0.1:%d\",\"%s\"],\"acpi\":[],\"ty\":5,\"pi\": \"\"}", BASE_RN, BASE_RI, PORT, BASE_POA);
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
    strcpy(csebase->csi, cJSON_GetObjectItemCaseSensitive(json, "csi")->valuestring);
    csebase->cst = cJSON_GetObjectItemCaseSensitive(json, "cst")->valueint;
    strcpy(csebase->ct, getCurrentTime());
    strcpy(csebase->lt, getCurrentTime());

    const char *keys[] = {"acpi", "lbl", "srt", "poa"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    char **json_strings[] = {&csebase->json_acpi, &csebase->json_lbl, &csebase->json_srt, &csebase->json_poa};

    for (int i = 0; i < num_keys; i++) {
        cJSON *json_array = cJSON_GetObjectItemCaseSensitive(json, keys[i]);
        if (json_array) {
            char *json_str = cJSON_Print(json_array);
            if (json_str) {
                *json_strings[i] = strdup(json_str);
                if (*json_strings[i] == NULL) {
                    fprintf(stderr, "Failed to allocate memory for the JSON %s string\n", keys[i]);
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

    if (isTableCreated == FALSE) {
        // Create the table if it doesn't exist
        const char *createTableSQL = "CREATE TABLE IF NOT EXISTS mtc (ty INTEGER, ri TEXT PRIMARY KEY, rn TEXT, pi TEXT, aei TEXT, csi TEXT, cst INTEGER, api TEXT, rr TEXT, et DATETIME, ct DATETIME, lt DATETIME)";
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
            rollback_transaction(db); // Rollback transaction
            closeDatabase(db);
            return FALSE;
        }

        char *zErrMsg = 0;
        const char *sql1 = "CREATE INDEX IF NOT EXISTS idx_mtc_pi ON mtc(pi);";
        rc = sqlite3_exec(db, sql1, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_mtc_pi created successfully\n");
        }

        const char *sql2 = "CREATE INDEX IF NOT EXISTS idx_mtc_ri ON mtc(ri);";
        rc = sqlite3_exec(db, sql2, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_mtc_ri created successfully\n");
        }

        const char *sql3 = "CREATE INDEX IF NOT EXISTS idx_multivalue_parent_id ON multivalue(parent_id);";
        rc = sqlite3_exec(db, sql3, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_multivalue_parent_id created successfully\n");
        }

        const char *sql4 = "CREATE INDEX IF NOT EXISTS idx_multivalue_atr_value ON multivalue(atr, value);";
        rc = sqlite3_exec(db, sql4, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_multivalue_atr_value created successfully\n");
        }
    }

    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, cst, csi, ct, lt) VALUES (?, ?, ?, ?, ?, ?, ?, ?)";
    sqlite3_stmt *stmt;
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        rollback_transaction(db); // Rollback transaction
        closeDatabase(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, csebase->ty);
    sqlite3_bind_text(stmt, 2, csebase->ri, strlen(csebase->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, csebase->rn, strlen(csebase->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, csebase->pi, strlen(csebase->pi), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, csebase->cst);
    sqlite3_bind_text(stmt, 6, csebase->csi, strlen(csebase->csi), SQLITE_STATIC);
    struct tm ct_tm, lt_tm;
    strptime(csebase->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(csebase->lt, "%Y%m%dT%H%M%S", &lt_tm);
    char ct_iso[30], lt_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    sqlite3_bind_text(stmt, 7, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, lt_iso, strlen(lt_iso), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        rollback_transaction(db); // Rollback transaction
        closeDatabase(db);
        return FALSE;
    }

    // Keys to check in the JSON object
    const char *keys_to_check[] = {"acpi", "lbl", "srt", "poa"};
    num_keys = sizeof(keys_to_check) / sizeof(keys_to_check[0]);

    // Initialize an array of strings to store the keys that are arrays
    const char *array_keys[num_keys];
    int count = 0;

    for (int i = 0; i < num_keys; i++) {
        cJSON *item = cJSON_GetObjectItemCaseSensitive(json, keys_to_check[i]);
        if (cJSON_IsArray(item)) {
            array_keys[count++] = keys_to_check[i];
        }
    }

    for (int i = 0; i < count; i++) {
        cJSON *atr_array = cJSON_GetObjectItemCaseSensitive(json, array_keys[i]);
        char *str = cJSON_Print(atr_array);
        if (cJSON_IsArray(atr_array)) {
            if (insert_multivalue_elements(db, csebase->ri, array_keys[i], array_keys[i], atr_array) == FALSE) {
                rollback_transaction(db); // Rollback transaction
                closeDatabase(db);
                cJSON_Delete(json);
                return FALSE;
            }
        }
    }


    // Free the cJSON object
    cJSON_Delete(json);

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
    cJSON_AddNumberToObject(innerObject, "cst", csebase->cst);
    cJSON_AddStringToObject(innerObject, "csi", csebase->csi);
    cJSON_AddStringToObject(innerObject, "nl", csebase->nl);
    cJSON_AddStringToObject(innerObject, "ct", csebase->ct);
    cJSON_AddStringToObject(innerObject, "lt", csebase->lt);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"srt", "lbl", "poa", "acpi"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {csebase->json_acpi, csebase->json_lbl, csebase->json_srt, csebase->json_poa};

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

    // Create the outer JSON object with the key "m2m:cse" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:cb", innerObject);    
    return root;
}