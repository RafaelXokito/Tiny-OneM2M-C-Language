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
    CSEBaseStruct *cse = (CSEBaseStruct *) malloc(sizeof(CSEBaseStruct));
    if (cse) {
        cse->url = NULL;
        cse->ty = 0;
        cse->ri[0] = '\0';
        cse->rn[0] = '\0';
        cse->pi[0] = '\0';
        cse->cst = 0;
        cse->json_srt = NULL;
        cse->json_lbl = NULL;
        cse->csi[0] = '\0';
        cse->nl[0] = '\0';
        cse->json_poa = NULL;
        cse->json_acpi = NULL;
        cse->ct[0] = '\0';
        cse->lt[0] = '\0';
    }
    return cse;
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

    size_t rnLength = strlen(cJSON_GetObjectItemCaseSensitive(json, "rn")->valuestring);
    // Allocate memory for ae->url, considering the extra characters for "/", and the null terminator.
    csebase->url = (char *)malloc(rnLength + 2);

    // Check if memory allocation is successful
    if (csebase->url == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }

    // Append "/<rn value>" to ae->url
    sprintf(csebase->url, "/%s", cJSON_GetObjectItemCaseSensitive(json, "rn")->valuestring);
    strcpy(csebase->rn, cJSON_GetObjectItemCaseSensitive(json, "rn")->valuestring);
    strcpy(csebase->pi, cJSON_GetObjectItemCaseSensitive(json, "pi")->valuestring);
    strcpy(csebase->csi, cJSON_GetObjectItemCaseSensitive(json, "csi")->valuestring);
    csebase->cst = cJSON_GetObjectItemCaseSensitive(json, "cst")->valueint;
    strcpy(csebase->ct, getCurrentTime());
    strcpy(csebase->lt, getCurrentTime());
    
    cJSON *json_array = cJSON_GetObjectItemCaseSensitive(json,  "poa");
    if (json_array) {
        char *json_str = cJSON_Print(json_array);
        size_t rnLengthPoa = strlen(json_str);
        csebase->json_poa = (char *)malloc(rnLengthPoa);
        if (csebase->json_poa == NULL) {
            // Handle memory allocation error
            fprintf(stderr, "Memory allocation error\n");
            return FALSE;
        }
        strcpy(csebase->json_poa, json_str);
    }
    
    size_t rnLengthBlob = strlen(cJSON_Print(csebase_to_json(csebase)));
    csebase->blob = (char *)malloc(rnLengthBlob);
    if (csebase->blob == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }
    strcpy(csebase->blob, cJSON_Print(csebase_to_json(csebase)));
    
    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }

    if (isTableCreated == FALSE) {
        // Create the table if it doesn't exist
        const char *createTableSQL = "CREATE TABLE IF NOT EXISTS mtc (ty INTEGER, ri TEXT PRIMARY KEY, rn TEXT, pi TEXT, aei TEXT, csi TEXT, cst INTEGER, api TEXT, rr TEXT, et DATETIME, ct DATETIME, lt DATETIME, url TEXT, lbl TEXT, acpi TEXT, daci TEXT, poa TEXT, blob TEXT)";
        short rc = sqlite3_exec(db, createTableSQL, NULL, NULL, NULL);
        if (rc != SQLITE_OK) {
            printf("Failed to create table: %s\n", sqlite3_errmsg(db));
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

        const char *sql3 = "CREATE INDEX IF NOT EXISTS idx_mtc_et ON mtc(et);";
        rc = sqlite3_exec(db, sql3, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_mtc_ri created successfully\n");
        }

        const char *sql4 = "CREATE INDEX IF NOT EXISTS idx_mtc_url ON mtc(url);";
        rc = sqlite3_exec(db, sql4, callback, 0, &zErrMsg);

        if(rc != SQLITE_OK) {
            fprintf(stderr, "SQL error: %s\n", zErrMsg);
            sqlite3_free(zErrMsg);
        } else {
            fprintf(stdout, "Index idx_mtc_url created successfully\n");
        }
    }
   
    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, cst, csi, ct, lt, url, poa, blob) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
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
    sqlite3_bind_text(stmt, 9, csebase->url, strlen(csebase->url), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, csebase->json_poa, strlen(csebase->json_poa), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, csebase->blob, strlen(csebase->blob), SQLITE_STATIC);

    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        rollback_transaction(db); // Rollback transaction
        closeDatabase(db);
        return FALSE;
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
    const char *keys[] = {"acpi", "lbl", "srt", "poa"};
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