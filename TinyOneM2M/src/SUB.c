/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafsubl Pereira (Rafsubl_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include "Common.h"

extern int DAYS_PLUS_ET;

SUBStruct *init_sub() {
    SUBStruct *sub = (SUBStruct *) malloc(sizeof(SUBStruct));
    if (sub) {
        sub->ty = SUB;
        sub->ri[0] = '\0';
        sub->rn[0] = '\0';
        sub->pi[0] = '\0';
        sub->et[0] = '\0';
        sub->lt[0] = '\0';
        sub->json_lbl = NULL;
        sub->json_acpi = NULL;
        sub->json_daci = NULL;
        sub->url = NULL;
        sub->enc[0] = '\0';
        sub->json_nu = NULL;
        sub->blob = NULL;
    }
    return sub;
}

char create_sub(SUBStruct * sub, cJSON *content, char** response) {
    
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
    const char *query = "SELECT COALESCE(MAX(CAST(substr(ri, 5) AS INTEGER)), 0) + 1 as result FROM mtc WHERE ty = 23";
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
        size_t ri_size = snprintf(NULL, 0, "CSUB%d", result) + 1; // Get the required size for the ri string

        ri = malloc(ri_size * sizeof(char)); // Allocate dynamic memory for the ri string
        if (ri == NULL) {
            fprintf(stderr, "Failed to allocate memory for ri\n");
            closeDatabase(db);
            return FALSE;
        }
        snprintf(ri, ri_size, "CSUB%d", result); // Write the formatted string to ri
        cJSON_AddStringToObject(content, "ri", ri);
    } else {
        fprintf(stderr, "Failed to fetch the result\n");
        closeDatabase(db);
        return FALSE;
    }
    sub->ty = SUB;
    strcpy(sub->ri, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring);
    strcpy(sub->rn, cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    strcpy(sub->pi, cJSON_GetObjectItemCaseSensitive(content, "pi")->valuestring);
    strcpy(sub->enc, cJSON_GetObjectItemCaseSensitive(content, "enc")->valuestring);
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
        strcpy(sub->et, et->valuestring);
    } else {
        strcpy(sub->et, get_datetime_days_later(DAYS_PLUS_ET));
    }
    strcpy(sub->ct, getCurrentTime());
    strcpy(sub->lt, sub->ct);
    
    const char *keys[] = {"acpi", "lbl", "daci", "nu"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < num_keys; i++) {
        cJSON *json_array = cJSON_GetObjectItemCaseSensitive(content, keys[i]);
        if (json_array) {
            char *json_str = cJSON_Print(json_array);
            if (json_str) {
                size_t len = strlen(json_str);
                if(strcmp(keys[i], "acpi") == 0){
                    sub->json_acpi = (char *)malloc(len+1);
                    strcpy(sub->json_acpi, json_str);
                }
                if(strcmp(keys[i],"lbl") == 0){
                    sub->json_lbl = (char *)malloc(len+1);
                    strcpy(sub->json_lbl, json_str);
                }
                if(strcmp(keys[i],"daci") == 0){
                    sub->json_daci = (char *)malloc(len+1);
                    strcpy(sub->json_daci, json_str);
                }
                if(strcmp(keys[i],"nu") == 0){
                    sub->json_nu = (char *)malloc(len+1);
                    strcpy(sub->json_nu, json_str);
                } 
            }           
        }else if (json_array == NULL){
            cJSON *empty_array = cJSON_CreateArray();
            size_t len = strlen(cJSON_Print(empty_array));
            if(strcmp(keys[i],"acpi") == 0){
                sub->json_acpi = (char *)malloc(len+1);
                strcpy(sub->json_acpi, cJSON_Print(empty_array));
            }
            if(strcmp(keys[i],"lbl") == 0){
                sub->json_lbl = (char *)malloc(len+1);
                strcpy(sub->json_lbl, cJSON_Print(empty_array));
            }
            if(strcmp(keys[i],"daci") == 0){
                sub->json_daci = (char *)malloc(len+1);
                strcpy(sub->json_daci, cJSON_Print(empty_array));
            }
            if(strcmp(keys[i],"nu") == 0){
                sub->json_nu = (char *)malloc(len+1);
                strcpy(sub->json_nu, cJSON_Print(empty_array));
            }  
        }       
    }
    
    //blob
    size_t rnLengthBlob = strlen(cJSON_Print(sub_to_json(sub)));
    sub->blob = (char *)malloc(rnLengthBlob);
    if (sub->blob == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }
    strcpy(sub->blob, cJSON_Print(sub_to_json(sub)));  

    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }
    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, et, ct, lt, url, blob, acpi, lbl, daci, nu, enc) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)";
    
    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr,"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        closeDatabase(db);
        return FALSE;
    }
    
    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, sub->ty);
    sqlite3_bind_text(stmt, 2, sub->ri, strlen(sub->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, sub->rn, strlen(sub->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, sub->pi, strlen(sub->pi), SQLITE_STATIC);
    struct tm ct_tm, lt_tm, et_tm;
    strptime(sub->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(sub->lt, "%Y%m%dT%H%M%S", &lt_tm);
    strptime(sub->et, "%Y%m%dT%H%M%S", &et_tm);
    char ct_iso[30], lt_iso[30], et_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
    sqlite3_bind_text(stmt, 5, et_iso, strlen(et_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, lt_iso, strlen(lt_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 8, sub->url, strlen(sub->url), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 9, sub->blob, strlen(sub->blob), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, sub->json_acpi, strlen(sub->json_acpi), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, sub->json_lbl, strlen(sub->json_lbl), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, sub->json_daci, strlen(sub->json_daci), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, sub->json_nu, strlen(sub->json_nu), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, sub->enc, strlen(sub->enc), SQLITE_STATIC);
    
    
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

    char *sql_not = sqlite3_mprintf("SELECT DISTINCT nu, url, enc FROM mtc WHERE LOWER(pi) = LOWER('%s') AND nu IS NOT NULL AND et > DATETIME('now');", sub->pi);
    if (sql_not == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
        return FALSE;
    }
    rc = sqlite3_prepare_v2(db, sql_not, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Populate the CNT
    pthread_t thread_id;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        notificationData* data = malloc(sizeof(notificationData));
        pthread_t thread_id;
        int result;

        const char *nu_temp = (const char *)sqlite3_column_text(stmt, 0);
        data->nu = malloc(strlen(nu_temp) + 1); // +1 for null terminator
        strcpy(data->nu, nu_temp);

        const char *url_temp = (const char *)sqlite3_column_text(stmt, 1);
        data->topic = malloc(strlen(url_temp) + 1); // +1 for null terminator
        strcpy(data->topic, url_temp);

        data->body = malloc(strlen(sub->blob) + 1); // +1 for null terminator
        strcpy(data->body, sub->blob);

        const char *enc_temp = (const char *)sqlite3_column_text(stmt, 2);
        // Check if the subscription eventNotificationCriteria contains "POST"
        if (strstr(enc_temp, "POST") == NULL) {
            continue;
        }

        char *suffix = "}}";

        int suffix_length = strlen(suffix);
        int body_length = strlen(data->body);
        int enc_temp_length = strlen(enc_temp); // assuming enc_temp is a string
        int topic_length = strlen(data->topic);

        // Here we construct the prefix dynamically with sprintf. 
        char prefix[256];  // Make sure this size is enough for your string
        sprintf(prefix, "{\"m2m:sgn\":{\"cr\":\"admin:admin\",\"nev\":{\"net\":\"%s\",\"om\":null,\"rep\":", "POST");
        
        int prefix_length = strlen(prefix);
        int total_length = prefix_length + body_length + enc_temp_length + topic_length + strlen(suffix) + 201;
        // Allocate enough memory for the new string
        char *wrapped_body = malloc(total_length);

        if(wrapped_body == NULL) {
            fprintf(stderr, "Failed to allocate memory for the wrapped body.\n");
            free(data->nu); // Free the memory for the string
            free(data->topic); // Free the memory for the string
            free(data->body); // Free the memory for the string
            free(data); // Then free the memory for the struct
            continue;
        } else {
            // Start with the prefix
            strcpy(wrapped_body, prefix);
            // Append the original body
            strcat(wrapped_body, data->body);
            // Append the topic
            strcat(wrapped_body, ",\"nfu\":null,\"sud\":null,\"sur\":\"");
            strcat(wrapped_body, data->topic);
            strcat(wrapped_body, "\",\"vrq\":null}");
            // Append the suffix
            strcat(wrapped_body, suffix);

            // Free the original body now that it's not needed
            free(data->body);
            // Make the wrapped body the new body
            data->body = wrapped_body;
        }
        
        result = pthread_create(&thread_id, NULL, send_notification, data); //pass data, not &data
        if (result != 0) {
            fprintf(stderr, "Error creating thread: %s\n", strerror(result));
            free(data->nu); // Free the memory for the string
            free(data->topic); // Free the memory for the string
            free(data->body); // Free the memory for the string
            free(data); // Then free the memory for the struct
        }
    }

    closeDatabase(db);
    printf("SUB data inserted successfully.\n");
    return TRUE;
}

cJSON *sub_to_json(const SUBStruct *sub) {
    cJSON *innerObject = cJSON_CreateObject();
    cJSON_AddStringToObject(innerObject, "ct", sub->ct);
    cJSON_AddNumberToObject(innerObject, "ty", sub->ty);
    cJSON_AddStringToObject(innerObject, "ri", sub->ri);
    cJSON_AddStringToObject(innerObject, "rn", sub->rn);
    cJSON_AddStringToObject(innerObject, "pi", sub->pi);
    cJSON_AddStringToObject(innerObject, "et", sub->et);
    cJSON_AddStringToObject(innerObject, "lt", sub->lt);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"acpi", "lbl", "daci", "nu"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {sub->json_acpi, sub->json_lbl, sub->json_daci, sub->json_nu};

    for (int i = 0; i < num_keys; i++) {
        if (json_strings[i] != NULL) {
            cJSON *json_object = cJSON_Parse(json_strings[i]);
            if (json_object) {
                cJSON_AddItemToObject(innerObject, keys[i], json_object);
            }
            else{
                fprintf(stderr, "Failed to parse JSON string for key '%s' back into cJSON object\n", keys[i]);
            }
        } else {
            cJSON *empty_array = cJSON_CreateArray();
            cJSON_AddItemToObject(innerObject, keys[i], empty_array);
        }
    }

    cJSON_AddStringToObject(innerObject, "enc", sub->enc);

    // Create the outer JSON object with the key "m2m:sub" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:sub", innerObject);
    return root;
}

char update_sub(struct Route* destination, cJSON *content, char** response){
    // retrieve the SUB from tge database
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, ct, lt, acpi, lbl, daci, nu, enc FROM mtc WHERE ri = '%s' AND ty = %d AND et > datetime('now');", destination->ri, SUB);
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

    // Populate the SUB
    SUBStruct *sub = init_sub();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        sub->ty = sqlite3_column_int(stmt, 0);
        strncpy(sub->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(sub->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(sub->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        const char *et_iso = (const char *)sqlite3_column_text(stmt, 4);
        const char *ct_iso = (const char *)sqlite3_column_text(stmt, 5);
        const char *lt_iso = (const char *)sqlite3_column_text(stmt, 6);
        struct tm ct_tm, lt_tm, et_tm;
        strptime(ct_iso, "%Y-%m-%d %H:%M:%S", &ct_tm);
        strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
        strptime(et_iso, "%Y-%m-%d %H:%M:%S", &et_tm);
        char ct_str[20], lt_str[20], et_str[20];
        strftime(ct_str, sizeof(ct_str), "%Y%m%dT%H%M%S", &ct_tm);
        strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
        strftime(et_str, sizeof(et_str), "%Y%m%dT%H%M%S", &et_tm);
        strncpy(sub->ct, ct_str, 20);
        strncpy(sub->lt, lt_str, 20);
        strncpy(sub->et, et_str, 20);
        size_t len = strlen((char *)sqlite3_column_text(stmt, 7));
        sub->json_acpi = (char *)malloc(len+1);
        strcpy(sub->json_acpi, (char *)sqlite3_column_text(stmt, 7));
        
        len = strlen((char *)sqlite3_column_text(stmt, 8));
        sub->json_lbl = (char *)malloc(len+1);
        strcpy(sub->json_lbl, (char *)sqlite3_column_text(stmt, 8));

        len = strlen((char *)sqlite3_column_text(stmt, 9));
        sub->json_daci = (char *)malloc(len+1);
        strcpy(sub->json_daci, (char *)sqlite3_column_text(stmt, 9));

        len = strlen((char *)sqlite3_column_text(stmt, 10));
        sub->json_nu = (char *)malloc(len+1);
        strcpy(sub->json_nu, (char *)sqlite3_column_text(stmt, 10));

        strncpy(sub->enc, (char *)sqlite3_column_text(stmt, 11), 50);
        break;
    }

    // Update the MTC table
    char *updateQueryMTC = sqlite3_mprintf("UPDATE mtc SET "); //string to create query

    int num_keys = cJSON_GetArraySize(content); //size of my body content
    const char *key; //to get the key(s) of my content
    const char *valueSUB; //to confirm the value already store in my SUB
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
            valueSUB = sub->et;
            strcpy(sub->et, json_strITEM);
        } else if (strcmp(key, "enc") == 0){
            valueSUB = sub->enc;
            strcpy(sub->enc, item->valuestring);
        } else if (strcmp(key, "acpi") != 0 && strcmp(key, "lbl") != 0 && strcmp(key, "daci") != 0 && strcmp(key, "nu") != 0){
            responseMessage(response, 400, "Bad Request", "Invalid key");
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        // validate if the value from the JSON Body is the same as the DB Table
        if (!cJSON_IsArray(item)) {
            if (strcmp(json_strITEM, valueSUB) == 0) { //if the content is equal ignore
                printf("Continue \n");
                continue;
            }
        }else{
            if (json_strITEM) {
                size_t len = strlen(json_strITEM);
                if(strcmp(key, "acpi") == 0){
                    sub->json_acpi = (char *)malloc(len+1);
                    strcpy(sub->json_acpi, json_strITEM);
                }
                if(strcmp(key, "lbl") == 0){
                    sub->json_lbl = (char *)malloc(len+1);
                    strcpy(sub->json_lbl, json_strITEM);
                }
                if(strcmp(key, "daci") == 0){
                    sub->json_daci = (char *)malloc(len+1);
                    strcpy(sub->json_daci, json_strITEM);
                }
                if(strcmp(key, "nu") == 0){
                    sub->json_nu = (char *)malloc(len+1);
                    strcpy(sub->json_nu, json_strITEM);
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
            strcpy(sub->et, et_iso);
        }
        else {
            if(cJSON_IsArray(item)){
                updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, json_strITEM);
            }else{
                updateQueryMTC = sqlite3_mprintf("%s%s = %Q, ",updateQueryMTC, key, cJSON_GetArrayItem(content, i)->valuestring);
            }
        }
    }
    strcpy(sub->lt,getCurrentTime());
    //blob
    size_t rnLengthBlob = strlen(cJSON_Print(sub_to_json(sub)));
    sub->blob = (char *)malloc(rnLengthBlob);
    if (sub->blob == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }
    strcpy(sub->blob, cJSON_Print(sub_to_json(sub)));  
    // Se tiver o valor inical não é feito o update
    if(strcmp(updateQueryMTC, "UPDATE mtc SET ") != 0){

        updateQueryMTC = sqlite3_mprintf("%slt = %Q, blob = \'%s\' WHERE ri = %Q AND ty = %d", updateQueryMTC, getCurrentTimeLong(), sub->blob,destination->ri, destination->ty);

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
        
        // Retrieve the SUB with the updated expiration time
        sql = sqlite3_mprintf("SELECT et, lt FROM mtc WHERE ri = '%s' AND ty = %d AND et > datetime('now');", destination->ri, destination->ty);

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
            const char *et_iso = (const char *)sqlite3_column_text(stmt, 0);
            const char *lt_iso = (const char *)sqlite3_column_text(stmt, 1);
            struct tm lt_tm, et_tm;
            strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
            strptime(et_iso, "%Y-%m-%d %H:%M:%S", &et_tm);
            char lt_str[20], et_str[20];
            strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
            strftime(et_str, sizeof(et_str), "%Y%m%dT%H%M%S", &et_tm);
            strncpy(sub->lt, lt_str, 20);
            strncpy(sub->et, et_str, 20);
            break;
        }
    }
    
    char *json_str = cJSON_Print(sub_to_json(sub));
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

    char *sql_not = sqlite3_mprintf("SELECT DISTINCT nu, url, enc FROM mtc WHERE LOWER(pi) = LOWER('%s') AND nu IS NOT NULL AND et > DATETIME('now');", sub->pi);
    if (sql_not == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
        return FALSE;
    }
    rc = sqlite3_prepare_v2(db, sql_not, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Populate the CNT
    pthread_t thread_id;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        notificationData* data = malloc(sizeof(notificationData));
        pthread_t thread_id;
        int result;

        const char *nu_temp = (const char *)sqlite3_column_text(stmt, 0);
        data->nu = malloc(strlen(nu_temp) + 1); // +1 for null terminator
        strcpy(data->nu, nu_temp);

        const char *url_temp = (const char *)sqlite3_column_text(stmt, 1);
        data->topic = malloc(strlen(url_temp) + 1); // +1 for null terminator
        strcpy(data->topic, url_temp);

        data->body = malloc(strlen(sub->blob) + 1); // +1 for null terminator
        strcpy(data->body, sub->blob);

        const char *enc_temp = (const char *)sqlite3_column_text(stmt, 2);
        // Check if the subscription eventNotificationCriteria contains "PUT"
        if (strstr(enc_temp, "PUT") == NULL) {
            continue;
        }

        char *suffix = "}}";

        int suffix_length = strlen(suffix);
        int body_length = strlen(data->body);
        int enc_temp_length = strlen(enc_temp); // assuming enc_temp is a string
        int topic_length = strlen(data->topic);

        // Here we construct the prefix dynamically with sprintf. 
        char prefix[256];  // Make sure this size is enough for your string
        sprintf(prefix, "{\"m2m:sgn\":{\"cr\":\"admin:admin\",\"nev\":{\"net\":\"%s\",\"om\":null,\"rep\":", "PUT");
        
        int prefix_length = strlen(prefix);
        int total_length = prefix_length + body_length + enc_temp_length + topic_length + strlen(suffix) + 201;
        // Allocate enough memory for the new string
        char *wrapped_body = malloc(total_length);

        if(wrapped_body == NULL) {
            fprintf(stderr, "Failed to allocate memory for the wrapped body.\n");
            free(data->nu); // Free the memory for the string
            free(data->topic); // Free the memory for the string
            free(data->body); // Free the memory for the string
            free(data); // Then free the memory for the struct
            continue;
        } else {
            // Start with the prefix
            strcpy(wrapped_body, prefix);
            // Append the original body
            strcat(wrapped_body, data->body);
            // Append the topic
            strcat(wrapped_body, ",\"nfu\":null,\"sud\":null,\"sur\":\"");
            strcat(wrapped_body, data->topic);
            strcat(wrapped_body, "\",\"vrq\":null}");
            // Append the suffix
            strcat(wrapped_body, suffix);

            // Free the original body now that it's not needed
            free(data->body);
            // Make the wrapped body the new body
            data->body = wrapped_body;
        }
        
        result = pthread_create(&thread_id, NULL, send_notification, data); //pass data, not &data
        if (result != 0) {
            fprintf(stderr, "Error creating thread: %s\n", strerror(result));
            free(data->nu); // Free the memory for the string
            free(data->topic); // Free the memory for the string
            free(data->body); // Free the memory for the string
            free(data); // Then free the memory for the struct
        }
    }

    closeDatabase(db);
    return TRUE;
}

char get_sub(struct Route* destination, char** response){
    char *sql = sqlite3_mprintf("SELECT blob, pi FROM mtc WHERE LOWER(url) = LOWER('%s') AND et > datetime('now');", destination->key);

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
    
    char *response_data = NULL;
    char *blob = NULL;
    char *pi = NULL;
    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        response_data = (char *)sqlite3_column_text(stmt, 0); // note the change in index to 0
        blob = malloc(strlen(response_data));
        strcpy(blob, response_data);
        pi = malloc(strlen((char *)sqlite3_column_text(stmt, 1)));
        strcpy(pi, (char *)sqlite3_column_text(stmt, 1));
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

    if (blob != NULL) {
        char *sql_not = sqlite3_mprintf("SELECT DISTINCT nu, url, enc FROM mtc WHERE LOWER(pi) = LOWER('%s') AND nu IS NOT NULL AND et > DATETIME('now');", pi);
        if (sql_not == NULL) {
            fprintf(stderr, "Failed to allocate memory for SQL query.\n");
            responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
            return FALSE;
        }
        rc = sqlite3_prepare_v2(db, sql_not, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }
        
        // Populate the CNT
        pthread_t thread_id;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            notificationData* data = malloc(sizeof(notificationData));
            pthread_t thread_id;
            int result;

            const char *nu_temp = (const char *)sqlite3_column_text(stmt, 0);
            data->nu = malloc(strlen(nu_temp) + 1); // +1 for null terminator
            strcpy(data->nu, nu_temp);

            const char *url_temp = (const char *)sqlite3_column_text(stmt, 1);
            data->topic = malloc(strlen(url_temp) + 1); // +1 for null terminator
            strcpy(data->topic, url_temp);

            data->body = malloc(strlen(blob) + 1); // +1 for null terminator
            strcpy(data->body, blob);

            const char *enc_temp = (const char *)sqlite3_column_text(stmt, 2);
            // Check if the subscription eventNotificationCriteria contains "GET"
            if (strstr(enc_temp, "GET") == NULL) {
                continue;
            }

            char *suffix = "}}";

            int suffix_length = strlen(suffix);
            int body_length = strlen(data->body);
            int enc_temp_length = strlen(enc_temp); // assuming enc_temp is a string
            int topic_length = strlen(data->topic);

            // Here we construct the prefix dynamically with sprintf. 
            char prefix[256];  // Make sure this size is enough for your string
            sprintf(prefix, "{\"m2m:sgn\":{\"cr\":\"admin:admin\",\"nev\":{\"net\":\"%s\",\"om\":null,\"rep\":", "GET");
            
            int prefix_length = strlen(prefix);
            int total_length = prefix_length + body_length + enc_temp_length + topic_length + strlen(suffix) + 201;
            // Allocate enough memory for the new string
            char *wrapped_body = malloc(total_length);

            if(wrapped_body == NULL) {
                fprintf(stderr, "Failed to allocate memory for the wrapped body.\n");
                free(data->nu); // Free the memory for the string
                free(data->topic); // Free the memory for the string
                free(data->body); // Free the memory for the string
                free(data); // Then free the memory for the struct
                continue;
            } else {
                // Start with the prefix
                strcpy(wrapped_body, prefix);
                // Append the original body
                strcat(wrapped_body, data->body);
                // Append the topic
                strcat(wrapped_body, ",\"nfu\":null,\"sud\":null,\"sur\":\"");
                strcat(wrapped_body, data->topic);
                strcat(wrapped_body, "\",\"vrq\":null}");
                // Append the suffix
                strcat(wrapped_body, suffix);

                // Free the original body now that it's not needed
                free(data->body);
                // Make the wrapped body the new body
                data->body = wrapped_body;
            }

            result = pthread_create(&thread_id, NULL, send_notification, data); //pass data, not &data
            if (result != 0) {
                fprintf(stderr, "Error creating thread: %s\n", strerror(result));
                free(data->nu); // Free the memory for the string
                free(data->topic); // Free the memory for the string
                free(data->body); // Free the memory for the string
                free(data); // Then free the memory for the struct
            }
        }
    }

    closeDatabase(db);
    return TRUE;
}
