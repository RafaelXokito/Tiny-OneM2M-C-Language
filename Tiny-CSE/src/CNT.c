/*
 * Created on Mon Apr 24 2023
 *
 * Author(s): Rafcntl Pereira (Rafcntl_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include "Common.h"

extern int DAYS_PLUS_ET;

CNTStruct *init_cnt() {
    CNTStruct *cnt = (CNTStruct *) malloc(sizeof(CNTStruct));
    if (cnt) {
        cnt->url = NULL;
        cnt->apn[0] = '\0';
        cnt->ct[0] = '\0';
        cnt->ty = 0;
        cnt->json_acpi = NULL;
        cnt->et[0] = '\0';
        cnt->json_lbl = NULL;
        cnt->pi[0] = '\0';
        cnt->json_daci = NULL;
        cnt->json_poa = NULL;
        cnt->json_ch = NULL;
        cnt->aa[0] = '\0';
        cnt->cnti[0] = '\0';
        cnt->rn[0] = '\0';
        cnt->api[0] = '\0';
        cnt->rr[0] = '\0';
        cnt->csz[0] = '\0';
        cnt->ri[0] = '\0';
        cnt->nl[0] = '\0';
        cnt->json_at = NULL;
        cnt->or[0] = '\0';
        cnt->lt[0] = '\0';
    }
    return cnt;
}

char create_cnt(CNTStruct * cnt, cJSON *content, char** response) {
    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response,500,"Internal Server Error","Could not open the database");
		return FALSE;
	}

    // Convert the JSON object to a C structure
    // the URL attribute was already populated in the caller of this function
    sqlite3_stmt *stmt;
    int result;
    const char *query = "SELECT COALESCE(MAX(CAST(substr(ri, 4) AS INTEGER)), 0) + 1 as result FROM mtc WHERE ty = 2";
    // Prepare the SQL statement
    if (sqlite3_prepare_v2(db, query, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "Cannot prepare statement: %s\n", sqlite3_errmsg(db));
        closeDatabase(db);
        return FALSE;
    }

    char *ri = NULL;
    // Execute the query and fetch the result
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        result = sqlite3_column_int(stmt, 0);
        size_t ri_size = snprintf(NULL, 0, "CCNT%d", result) + 1; // Get the required size for the ri string

        ri = malloc(ri_size * sizeof(char)); // Allocate dynamic memory for the ri string
        if (ri == NULL) {
            fprintf(stderr, "Failed to allocate memory for ri\n");
            closeDatabase(db);
            return FALSE;
        }
        snprintf(ri, ri_size, "CCNT%d", result); // Write the formatted string to ri
        cJSON_AddStringToObject(content, "ri", ri);
    } else {
        fprintf(stderr, "Failed to fetch the result\n");
        closeDatabase(db);
        return FALSE;
    }

    cnt->ty = CNT;
    strcpy(cnt->ri, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring);
    strcpy(cnt->rn, cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    strcpy(cnt->pi, cJSON_GetObjectItemCaseSensitive(content, "pi")->valuestring);
    strcpy(cnt->cnti, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring); // CNTI igual ao RI
    strcpy(cnt->api, cJSON_GetObjectItemCaseSensitive(content, "api")->valuestring);
    strcpy(cnt->rr, cJSON_GetObjectItemCaseSensitive(content, "rr")->valuestring);
    cJSON *et = cJSON_GetObjectItemCaseSensitive(content, "et");
    if (et) {
        struct tm et_tm;
        char *parse_result = strptime(et->valuestring, "%Y%m%dT%H%M%S", &et_tm);
        if (parse_result == NULL) {
            // The date string did not match the expected format
            responseMessage(response, 400, "Bad Request", "Invalid date format");
            return FALSE;
        }

        time_t datetime_timestamp, current_time;
        
        // Convert the parsed time to a timestamp
        datetime_timestamp = mktime(&et_tm);
        // Get the current time
        current_time = time(NULL);

        // Compara o timestamp atual com o timestamp recebido, caso o timestamp está no passado dá excepção
        if (difftime(datetime_timestamp, current_time) < 0) {
            responseMessage(response, 400, "Bad Request", "Expiration time is in the past");
            return FALSE;
        }        
        strcpy(cnt->et, et->valuestring);
    } else {
        strcpy(cnt->et, get_datetime_days_later(DAYS_PLUS_ET));
    }
    strcpy(cnt->ct, getCurrentTime());
    strcpy(cnt->lt, getCurrentTime());

    const char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    char **json_strings[] = {&cnt->json_acpi, &cnt->json_lbl, &cnt->json_daci, &cnt->json_poa, &cnt->json_ch, &cnt->json_at};

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
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, cnti, api, rr, et, ct, lt, url) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr,"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        closeDatabase(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, cnt->ty);
    sqlite3_bind_text(stmt, 2, cnt->ri, strlen(cnt->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, cnt->rn, strlen(cnt->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, cnt->pi, strlen(cnt->pi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, cnt->cnti, strlen(cnt->cnti), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, cnt->api, strlen(cnt->api), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, cnt->rr, strlen(cnt->rr), SQLITE_STATIC);
    struct tm ct_tm, lt_tm, et_tm;
    strptime(cnt->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(cnt->lt, "%Y%m%dT%H%M%S", &lt_tm);
    strptime(cnt->et, "%Y%m%dT%H%M%S", &et_tm);
    char ct_iso[30], lt_iso[30], et_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
    sqlite3_bind_text(stmt, 8, et_iso, strlen(et_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, lt_iso, strlen(lt_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, cnt->url, strlen(cnt->url), SQLITE_STATIC);

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
            if (insert_multivalue_elements(db, cnt->ri, array_keys[i], array_keys[i], atr_array) == FALSE) {
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

    printf("CNT data inserted successfully.\n");

    return TRUE;
}

cJSON *cnt_to_json(const CNTStruct *cnt) {
    cJSON *innerObject = cJSON_CreateObject();
    cJSON_AddStringToObject(innerObject, "apn", cnt->apn);
    cJSON_AddStringToObject(innerObject, "ct", cnt->ct);
    cJSON_AddNumberToObject(innerObject, "ty", cnt->ty);
    cJSON_AddStringToObject(innerObject, "ri", cnt->ri);
    cJSON_AddStringToObject(innerObject, "rn", cnt->rn);
    cJSON_AddStringToObject(innerObject, "pi", cnt->pi);
    cJSON_AddStringToObject(innerObject, "aa", cnt->aa);
    cJSON_AddStringToObject(innerObject, "cnti", cnt->cnti);
    cJSON_AddStringToObject(innerObject, "api", cnt->api);
    cJSON_AddStringToObject(innerObject, "csz", cnt->csz);
    cJSON_AddStringToObject(innerObject, "et", cnt->et);
    cJSON_AddStringToObject(innerObject, "nl", cnt->nl);
    cJSON_AddStringToObject(innerObject, "or", cnt->or);
    cJSON_AddStringToObject(innerObject, "lt", cnt->lt);

    short rr_bool = 0;
    if (strcmp(cnt->rr, "true") == 0) {
        rr_bool = 1;
    } else {
        rr_bool = 0;
    }
    cJSON_AddBoolToObject(innerObject, "rr", rr_bool);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {cnt->json_acpi, cnt->json_lbl, cnt->json_daci, cnt->json_poa, cnt->json_ch, cnt->json_at};

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

    // Create the outer JSON object with the key "m2m:cnt" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:cnt", innerObject);

    return root;
}

char update_cnt(struct Route* destination, cJSON *content, char** response){
    const char *allowed_keysMULTIVALUE[] = {"apn", "acpi", "lbl", "daci", "poa", "ch", "aa", "csz", "nl", "at", "or"};
    size_t num_allowed_keysMULTIVALUE = sizeof(allowed_keysMULTIVALUE) / sizeof(allowed_keysMULTIVALUE[0]);

    // retrieve the CNT from tge database
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, cnti, api, rr, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    if (sql == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
        return FALSE;
    }
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Failed to initialize the database.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to initialize the database.");
        sqlite3_free(sql);
        return FALSE;
    }
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Populate the CNT
    CNTStruct *cnt = init_cnt();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cnt->ty = sqlite3_column_int(stmt, 0);
        strncpy(cnt->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(cnt->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(cnt->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        strncpy(cnt->cnti, (char *)sqlite3_column_text(stmt, 4), 10);
        strncpy(cnt->api, (char *)sqlite3_column_text(stmt, 5), 20);
        strncpy(cnt->rr, (char *)sqlite3_column_text(stmt, 6), 5);
        const char *et_iso = (const char *)sqlite3_column_text(stmt, 7);
        const char *ct_iso = (const char *)sqlite3_column_text(stmt, 8);
        const char *lt_iso = (const char *)sqlite3_column_text(stmt, 9);
        struct tm ct_tm, lt_tm, et_tm;
        strptime(ct_iso, "%Y-%m-%d %H:%M:%S", &ct_tm);
        strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
        strptime(et_iso, "%Y-%m-%d %H:%M:%S", &et_tm);
        char ct_str[20], lt_str[20], et_str[20];
        strftime(ct_str, sizeof(ct_str), "%Y%m%dT%H%M%S", &ct_tm);
        strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
        strftime(et_str, sizeof(et_str), "%Y%m%dT%H%M%S", &et_tm);
        strncpy(cnt->ct, ct_str, 20);
        strncpy(cnt->lt, lt_str, 20);
        strncpy(cnt->et, et_str, 20);
        break;
    }

    // Update the MTC table
    char *updateQueryMTC = sqlite3_mprintf("UPDATE mtc SET "); //string to create query

    int num_keys = cJSON_GetArraySize(content); //size of my body content
    const char *key; //to get the key(s) of my content
    const char *valueCNT; //to confirm the value already store in my CNT
    char my_string[100]; //to convert destination->ty
    cJSON *item; //to get the values of my content MTC
    cJSON *itemMULTI; //to get the values of my content MULTIVALUE

    struct tm parsed_time;
    time_t datetime_timestamp, current_time;
    char* errMsg = NULL;
    // Clear the struct to avoid garbage values
    memset(&parsed_time, 0, sizeof(parsed_time));

    rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        responseMessage(response, 500, "Internal Server Error", "Can't begin transaction.");
        closeDatabase(db);
        return FALSE;
    }
    
    for (int i = 0; i < num_keys; i++) {
        key = cJSON_GetArrayItem(content, i)->string;

        printf("key: %s\n", key);
        char isMTCAtr = FALSE;

        if (strcmp(key, "rr") == 0) {
            valueCNT = cnt->rr;
            isMTCAtr = TRUE;
        } else if (strcmp(key, "et") == 0){
            valueCNT = cnt->et;
            isMTCAtr = TRUE;
        } else if (strcmp(key, "apn") == 0){
            valueCNT = cnt->apn;
        } else if (strcmp(key, "nl") == 0){
            valueCNT = cnt->nl;
        } else if (strcmp(key, "or") == 0){
            valueCNT = cnt->or;
        } else if (strcmp(key, "aa") == 0){
            valueCNT = cnt->aa;
        } else if (strcmp(key, "csz") == 0){ 
            valueCNT = cnt->csz;
        } else if (strcmp(key, "acpi") == 0) { 
            valueCNT = cnt->json_acpi;
        } else if (strcmp(key, "lbl") == 0){
            valueCNT = cnt->json_lbl;
        }else if (strcmp(key, "daci") == 0){
            valueCNT = cnt->json_daci;
        }else if (strcmp(key, "poa") == 0){
            valueCNT = cnt->json_poa;
        }else if (strcmp(key, "ch") == 0){
            valueCNT = cnt->json_ch;
        } else if (strcmp(key, "at") == 0){
            valueCNT = cnt->json_at;
        } else {
            responseMessage(response, 400, "Bad Request", "Invalid key");
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        // Get the JSON string associated to the "key" from the content of JSON Body
        item = cJSON_GetObjectItemCaseSensitive(content, key);
        char *json_strITEM = cJSON_Print(item);

        // validate if the value from the JSON Body is the same as the DB Table
        if (!cJSON_IsArray(item)) {
            if (strcmp(json_strITEM, valueCNT) == 0) { //if the content is equal ignore
                printf("Continue \n");
                continue;
            }
        }

        if (isMTCAtr == TRUE && strcmp(key, "et") != 0) {
            updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, cJSON_GetArrayItem(content, i)->valuestring);
        }
        
        // Add the expiration time into the update statement
        if(strcmp(key, "et") == 0) {
            struct tm et_tm;
            char *new_json_stringET = strdup(json_strITEM); // create a copy of json_strITEM

            // remove the last character from new_json_stringET
            new_json_stringET += 1; //REMOVE "" from the value
            new_json_stringET[strlen(new_json_stringET) - 1] = '\0';

            // Verificar se a data está no formato correto
            char *parse_result = strptime(new_json_stringET, "%Y%m%dT%H%M%S", &et_tm);
            if (parse_result == NULL) {
                // The date string did not match the expected format
                responseMessage(response, 400, "Bad Request", "Invalid date format");
                sqlite3_finalize(stmt);
                closeDatabase(db);
                return FALSE;
            }

            // Convert the parsed time to a timestamp
            datetime_timestamp = mktime(&et_tm);
            // Get the current time
            current_time = time(NULL);

            // Compara o timestamp atual com o timestamp recebido, caso o timestamp está no passado dá excepção
            if (difftime(datetime_timestamp, current_time) < 0) {
                responseMessage(response, 400, "Bad Request", "Expiration time is in the past");
                sqlite3_finalize(stmt);
                closeDatabase(db);
                return FALSE;
            }

            char et_iso[20];
            strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
            updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, et_iso);
        }

        for(int k = 0; k < num_allowed_keysMULTIVALUE; k++){
            if(strcmp(key, allowed_keysMULTIVALUE[k]) == 0) { // as keys sao da tabela multivalue
                itemMULTI = cJSON_GetObjectItemCaseSensitive(content, key);
                char *json_strITEMULti = cJSON_Print(itemMULTI);
                
                char* sql_multi = sqlite3_mprintf("DELETE FROM multivalue WHERE mtc_ri='%q' AND atr='%s'", destination->ri, key);
                rc = sqlite3_exec(db, sql_multi, NULL, NULL, &errMsg);
                sqlite3_free(sql_multi);

                if (rc != SQLITE_OK) {
                    responseMessage(response,400,"Bad Request","Error deleting record");
                    fprintf(stderr, "Error deleting record: %s\n", errMsg);
                    rollback_transaction(db); // Rollback transaction
                    sqlite3_free(errMsg);
                    closeDatabase(db);
                    return FALSE;
                }
                // insert here
                if (!insert_multivalue_element(item, destination->ri, 0, key, key, db)) {
                    responseMessage(response,500,"Internal Server Error","Error inserting record");
                    fprintf(stderr, "Error inserting record");
                    rollback_transaction(db); // Rollback transaction
                    sqlite3_free(errMsg);
                    closeDatabase(db);
                    return FALSE;
                }
            }
        }

    }

    // Se tiver o valor inical não é feito o update
    if(strcmp(updateQueryMTC, "UPDATE mtc SET ") != 0){

        updateQueryMTC = sqlite3_mprintf("%slt = %Q WHERE ri = %Q AND ty = %d", updateQueryMTC, getCurrentTimeLong(),destination->ri, destination->ty);
    
        printf("UPDATE: %s\n", updateQueryMTC);

        rc = sqlite3_exec(db, updateQueryMTC, NULL, NULL, &errMsg);
        if (rc != SQLITE_OK) {
            rollback_transaction(db);
            responseMessage(response,400,"Bad Request","Error updating");
            printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
            sqlite3_free(errMsg);
            closeDatabase(db);
            return FALSE;
        }

        rc = commit_transaction(db);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Can't commit transaction\n");
            sqlite3_free(errMsg);
            closeDatabase(db);
            return FALSE;
        }
        
        // Retrieve the CNT with the updated expiration time
        sql = sqlite3_mprintf("SELECT rr, et, lt FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
        printf("%s\n", sql);

        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            rollback_transaction(db);
            printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            responseMessage(response,400,"Bad Request","Error running select"); 
            sqlite3_finalize(stmt);
            sqlite3_free(errMsg);
            closeDatabase(db);
            return FALSE;
        }

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            strncpy(cnt->rr, (char *)sqlite3_column_text(stmt, 0), 5);
            const char *et_iso = (const char *)sqlite3_column_text(stmt, 1);
            const char *lt_iso = (const char *)sqlite3_column_text(stmt, 2);
            struct tm lt_tm, et_tm;
            strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
            strptime(et_iso, "%Y-%m-%d %H:%M:%S", &et_tm);
            char lt_str[20], et_str[20];
            strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
            strftime(et_str, sizeof(et_str), "%Y%m%dT%H%M%S", &et_tm);
            strncpy(cnt->lt, lt_str, 20);
            strncpy(cnt->et, et_str, 20);
            break;
        }
    }
    
    cJSON* root = cnt_to_json(cnt);
    if (root == NULL) {
        fprintf(stderr, "Failed to convert CNTStruct to JSON.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    num_keys = sizeof(keys) / sizeof(keys[0]);
    add_arrays_to_json(db, cnt->ri, cJSON_GetObjectItem(root, "m2m:cnt"), keys, num_keys);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        cJSON_Delete(root);
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }
    char * response_data = json_str;
    
    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(response_data) + 1;
    
    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        sqlite3_finalize(stmt);
        closeDatabase(db);
        cJSON_Delete(root);
        free(json_str);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", response_data);

    free(json_str);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    return TRUE;
}

char get_cnt(struct Route* destination, char** response){
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, cnti, api, rr, et, lt, ct FROM mtc WHERE LOWER(url) = LOWER('%s');", destination->key);

    if (sql == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        return FALSE;
    }
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Failed to initialize the database.\n");
        sqlite3_free(sql);
        return FALSE;
    }
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    printf("Creating the json object\n");
    CNTStruct *cnt = init_cnt();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        cnt->ty = sqlite3_column_int(stmt, 0);
        strncpy(cnt->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(cnt->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(cnt->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        strncpy(cnt->cnti, (char *)sqlite3_column_text(stmt, 4), 10);
        strncpy(cnt->api, (char *)sqlite3_column_text(stmt, 5), 20);
        strncpy(cnt->rr, (char *)sqlite3_column_text(stmt, 6), 5);
        const char *et_iso = (const char *)sqlite3_column_text(stmt, 7);
        const char *ct_iso = (const char *)sqlite3_column_text(stmt, 8);
        const char *lt_iso = (const char *)sqlite3_column_text(stmt, 9);
        struct tm ct_tm, lt_tm, et_tm;
        strptime(ct_iso, "%Y-%m-%d %H:%M:%S", &ct_tm);
        strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
        strptime(et_iso, "%Y-%m-%d %H:%M:%S", &et_tm);
        char ct_str[20], lt_str[20], et_str[20];
        strftime(ct_str, sizeof(ct_str), "%Y%m%dT%H%M%S", &ct_tm);
        strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
        strftime(et_str, sizeof(et_str), "%Y%m%dT%H%M%S", &et_tm);
        strncpy(cnt->ct, ct_str, 20);
        strncpy(cnt->lt, lt_str, 20);
        strncpy(cnt->et, et_str, 20);
        break;
    }

    cJSON* root = cnt_to_json(cnt);
    if (root == NULL) {
        fprintf(stderr, "Failed to convert CNTStruct to JSON.\n");
        responseMessage(response, 400, "Bad Request", "Failed to convert CNTStruct to JSON.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    int num_keys = sizeof(keys) / sizeof(keys[0]);
    add_arrays_to_json(db, cnt->ri, cJSON_GetObjectItem(root, "m2m:cnt"), keys, num_keys);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        responseMessage(response, 400, "Bad Request", "Failed to print JSON as a string.\n");
        cJSON_Delete(root);
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char * response_data = json_str;

    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(response_data) + 1;
    
    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        sqlite3_finalize(stmt);
        closeDatabase(db);
        cJSON_Delete(root);
        free(json_str);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", response_data);

    printf("ola%s\n", *response);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);
    return TRUE;
}
