/*
 * Created on Mon Apr 24 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
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
        cnt->ct[0] = '\0';
        cnt->ty = CNT;
        cnt->json_acpi = NULL;
        cnt->et[0] = '\0';
        cnt->json_lbl = NULL;
        cnt->pi[0] = '\0';
        cnt->json_daci = NULL;
        cnt->json_ch = NULL;
        cnt->aa[0] = '\0';
        cnt->rn[0] = '\0';
        cnt->ri[0] = '\0';
        cnt->json_at = NULL;
        cnt->or[0] = '\0';
        cnt->lt[0] = '\0';
        cnt->blob = NULL;
        cnt->st = 0;
        cnt->mbs = -1;
        cnt->mni = -1;
        cnt->cni = 0;
        cnt->cbs = 0;
        cnt->li = -1;
        cnt->dr = -1;
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
    const char *query = "SELECT COALESCE(MAX(CAST(substr(ri, 5) AS INTEGER)), 0) + 1 as result FROM mtc WHERE ty = 3";
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
    cnt->mbs = cJSON_GetObjectItemCaseSensitive(content, "mbs")->valueint;
    cnt->mni = cJSON_GetObjectItemCaseSensitive(content, "mni")->valueint;
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
    strcpy(cnt->lt, cnt->ct);

    const char *keys[] = {"acpi", "lbl", "daci"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < num_keys; i++) {
        cJSON *json_array = cJSON_GetObjectItemCaseSensitive(content, keys[i]);
        if (json_array) {
            char *json_str = cJSON_Print(json_array);
            if (json_str) {
                size_t len = strlen(json_str);
                if(strcmp(keys[i],"acpi") == 0){
                    cnt->json_acpi = (char *)malloc(len+1);
                    strcpy(cnt->json_acpi, json_str);
                }
                if(strcmp(keys[i],"lbl") == 0){
                    cnt->json_lbl = (char *)malloc(len+1);
                    strcpy(cnt->json_lbl, json_str);
                }
                if(strcmp(keys[i],"daci") == 0){
                    cnt->json_daci = (char *)malloc(len+1);
                    strcpy(cnt->json_daci, json_str);
                }
            }           
        }else if (json_array == NULL){
            cJSON *empty_array = cJSON_CreateArray();
            size_t len = strlen(cJSON_Print(empty_array));
            if(strcmp(keys[i],"acpi") == 0){
                cnt->json_acpi = (char *)malloc(len+1);
                strcpy(cnt->json_acpi, cJSON_Print(empty_array));
            }
            if(strcmp(keys[i],"lbl") == 0){
                cnt->json_lbl = (char *)malloc(len+1);
                strcpy(cnt->json_lbl, cJSON_Print(empty_array));
            }
            if(strcmp(keys[i],"daci") == 0){
                cnt->json_daci = (char *)malloc(len+1);
                strcpy(cnt->json_daci, cJSON_Print(empty_array));
            }
        }       
    }
    
    //blob
    content = cnt_to_json(cnt);
    size_t rnLengthBlob = strlen(cJSON_Print(content));
    cnt->blob = (char *)malloc(rnLengthBlob);
    if (cnt->blob == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }
    strcpy(cnt->blob, cJSON_Print(cnt_to_json(cnt)));  
    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }
    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, st, mni, mbs, cni, cbs, et, ct, lt, url, blob, acpi, lbl, daci) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";

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
    sqlite3_bind_int(stmt, 5, cnt->st);
    if (cnt->mni != -1) sqlite3_bind_int(stmt, 6, cnt->mni); else sqlite3_bind_null(stmt, 6);
    if (cnt->mbs != -1) sqlite3_bind_int(stmt, 7, cnt->mbs); else sqlite3_bind_null(stmt, 7);
    sqlite3_bind_int(stmt, 8, cnt->cni);
    sqlite3_bind_int(stmt, 9, cnt->cbs);
    struct tm ct_tm, lt_tm, et_tm;
    strptime(cnt->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(cnt->lt, "%Y%m%dT%H%M%S", &lt_tm);
    strptime(cnt->et, "%Y%m%dT%H%M%S", &et_tm);
    char ct_iso[30], lt_iso[30], et_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
    sqlite3_bind_text(stmt, 10, et_iso, strlen(et_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, lt_iso, strlen(lt_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, cnt->url, strlen(cnt->url), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, cnt->blob, strlen(cnt->blob), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 15, cnt->json_acpi, strlen(cnt->json_acpi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 16, cnt->json_lbl, strlen(cnt->json_lbl), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 17, cnt->json_daci, strlen(cnt->json_daci), SQLITE_STATIC);
   
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
    cJSON_AddStringToObject(innerObject, "ct", cnt->ct);
    cJSON_AddNumberToObject(innerObject, "ty", cnt->ty);
    cJSON_AddStringToObject(innerObject, "ri", cnt->ri);
    cJSON_AddStringToObject(innerObject, "rn", cnt->rn);
    cJSON_AddStringToObject(innerObject, "pi", cnt->pi);
    cJSON_AddStringToObject(innerObject, "aa", cnt->aa);
    cJSON_AddNumberToObject(innerObject, "st", cnt->st);

    if (cnt->mni != -1) cJSON_AddNumberToObject(innerObject, "mni", cnt->mni); else cJSON_AddNullToObject(innerObject, "mni");
    if (cnt->mbs != -1) cJSON_AddNumberToObject(innerObject, "mbs", cnt->mbs); else cJSON_AddNullToObject(innerObject, "mbs");

    cJSON_AddNumberToObject(innerObject, "cni", cnt->cni);
    cJSON_AddNumberToObject(innerObject, "cbs", cnt->cbs);

    cJSON_AddStringToObject(innerObject, "et", cnt->et);
    cJSON_AddStringToObject(innerObject, "or", cnt->or);
    cJSON_AddStringToObject(innerObject, "lt", cnt->lt);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"acpi", "lbl", "daci", "ch", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {cnt->json_acpi, cnt->json_lbl, cnt->json_daci, cnt->json_ch, cnt->json_at};

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
    // retrieve the CNT from the database
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, st, mni, mbs, cni, cbs, et, ct, lt, acpi, lbl, daci FROM mtc WHERE url = '%s';", destination->key);
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
        cnt->st = sqlite3_column_int(stmt, 4);
        cnt->mni = sqlite3_column_int(stmt, 5);
        cnt->mbs = sqlite3_column_int(stmt, 6);
        cnt->cni = sqlite3_column_int(stmt, 7);
        cnt->cbs = sqlite3_column_int(stmt, 8);
        const char *et_iso = (const char *)sqlite3_column_text(stmt, 9);
        const char *ct_iso = (const char *)sqlite3_column_text(stmt, 10);
        const char *lt_iso = (const char *)sqlite3_column_text(stmt, 11);
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
    char *valueCNT; //to confirm the value already store in my CNT
    char my_string[100]; //to convert destination->ty
    cJSON *item; //to get the values of my content MTC

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
        // Get the JSON string associated to the "key" from the content of JSON Body
        item = cJSON_GetObjectItemCaseSensitive(content, key);
        char *json_strITEM = cJSON_Print(item);
        // remove a few 
        if (strcmp(key, "et") == 0){
            valueCNT = cnt->et;
            strcpy(cnt->et, json_strITEM);
        } else if (strcmp(key, "or") == 0){
            valueCNT = cnt->or;
            strcpy(cnt->or, json_strITEM);
        } else if (strcmp(key, "mni") == 0){
            valueCNT = malloc(strlen(json_strITEM)+1);
            snprintf(valueCNT, strlen(json_strITEM)+1, "%d", cnt->mni);
            cnt->mni = atoi(json_strITEM);
        }   else if (strcmp(key, "mbs") == 0){
            valueCNT = malloc(strlen(json_strITEM)+1);
            snprintf(valueCNT, strlen(json_strITEM)+1, "%d", cnt->mbs);
            cnt->mbs = atoi(json_strITEM);
        } else if (strcmp(key, "aa") == 0){
            valueCNT = cnt->aa;
            strcpy(cnt->aa, json_strITEM);
        } else if (strcmp(key, "acpi") != 0 && strcmp(key, "lbl") != 0 && strcmp(key, "daci") != 0 && strcmp(key, "ch") != 0 && strcmp(key, "at") != 0 && strcmp(key, "or") != 0 && strcmp(key, "dr") != 0){
            responseMessage(response, 400, "Bad Request", "Invalid key");
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        // validate if the value from the JSON Body is the same as the DB Table
        if (!cJSON_IsArray(item)) {
            if (strcmp(json_strITEM, valueCNT) == 0) { //if the content is equal ignore
                printf("Continue \n");
                continue;
            }
        }else{
            if (json_strITEM) {
                size_t len = strlen(json_strITEM);
                if(strcmp(key, "acpi") == 0){
                    cnt->json_acpi = (char *)malloc(len+1);
                    strcpy(cnt->json_acpi, json_strITEM);
                }
                if(strcmp(key, "lbl") == 0){
                    cnt->json_lbl = (char *)malloc(len+1);
                    strcpy(cnt->json_lbl, json_strITEM);
                }
                if(strcmp(key, "daci") == 0){
                    cnt->json_daci = (char *)malloc(len+1);
                    strcpy(cnt->json_daci, json_strITEM);
                }           
            }
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
            strcpy(cnt->et, et_iso);
        }
        else{
            if(cJSON_IsArray(item)){
                updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, json_strITEM);
            }else{
                updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, cJSON_GetArrayItem(content, i)->valuestring);
            }
        }
    }

    // Se tiver o valor inical não é feito o update
    if(strcmp(updateQueryMTC, "UPDATE mtc SET ") != 0){

        strcpy(cnt->lt,getCurrentTime());

        cnt->st = cnt->st + 1;

        //blob
        size_t rnLengthBlob = strlen(cJSON_Print(cnt_to_json(cnt)));
        cnt->blob = (char *)malloc(rnLengthBlob);
        if (cnt->blob == NULL) {
            // Handle memory allocation error
            fprintf(stderr, "Memory allocation error\n");
            return FALSE;
        }
        strcpy(cnt->blob, cJSON_Print(cnt_to_json(cnt)));  

        updateQueryMTC = sqlite3_mprintf("%slt = %Q, st = %d, blob = \'%s\' WHERE url = %Q", updateQueryMTC, cnt->lt, cnt->st, cnt->blob, destination->key);
    
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
    }
    
    
    char *json_str = cJSON_Print(cnt_to_json(cnt));
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
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
        free(json_str);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", response_data);

    free(json_str);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    return TRUE;
}

char get_cnt(struct Route* destination, char** response){
    char *sql = sqlite3_mprintf("SELECT blob FROM mtc WHERE LOWER(url) = LOWER('%s');", destination->key);

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

    // Copy the blob from the resource to the response_data
    char *response_data = NULL;
    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        response_data = (char *)sqlite3_column_text(stmt, 0); // note the change in index to 0
    } else {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        responseMessage(response, 400, "Bad Request", "Failed to print JSON as a string.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

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
        free(response_data);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", response_data);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    return TRUE;
}