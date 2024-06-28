/*
 * Filename: MTC_Protocol.c
 * Created Date: Monday, March 27th 2023, 5:24:39 pm
 * Author: Rafael Pereira (rafael_pereira_2000@hotmail.com), 
 *         Carla Mendes (carlasofiamendes@outlook.com),
 *         Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include "Common.h"

char init_protocol(struct Route** head) {

    char rs = init_types();
    if (rs == FALSE) {
        fprintf(stderr, "Error initializing types.\n");
        return FALSE;
    }

    // Sqlite3 initialization opening/creating database
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Error initializing database.\n");
        return FALSE;
    }

    // Initialize the mutex
    pthread_mutex_t db_mutex;
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
        fprintf(stderr, "Error initializing mutex.\n");
        closeDatabase(db);
        return FALSE;
    }

    // Perform database operations
    pthread_mutex_lock(&db_mutex);

    // Check if the cse_base exists
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='mtc';", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare query: %s\n", sqlite3_errmsg(db));
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        closeDatabase(db);
        return FALSE;
    }

    CSEBaseStruct *csebase = init_cse_base();
    if (csebase == NULL) {
        fprintf(stderr, "Error allocating memory for CSEBaseStruct.\n");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        // If table doesn't exist, we create it and populate it
        printf("The table does not exist.\n");
        sqlite3_finalize(stmt);
        
        char rs = create_cse_base(csebase, FALSE);
        if (rs == FALSE) {
            fprintf(stderr, "Error initializing CSE base.\n");
            pthread_mutex_unlock(&db_mutex);
            pthread_mutex_destroy(&db_mutex);
            closeDatabase(db);
            free(csebase);
            return FALSE;
        }
    } else {
        sqlite3_finalize(stmt);

        // Check if the table has any data
        sqlite3_stmt *stmt;
        char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM mtc WHERE ty = %d ORDER BY ROWID DESC LIMIT 1;", CSEBASE);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_free(sql);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to prepare 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            pthread_mutex_unlock(&db_mutex);
            pthread_mutex_destroy(&db_mutex);
            closeDatabase(db);
            free(csebase);
            return FALSE;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            fprintf(stderr, "Failed to execute 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            pthread_mutex_unlock(&db_mutex);
            pthread_mutex_destroy(&db_mutex);
            closeDatabase(db);
            free(csebase);
            return FALSE;
        }

        int rowCount = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);

        if (rowCount == 0) {
            printf("The mtc table doesn't have CSE_Base resources.\n");

            char rs = create_cse_base(csebase, TRUE);
            if (rs == FALSE) {
                fprintf(stderr, "Error initializing CSE base.\n");
                pthread_mutex_unlock(&db_mutex);
                pthread_mutex_destroy(&db_mutex);
                closeDatabase(db);
                free(csebase);
                return FALSE;
            }
        } else {
            printf("CSE_Base already inserted.\n");
            // In case of the table and data exists, get the
            char rs = getLastCSEBaseStruct(csebase, db);
            if (rs == FALSE) {
                fprintf(stderr, "Error getting last CSE base structure.\n");
                pthread_mutex_unlock(&db_mutex);
                pthread_mutex_destroy(&db_mutex);
                closeDatabase(db);
                free(csebase);
                return FALSE;
            }
        }
    }

    // Access database here
    pthread_mutex_unlock(&db_mutex);
    pthread_mutex_destroy(&db_mutex);

    // Add New Routes
    const int URI_BUFFER_SIZE = 60;
    char uri[URI_BUFFER_SIZE];
    snprintf(uri, sizeof(uri), "/%s", csebase->rn);
    to_lowercase(uri);
    addRoute(head, uri, csebase->ri, csebase->ty, csebase->rn);

    // The DB connection should exist in each thread and should not be shared
    if (closeDatabase(db) == FALSE) {
        fprintf(stderr, "Error closing database.\n");
        return FALSE;
    }

    return TRUE;
}

char retrieve_csebase(struct Route * destination, char **response) {
    char *sql = sqlite3_mprintf("SELECT blob FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not open the database");
        return FALSE;
    }
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 500, "Internal Server Error", "Failed to prepare statement");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    printf("Creating the json object\n");
    CSEBaseStruct *csebase = init_cse_base();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        
        char *blob = (char *)sqlite3_column_text(stmt, 0);
        csebase->blob = (char *)malloc(strlen(blob)+1);
        strcpy(csebase->blob, blob);
        break;
    }
    
    char *json_str = csebase->blob;
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }
    
    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(json_str) + 1;
    
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
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", json_str);

    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);

    return TRUE;
}

char discovery(struct Route *head, struct Route *destination, const char *queryString, char **response) {
    // Initialize the database
    struct sqlite3 *db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not open the database");
        return FALSE;
    }

    char *query_copy = NULL;
    char *query_copy2 = NULL;

    if (queryString != NULL) {
        query_copy = strdup(queryString);
        if (query_copy == NULL) {
            responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
            closeDatabase(db);
            return FALSE;
        }

        query_copy2 = strdup(queryString);
        if (query_copy2 == NULL) {
            responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
            sqlite3_free(query_copy);
            closeDatabase(db);
            return FALSE;
        }
    } else {
        responseMessage(response, 400, "Bad Request", "No query string provided");
        closeDatabase(db);
        return FALSE;
    }

    // Define condition strings for different types of conditions
    char *MVconditions = NULL;
        char *MTCconditions = NULL;

    // Define arrays of allowed non-array keys, array keys, and time keys
    const char *keysNA[] = {"ty"};
    const char *keysA[] = {"lbl"};
    const char *keysT[] = {"createdbefore", "createdafter", "modifiedsince", "unmodifiedsince", "expirebefore", "expireafter"};

    size_t keysNA_len = sizeof(keysNA) / sizeof(keysNA[0]);
    size_t keysA_len = sizeof(keysA) / sizeof(keysA[0]);
    size_t keysT_len = sizeof(keysT) / sizeof(keysT[0]);

    short limit = 50;

    char *saveptr_fo;

    // Tokenize the query string
    char *token = strtok_r(query_copy, "&", &saveptr_fo);

    // Determine filter operation
    char filter_operation[4] = ""; // Allocate a char array to store the filter operation
    while (token != NULL) {
        char *key = strtok(token, "=");
        char *value = strtok(NULL, "=");
        if (strcmp(key, "filteroperation") == 0) {
            strncpy(filter_operation, value, sizeof(filter_operation) - 1); // Copy the value to filter_operation using strncpy to avoid buffer overflow
            filter_operation[sizeof(filter_operation) - 1] = '\0'; // Ensure the string is null-terminated
            break;
        }
        token = strtok_r(NULL, "&", &saveptr_fo);
    }

    // Set default filter operation if not specified
        if (strcmp(filter_operation, "") == 0) {
        strncpy(filter_operation, "AND", sizeof(filter_operation) - 1);
    }

    // Create the conditions string
    char *conditions = "";
    char *saveptr;
    char *token2 = strtok_r(query_copy2, "&", &saveptr);

    while (token2 != NULL) {
        char *key = strtok(token2, "=");
        char *value = strtok(NULL, "=");

        if (strcmp(key, "filteroperation") == 0) {
            token2 = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Skip if fu=1
        if (strcmp(key, "fu") == 0 && strcmp(value, "1") == 0) {
            token2 = strtok_r(NULL, "&", &saveptr);
            continue;
        }

                if (strcmp(key, "limit") == 0 && is_number(value)) {
            limit = atoi(value);

            token2 = strtok_r(NULL, "&", &saveptr);
            continue;
        }

        // Check if the key is in one of the key arrays
        int found = 0;

        // Check non-array keys
        if (key_in_array(key, keysNA, keysNA_len)) {
            found = 1;
            // Add the condition to the query string
            char *condition;
            if (strcmp(key, "ty") == 0) {  // Integer key
                condition = sqlite3_mprintf("%s %s = %Q", filter_operation, key, value);
            } else {  // String key
                condition = sqlite3_mprintf("%s %s LIKE \"%%%s%%\"", filter_operation, key, value);
            }
            
            char *newMTCconditions = sqlite3_mprintf("%s%s ", MTCconditions ? MTCconditions : "", condition);
            if (MTCconditions) sqlite3_free(MTCconditions);
            
            MTCconditions = newMTCconditions;
            sqlite3_free(condition);
        }

        // Check array keys
        if (!found && key_in_array(key, keysA, keysA_len)) {
            found = 1;
            // Add the condition to the query string
            char *newMVconditions = sqlite3_mprintf("%s %s EXISTS (SELECT 1 FROM mtc WHERE %Q = %Q)", MVconditions ? MVconditions : "", filter_operation, key, value);
            if (MVconditions) sqlite3_free(MVconditions);
            MVconditions = newMVconditions;
        }

        // Check time keys
        if (!found && key_in_array(key, keysT, keysT_len)) {
            found = 1;
            // Extract the date/time information from the value
            struct tm tm;
            strptime(value, "%Y%m%dt%H%M%S", &tm); // Correct format to parse the input datetime string

            // Convert the struct tm to the desired format
            char *formatted_datetime = (char *)calloc(20, sizeof(char)); // Allocate enough space for the resulting string
            strftime(formatted_datetime, 20, "%Y-%m-%d %H:%M:%S", &tm);

            // Add a condition to the SQL query
            char *condition;
            if (strstr(key, "before") != NULL) {
                if (strstr(key, "created") != NULL) {
                    condition = sqlite3_mprintf("%s ct < %Q", filter_operation, formatted_datetime);
                } else if (strstr(key, "expire") != NULL) {
                    condition = sqlite3_mprintf("%s et < %Q", filter_operation, formatted_datetime);
                }
            } else if (strstr(key, "after") != NULL) {
                if (strstr(key, "created") != NULL) {
                    condition = sqlite3_mprintf("%s ct > %Q", filter_operation, formatted_datetime);
                } else if (strstr(key, "expire") != NULL) {
                    condition = sqlite3_mprintf("%s et > %Q", filter_operation, formatted_datetime);
                }
            } else if (strstr(key, "since") != NULL) {
                if (strstr(key, "modified") != NULL) {
                    condition = sqlite3_mprintf("%s lt >= %Q", filter_operation, formatted_datetime);
                } else if (strstr(key, "unmodified") != NULL) {
                    condition = sqlite3_mprintf("%s lt <= %Q", filter_operation, formatted_datetime);
                }
            }

            char *newMTCconditions = sqlite3_mprintf("%s%s ", MTCconditions ? MTCconditions : "", condition);
            if (MTCconditions) sqlite3_free(MTCconditions);

            MTCconditions = newMTCconditions;
            sqlite3_free(condition);
            free(formatted_datetime);
        }

        // If key not found in any of the arrays, return an error
        if (!found) {
            fprintf(stderr, "Invalid key: %s\n", key);
            responseMessage(response, 400, "Bad Request", "Invalid key");
            sqlite3_free(query_copy);
            sqlite3_free(query_copy2);
            closeDatabase(db);
            return FALSE;
        }

        token2 = strtok_r(NULL, "&", &saveptr);
    }

    // Remove the first AND/OR from MTCconditions
    if (MTCconditions != NULL) {
        char *pos = strstr(MTCconditions, filter_operation);
        if (pos == MTCconditions) {
            memmove(MTCconditions, pos + strlen(filter_operation), strlen(pos + strlen(filter_operation)) + 1);
        }

        if (strcmp(MTCconditions, "") != 0) {
            MTCconditions = sqlite3_mprintf(" AND (%s)", MTCconditions);
        }
    }

    // Execute the dynamic SELECT statement
    char *query = sqlite3_mprintf("SELECT * FROM (SELECT url FROM mtc WHERE ri = \"%s\" AND 1 = 1%s LIMIT %d) "
                              "UNION "
                              "SELECT mtc.url FROM mtc WHERE mtc.pi = \"%s\" AND mtc.et > datetime('now') AND 1=1%s LIMIT %d",
                              destination->ri,
                              MTCconditions ? MTCconditions : "",
                              limit,
                              destination->ri,
                              MTCconditions ? MTCconditions : "",
                              limit);

    printf("%s\n",query);

    // Free the temporary strings
    if (MVconditions) sqlite3_free(MVconditions);
    if (MTCconditions) sqlite3_free(MTCconditions);

    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
        fprintf(stderr, "Cannot prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Cannot prepare statement");
        sqlite3_free(query_copy);
        sqlite3_free(query_copy2);
        closeDatabase(db);
        return FALSE;
    }

    cJSON *root = cJSON_CreateObject();
    cJSON *uril_array = cJSON_CreateArray();
    cJSON_AddItemToObject(root, "m2m:uril", uril_array);

    // Iterate over the result set and add the MTC_RI values to the cJSON array
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *mtc_url = (const char *)sqlite3_column_text(stmt, 0);

        cJSON *mtc_ri_value = cJSON_CreateString(mtc_url);
        cJSON_AddItemToArray(uril_array, mtc_ri_value);
    }

    sqlite3_finalize(stmt);
    closeDatabase(db);
    if (query_copy) free(query_copy);
    if (query_copy2) free(query_copy2);
    if (query) sqlite3_free(query);

    printf("Convert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        fprintf(stderr, "Failed to convert cJSON object to a JSON string\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to convert cJSON object to a JSON string");
        // Cleanup
        sqlite3_finalize(stmt);
        closeDatabase(db);
        if (query_copy) free(query_copy);
        if (query_copy2) free(query_copy2);
        if (MVconditions) sqlite3_free(MVconditions);
        if (MTCconditions) sqlite3_free(MTCconditions);
        if (query) sqlite3_free(query);
        cJSON_Delete(root);
        return FALSE;
    }
    char *response_data = json_str;
    
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
        if (query_copy) free(query_copy);
        if (query_copy2) free(query_copy2);
        if (MVconditions) sqlite3_free(MVconditions);
        if (MTCconditions) sqlite3_free(MTCconditions);
        if (query) sqlite3_free(query);
        cJSON_Delete(root);
        free(json_str);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", response_data);

    // Cleanup
    cJSON_Delete(root);
    free(json_str);

    return TRUE;
}

char post_ae(struct Route** head, struct Route* destination, cJSON *content, char** response) {
    
    // JSON Validation
    
    // "rn" is an optional, but if dont come with it we need to generate a resource name
    char *keys_rn[] = {"rn"};  // Resource Name
    int num_keys = 1;  // number of keys in the array
    char *aux_response = NULL;
    char rs = validate_keys(content, keys_rn, num_keys, &aux_response);
    if (rs == FALSE) {
        // Se não tiver "rn" geramos um com "AE-<UniqueID>"
        char unique_id[MAX_CONFIG_LINE_LENGTH];
        generate_unique_id(unique_id);

        char unique_name[MAX_CONFIG_LINE_LENGTH+3];
        snprintf(unique_name, sizeof(unique_name), "AE-%s", unique_id);
        cJSON_AddStringToObject(content, "rn", unique_name);
    } else {
        cJSON *rn_item = cJSON_GetObjectItem(content, "rn");
        if (rn_item == NULL || !cJSON_IsString(rn_item)) {
            printf("Error: RN not found or is not a string\n");
            responseMessage(response, 400, "Bad Request", "Error: RN not found or is not a string");
            return FALSE;
        }

        // Remove unauthorized chars
        remove_unauthorized_chars(rn_item->valuestring);
    }

    // Mandatory Atributes
    char *keys_m[] = {"api", "rr"};  // App-ID, requestReachability
    num_keys = 2;  // number of keys in the array
    aux_response = NULL;
    rs = validate_keys(content, keys_m, num_keys, &aux_response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", aux_response);
        return FALSE;
    }

    const char *allowed_keys[] = {"apn", "acpi", "et", "lbl", "daci", "poa", "ch", "aa", "aei", "rn", "api", "rr", "csz", "nl", "at", "or"};
    size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }
    
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s",destination->value);
    cJSON *value_rn = cJSON_GetObjectItem(content, "rn");
    if (value_rn == NULL) {
        responseMessage(response, 400, "Bad Request", "rn (resource name) key not found");
        return FALSE;
    }
    char temp_uri[60];
    int result = snprintf(temp_uri, sizeof(temp_uri), "%s/%s", uri, value_rn->valuestring);
    if (result < 0 || result >= sizeof(temp_uri)) {
        responseMessage(response, 400, "Bad Request", "URI is too long");
        return FALSE;
    }
    
    // Copy the result from the temporary buffer to the uri buffer
    strncpy(uri, temp_uri, sizeof(uri));
    uri[sizeof(uri) - 1] = '\0'; // Ensure null termination
    to_lowercase(uri);

    pthread_mutex_t db_mutex;

    // initialize mutex
     if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }

    printf("Creating AE\n");
    AEStruct *ae = init_ae();
    if (ae == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not initialize AE");
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);

    size_t destinationKeyLength = strlen(destination->key);
    size_t rnLength = strlen(cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);

    // Allocate memory for ae->url, considering the extra characters for "/", and the null terminator.
    ae->url = (char *)malloc(destinationKeyLength + rnLength + 2);

    // Check if memory allocation is successful
    if (ae->url == NULL) {
        // Handle memory allocation error
        responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
        fprintf(stderr, "Memory allocation error\n");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        free(ae);
        return FALSE;
    }

    // Copy the destination key into ae->url
    strncpy(ae->url, destination->key, destinationKeyLength);
    ae->url[destinationKeyLength] = '\0'; // Add null terminator

    // Append "/<rn value>" to ae->url
    sprintf(ae->url + destinationKeyLength, "/%s", cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    if (search(*head, ae->url) != NULL) {
        responseMessage(response, 409, "Conflict", "Resource already exists (Skipping)");
        free(ae->url);
        free(ae);
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    clock_t start_time, end_time;
    double elapsed_time;
    // Record the start time
    start_time = clock();

    rs = create_ae(ae, content, response);

    // Record the end time
    end_time = clock();

    // Calculate the elapsed time in seconds
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Time taken by create_ae: %f seconds\n", elapsed_time);
    if (rs == FALSE) {
        // É feito dentro da função create_ae
        free(ae->url);
        free(ae);
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Add New Routes
    to_lowercase(uri);
    addRoute(head, uri, ae->ri, ae->ty, ae->rn);
    printf("New Route: %s -> %s -> %d -> %s \n", uri, ae->ri, ae->ty, ae->rn);

    // Convert the AE struct to json and the Json Object to Json String
    cJSON *root = ae_to_json(ae);
    char *str = cJSON_PrintUnformatted(root);
    if (str == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not print cJSON object");
        cJSON_Delete(root);
        free(ae->url);
        free(ae);
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(str) + 1;

    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        cJSON_free(str);
        cJSON_Delete(root);
        free(ae->url);
        free(ae);
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", str);

    // Free allocated resources
    cJSON_Delete(root);
    cJSON_free(str);

    // // access database here
    pthread_mutex_unlock(&db_mutex);

    // // clean up
    pthread_mutex_destroy(&db_mutex);

    // Free the AE struct and its members
    free(ae->url);
    free(ae);

    return TRUE;
}

char post_cnt(struct Route** head, struct Route* destination, cJSON *content, char** response) {
    
    // JSON Validation
    
    // "rn" is an optional, but if dont come with it we need to generate a resource name
    char *keys_rn[] = {"rn"};  // Resource Name
    int num_keys = 1;  // number of keys in the array
    char *aux_response = NULL;
    char rs = validate_keys(content, keys_rn, num_keys, &aux_response);
    if (rs == FALSE) {
        // Se não tiver "rn" geramos um com "CNT-<UniqueID>"
        char unique_id[MAX_CONFIG_LINE_LENGTH];
        generate_unique_id(unique_id);

        char unique_name[MAX_CONFIG_LINE_LENGTH+4];
        snprintf(unique_name, sizeof(unique_name), "CNT-%s", unique_id);
        cJSON_AddStringToObject(content, "rn", unique_name);
    } else {
        cJSON *rn_item = cJSON_GetObjectItem(content, "rn");
        if (rn_item == NULL || !cJSON_IsString(rn_item)) {
            printf("Error: RN not found or is not a string\n");
            responseMessage(response, 400, "Bad Request", "Error: RN not found or is not a string");
            return FALSE;
        }

        // Remove unauthorized chars
        remove_unauthorized_chars(rn_item->valuestring);
    }

    cJSON *value = NULL;
    value = cJSON_GetObjectItem(content, "mbs");  // retrieve the value associated with the key
    if (value == NULL) {
        cJSON_AddNumberToObject(content, "mbs", -1);
    }

    value = cJSON_GetObjectItem(content, "mni");  // retrieve the value associated with the key
    if (value == NULL) {
        cJSON_AddNumberToObject(content, "mni", -1);
    }

    // Theres no Mandatory Atributes
    // Não consta no excel auxiliar: dr -> disableRetrieval
    const char *allowed_keys[] = {"rn", "et", "acpi", "lbl", "at", "aa", "daci", "cr", "mni", "mbs", "mia", "li", "or", "dr"};
    size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }

    // Default values
    cJSON_AddNumberToObject(content, "st", 0);
    cJSON_AddNumberToObject(content, "cni", 0);
    cJSON_AddNumberToObject(content, "cbs", 0);
    
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s",destination->value);
    cJSON *value_rn = cJSON_GetObjectItem(content, "rn");
    if (value_rn == NULL) {
        responseMessage(response, 400, "Bad Request", "rn (resource name) key not found");
        return FALSE;
    }
    char temp_uri[60];
    int result = snprintf(temp_uri, sizeof(temp_uri), "%s/%s", uri, value_rn->valuestring);
    if (result < 0 || result >= sizeof(temp_uri)) {
        responseMessage(response, 400, "Bad Request", "URI is too long");
        return FALSE;
    }
    
    // Copy the result from the temporary buffer to the uri buffer
    strncpy(uri, temp_uri, sizeof(uri));
    uri[sizeof(uri) - 1] = '\0'; // Ensure null termination
    to_lowercase(uri);

    pthread_mutex_t db_mutex;

    // initialize mutex
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
        responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
        return FALSE;
    }

    printf("Creating CNT\n");
    CNTStruct *cnt = init_cnt();
    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    size_t destinationKeyLength = strlen(destination->key);
    size_t rnLength = strlen(cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);

    // Allocate memory for cnt->url, considering the extra characters for "/", and the null terminator.
    cnt->url = (char *)malloc(destinationKeyLength + rnLength + 2);

    // Check if memory allocation is successful
    if (cnt->url == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Copy the destination key into cnt->url
    strncpy(cnt->url, destination->key, destinationKeyLength);
    cnt->url[destinationKeyLength] = '\0'; // Add null terminator

    // Append "/<rn value>" to cnt->url
    sprintf(cnt->url + destinationKeyLength, "/%s", cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    to_lowercase(cnt->url);
    if (search(*head, cnt->url) != NULL) {
        responseMessage(response, 409, "Conflict", "Resource already exists (Skipping)");
        return FALSE;
    }

    rs = create_cnt(cnt, content, response);

    if (rs == FALSE) {
        // É feito dentro da função create_cnt
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    
    // Add New Routes
    addRoute(head, cnt->url, cnt->ri, cnt->ty, cnt->rn);
    printf("New Route: %s -> %s -> %d -> %s \n", cnt->url, cnt->ri, cnt->ty, cnt->rn);

    // Creating ol(dest) and la(test) routes
    char *url_ol;
    char *url_la;

    // Allocate memory for the new URLs
    url_ol = malloc(strlen(cnt->url) + strlen("/ol") + 1);  // +1 for the null-terminator
    url_la = malloc(strlen(cnt->url) + strlen("/la") + 1);  // +1 for the null-terminator

    if (url_ol == NULL || url_la == NULL) {
        fprintf(stderr, "Memory allocation failed. \n'la' and 'li' CNT routes not available\n");
        responseMessage(response, 500, "Internal Server Error", "Memory allocation failed. 'la' and 'li' CNT routes not available");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Copy the original URL and append /ol and /la
    sprintf(url_ol, "%s/ol", cnt->url);
    sprintf(url_la, "%s/la", cnt->url);

    addRoute(head, url_ol, cnt->ri, CIN, "ol");
    addRoute(head, url_la, cnt->ri, CIN, "la");

    // Convert the CNT struct to json and the Json Object to Json String
    cJSON *root = cnt_to_json(cnt);
    char *str = cJSON_PrintUnformatted(root);
    if (str == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not print cJSON object");
        cJSON_Delete(root);
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    
    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(str) + 1;
    
    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        cJSON_free(str);
        cJSON_Delete(root);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", str);

    // Free allocated resources
    cJSON_Delete(root);
    cJSON_free(str);

    // // access database here
    pthread_mutex_unlock(&db_mutex);

    // // clean up
    pthread_mutex_destroy(&db_mutex);

    free(url_ol);
    free(url_la);

    return TRUE;
}

char post_cin(struct Route** head, struct Route* destination, cJSON *content, char** response) {
    
    // JSON Validation
    
    // "rn" is an optional, but if dont come with it we need to generate a resource name
    char *keys_rn[] = {"rn"};  // Resource Name
    int num_keys = 1;  // number of keys in the array
    char *aux_response = NULL;
    char rs = validate_keys(content, keys_rn, num_keys, &aux_response);
    if (rs == FALSE) {
        // Se não tiver "rn" geramos um com "CIN-<UniqueID>"
        char unique_id[MAX_CONFIG_LINE_LENGTH];
        generate_unique_id(unique_id);

        char unique_name[MAX_CONFIG_LINE_LENGTH+4];
        snprintf(unique_name, sizeof(unique_name), "CIN-%s", unique_id);
        cJSON_AddStringToObject(content, "rn", unique_name);
    } else {
        cJSON *rn_item = cJSON_GetObjectItem(content, "rn");
        if (rn_item == NULL || !cJSON_IsString(rn_item)) {
            printf("Error: RN not found or is not a string\n");
            responseMessage(response, 400, "Bad Request", "Error: RN not found or is not a string");
            return FALSE;
        }

        // Remove unauthorized chars
        remove_unauthorized_chars(rn_item->valuestring);
    }
    
    // Optional value with default value
    cJSON *value = NULL;
    value = cJSON_GetObjectItem(content, "cnf");  // retrieve the value associated with the key
    if (value == NULL) {
        cJSON_AddStringToObject(content, "cnf", "text/plain:0");
    }
    
    // Mandatory Atributes
    char *keys_m[] = {"con"};  // Content
    num_keys = 1;  // number of keys in the array
    aux_response = NULL;
    rs = validate_keys(content, keys_m, num_keys, &aux_response);
    if (rs == FALSE) {
        cJSON_AddStringToObject(content, "con", "");
    }

    const char *allowed_keys[] = {"rn", "et", "at", "aa", "lbl", "cnf", "cr", "or", "con", "dc", "dgt"};
    size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }

    value = cJSON_GetObjectItem(content, "con");  // retrieve the value associated with the key
    if (value == NULL) {
        cJSON_AddNumberToObject(content, "cs", 0);
    } else {
        cJSON_AddNumberToObject(content, "cs", strlen(cJSON_GetObjectItem(content, "con")->valuestring));
    }
    
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s",destination->value);
    cJSON *value_rn = cJSON_GetObjectItem(content, "rn");
    if (value_rn == NULL) {
        responseMessage(response, 400, "Bad Request", "rn (resource name) key not found");
        return FALSE;
    }
    char temp_uri[60];
    int result = snprintf(temp_uri, sizeof(temp_uri), "%s/%s", uri, value_rn->valuestring);
    if (result < 0 || result >= sizeof(temp_uri)) {
        responseMessage(response, 400, "Bad Request", "URI is too long");
        return FALSE;
    }
    
    // Copy the result from the temporary buffer to the uri buffer
    strncpy(uri, temp_uri, sizeof(uri));
    uri[sizeof(uri) - 1] = '\0'; // Ensure null termination
    to_lowercase(uri);
    

    pthread_mutex_t db_mutex;

    // initialize mutex
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
        responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
        return FALSE;
    }

    printf("Creating CIN\n");
    CINStruct *cin = init_cin();
    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    size_t destinationKeyLength = strlen(destination->key);
    size_t rnLength = strlen(cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);

    // Allocate memory for cin->url, considering the extra characters for "/", and the null terminator.
    cin->url = (char *)malloc(destinationKeyLength + rnLength + 2);

    // Check if memory allocation is successful
    if (cin->url == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Copy the destination key into cin->url
    strncpy(cin->url, destination->key, destinationKeyLength);
    cin->url[destinationKeyLength] = '\0'; // Add null terminator
    
    // Append "/<rn value>" to cin->url
    sprintf(cin->url + destinationKeyLength, "/%s", cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    to_lowercase(cin->url);
    if (search(*head, cin->url) != NULL) {
        responseMessage(response, 409, "Conflict", "Resource already exists (Skipping)");
        return FALSE;
    }

    // retrieve the st from CNT from the database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Failed to initialize the database.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to initialize the database.");
        return FALSE;
    }
    char *sql = sqlite3_mprintf("SELECT st FROM mtc WHERE LOWER(url) = LOWER('%s') AND et > datetime('now');", destination->key);
    if (sql == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
        return FALSE;
    }
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Populate the st attribute
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        cJSON_AddNumberToObject(content, "st", sqlite3_column_int(stmt, 0));
    } else {
        printf("Failed to step through the statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to step through the statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    rs = create_cin(db, cin, content, response);

    if (rs == FALSE) {
        // É feito dentro da função create_cin
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    
    // Add New Routes
    addRoute(head, cin->url, cin->ri, cin->ty, cin->rn);
    printf("New Route: %s -> %s -> %d -> %s \n", uri, cin->ri, cin->ty, cin->rn);
    
    // Convert the CIN struct to json and the Json Object to Json String
    cJSON *root = cin_to_json(cin);
    char *str = cJSON_PrintUnformatted(root);
    if (str == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not print cJSON object");
        cJSON_Delete(root);
        return FALSE;
    }
    
    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(str) + 1;
    
    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        cJSON_free(str);
        cJSON_Delete(root);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", str);

    // Free allocated resources
    cJSON_Delete(root);
    cJSON_free(str);

    // // access database here
    pthread_mutex_unlock(&db_mutex);

    // // clean up
    pthread_mutex_destroy(&db_mutex);

    return TRUE;
}

char post_sub(struct Route** head, struct Route* destination, cJSON *content, char** response) {
    
    // JSON Validation
    
    // "rn" is an optional, but if dont come with it we need to generate a resource name
    char *keys_rn[] = {"rn"};  // Resource Name
    int num_keys = 1;  // number of keys in the array
    char *aux_response = NULL;
    char rs = validate_keys(content, keys_rn, num_keys, &aux_response);
    if (rs == FALSE) {
        // Se não tiver "rn" geramos um com "SUB-<UniqueID>"
        char unique_id[MAX_CONFIG_LINE_LENGTH];
        generate_unique_id(unique_id);

        char unique_name[MAX_CONFIG_LINE_LENGTH+4];
        snprintf(unique_name, sizeof(unique_name), "SUB-%s", unique_id);
        cJSON_AddStringToObject(content, "rn", unique_name);
    } else {
        cJSON *rn_item = cJSON_GetObjectItem(content, "rn");
        if (rn_item == NULL || !cJSON_IsString(rn_item)) {
            printf("Error: RN not found or is not a string\n");
            responseMessage(response, 400, "Bad Request", "Error: RN not found or is not a string");
            return FALSE;
        }

        // Remove unauthorized chars
        remove_unauthorized_chars(rn_item->valuestring);
    }

    // Mandatory Atributes
    char *keys_m[] = {"nu"};  // App-ID, requestReachability
    num_keys = 1;  // number of keys in the array
    aux_response = NULL;
    rs = validate_keys(content, keys_m, num_keys, &aux_response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", aux_response);
        return FALSE;
    }

    cJSON *value = NULL;
    value = cJSON_GetObjectItem(content, "enc");  // retrieve the value associated with the key
    if (value == NULL) {
        cJSON_AddStringToObject(content, "enc", "POST, PUT, GET, DELETE");
    }
    printf("%s\n", cJSON_Print(content));
    const char *allowed_keys[] = {"rn", "acpi", "et", "lbl", "daci", "nu", "enc"};
    size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }
    
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s",destination->value);
    cJSON *value_rn = cJSON_GetObjectItem(content, "rn");
    if (value_rn == NULL) {
        responseMessage(response, 400, "Bad Request", "rn (resource name) key not found");
        return FALSE;
    }
    char temp_uri[60];
    int result = snprintf(temp_uri, sizeof(temp_uri), "%s/%s", uri, value_rn->valuestring);
    if (result < 0 || result >= sizeof(temp_uri)) {
        responseMessage(response, 400, "Bad Request", "URI is too long");
        return FALSE;
    }
    
    // Copy the result from the temporary buffer to the uri buffer
    strncpy(uri, temp_uri, sizeof(uri));
    uri[sizeof(uri) - 1] = '\0'; // Ensure null termination

    pthread_mutex_t db_mutex;

    // initialize mutex
     if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }

    printf("Creating SUB\n");
    SUBStruct *sub = init_sub();
    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    size_t destinationKeyLength = strlen(destination->key);
    size_t rnLength = strlen(cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);

    // Allocate memory for sub->url, considering the extra characters for "/", and the null terminator.
    sub->url = (char *)malloc(destinationKeyLength + rnLength + 2);

    // Check if memory allocation is successful
    if (sub->url == NULL) {
        // Handle memory allocation error
        responseMessage(response, 500, "Internal Server Error", "Memory allocation error");
        fprintf(stderr, "Memory allocation error\n");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Copy the destination key into sub->url
    strncpy(sub->url, destination->key, destinationKeyLength);
    sub->url[destinationKeyLength] = '\0'; // Add null terminator

    // Append "/<rn value>" to sub->url
    sprintf(sub->url + destinationKeyLength, "/%s", cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    to_lowercase(sub->url);
    if (search(*head, sub->url) != NULL) {
        responseMessage(response, 409, "Conflict", "Resource already exists (Skipping)");
        return FALSE;
    }
    
    clock_t start_time, end_time;
    double elapsed_time;
    // Record the start time
    start_time = clock();

    rs = create_sub(sub, content, response);

    // Record the end time
    end_time = clock();

    // Calculate the elapsed time in seconds
    elapsed_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("Time taken by create_sub: %f seconds\n", elapsed_time);
    if (rs == FALSE) {
        // É feito dentro da função create_sub
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    
    // Add New Routes
    addRoute(head, sub->url, sub->ri, sub->ty, sub->rn);
    printf("New Route: %s -> %s -> %d -> %s \n", uri, sub->ri, sub->ty, sub->rn);
    
    // Convert the SUB struct to json and the Json Object to Json String
    cJSON *root = sub_to_json(sub);
    char *str = cJSON_PrintUnformatted(root);
    if (str == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not print cJSON object");
        cJSON_Delete(root);
        return FALSE;
    }
    
    // Calculate the required buffer size
    size_t response_size = strlen("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n") + strlen(str) + 1;
    
    // Allocate memory for the response buffer
    *response = (char *)malloc(response_size * sizeof(char));

    // Check if memory allocation was successful
    if (*response == NULL) {
        fprintf(stderr, "Failed to allocate memory for the response buffer\n");
        // Cleanup
        cJSON_free(str);
        cJSON_Delete(root);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", str);

    // Free allocated resources
    cJSON_Delete(root);
    cJSON_free(str);

    // // access database here
    pthread_mutex_unlock(&db_mutex);

    // // clean up
    pthread_mutex_destroy(&db_mutex);

    return TRUE;
}

char retrieve_cnt(struct Route * destination, char **response) {
    pthread_mutex_t db_mutex;
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
    short rs = get_cnt(destination, response);
    if (rs == FALSE) {
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    return TRUE;
}

char retrieve_ae(struct Route * destination, char **response) {
    pthread_mutex_t db_mutex;
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
	short rs = get_ae(destination, response);
    if (rs == FALSE) {
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    return TRUE;
}

char retrieve_cin(struct Route * destination, char **response) {
    pthread_mutex_t db_mutex;
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
    short rs = get_cin(destination, response);
    if (rs == FALSE) {
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    return TRUE;
}

char retrieve_sub(struct Route * destination, char **response) {
    pthread_mutex_t db_mutex;
    if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
	short rs = get_sub(destination, response);
    if (rs == FALSE) {
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
    return TRUE;
}

char validate_keys(cJSON *object, char *keys[], int num_keys, char **response) {
    cJSON *value = NULL;
    size_t response_size = 0;
    size_t new_size = 0;

    for (int i = 0; i < num_keys; i++) {
        value = cJSON_GetObjectItem(object, keys[i]);  // retrieve the value associated with the key
        if (value == NULL) {
            // Calculate the new size required
            new_size = response_size + strlen(keys[i]) + strlen(" key not found; ") + 1;

            // Reallocate memory for the response buffer
            *response = (char *)realloc(*response, new_size * sizeof(char));
            if (*response == NULL) {
                fprintf(stderr, "Failed to reallocate memory for the response buffer\n");
                return FALSE;
            }

            // Concat each error key not found in object
            sprintf(*response + response_size, "%s key not found; ", keys[i]);

            // Update the response_size
            response_size = new_size;
        }
    }

    return response_size == 0 ? TRUE : FALSE;  // all keys were found in object
}

char has_disallowed_keys(cJSON *json_object, const char **allowed_keys, size_t num_allowed_keys) {
    for (cJSON *item = json_object->child; item != NULL; item = item->next) {
        char is_allowed = FALSE;
        for (size_t i = 0; i < num_allowed_keys; i++) {
            if (strcmp(item->string, allowed_keys[i]) == 0) {
                is_allowed = TRUE;
                break;
            }
        }
        if (!is_allowed) {
            return TRUE;
        }
    }
    return FALSE;
}

char delete_resource(struct Route * destination, char **response) {

    char* errMsg = NULL;

    // Sqlite3 initialization opening/creating database
    sqlite3 *db  = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Failed to initialize the database.\n");
        return FALSE;
    }

    char *sql_blob = sqlite3_mprintf("SELECT blob, pi FROM mtc WHERE LOWER(ri) = LOWER('%s') AND et > DATETIME('now');", destination->ri);
    
    if (sql_blob == NULL) {
        fprintf(stderr, "Failed to allocate memory for SQL query.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to allocate memory for SQL query.");
        return FALSE;
    }
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, sql_blob, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to prepare statement.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }
    char *blob = NULL;
    char *pi = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        blob = malloc(strlen((char *)sqlite3_column_text(stmt, 0)));
        strcpy(blob, (char *)sqlite3_column_text(stmt, 0));
        pi = malloc(strlen((char *)sqlite3_column_text(stmt, 1)));
        strcpy(pi, (char *)sqlite3_column_text(stmt, 1));
    } else {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Failed to find the resource.");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char *sql_not = sqlite3_mprintf("SELECT DISTINCT nu, url, enc as blob FROM mtc WHERE LOWER(pi) = LOWER('%s') AND nu IS NOT NULL AND et > DATETIME('now');", pi);
    
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

    // Enable foreign keys
    char *sql = "PRAGMA foreign_keys=ON;";
    char *err_msg = 0;

    rc = sqlite3_exec(db, sql, 0, 0, &err_msg);

    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to enable foreign keys: %s\n", err_msg);
        sqlite3_free(err_msg);
    } else {
        fprintf(stdout, "Foreign keys enabled successfully\n");
    }

    rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }

    if (destination->ty == CIN) {
        // Get the parent url.
        // Given the following key: /onem2m/ae-4
        // The result would be: /onem2m
        char *last_slash = strrchr(destination->key, '/');
        char *result = NULL;

        if (last_slash != NULL) {
            int len = last_slash - destination->key;
            result = malloc((len + 1) * sizeof(char)); // Allocate memory dynamically
            if (result == NULL) {
                // handle error, e.g. print error message and exit or return
                fprintf(stderr, "Error: Unable to allocate memory for result.\n");
                free(result);
                responseMessage(response,500,"Internal Server Error","Error: Unable to allocate memory for result.");
                rollback_transaction(db); // Rollback transaction
                sqlite3_free(errMsg);
                closeDatabase(db);
                return FALSE;
            }
            strncpy(result, destination->key, len);
            result[len] = '\0'; // Ensure the string is null terminated
        } else {
            result = strdup(destination->key); // Use strdup() to allocate memory and copy string
            if (result == NULL) {
                // handle error
                fprintf(stderr, "Error: Unable to allocate memory for result.\n");
                free(result);
                responseMessage(response,500,"Internal Server Error","Error: Unable to allocate memory for result.");
                rollback_transaction(db); // Rollback transaction
                sqlite3_free(errMsg);
                closeDatabase(db);
                return FALSE;
            }
        }

        sqlite3_stmt *stmt;
        int cni, cbs;
        char *sql = sqlite3_mprintf("SELECT cni - 1, cbs - (SELECT cs FROM mtc WHERE ri = '%s'), blob FROM mtc WHERE LOWER(url) = LOWER('%s') AND et > datetime('now');", destination->ri, result);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
        sqlite3_free(sql);
        if (rc != SQLITE_OK) {
            fprintf(stderr,"Failed to execute statement: %s\n", sqlite3_errmsg(db));
            responseMessage(response, 400, "Bad Request", "Verify the request body");
            rollback_transaction(db); // Rollback transaction
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        cJSON *cntBlob = NULL;
        if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            cni = sqlite3_column_int(stmt, 0);
            cbs = sqlite3_column_int(stmt, 1);

            cntBlob = cJSON_Parse((char *)sqlite3_column_text(stmt, 2));
            cJSON *cnt = cJSON_GetObjectItem(cntBlob, "m2m:cnt");
            if(cnt != NULL) {
                cJSON *JsonCni = cJSON_GetObjectItem(cnt, "cni");
                cJSON *JsonCbs = cJSON_GetObjectItem(cnt, "cbs");

                if(JsonCni != NULL) {
                    cJSON_ReplaceItemInObject(cnt, "cni", cJSON_CreateNumber(cni));
                }

                if(JsonCbs != NULL) {
                    cJSON_ReplaceItemInObject(cnt, "cbs", cJSON_CreateNumber(cbs));
                }
            }
        }

        sqlite3_finalize(stmt);

        // Update the parent container
        const char *updateSql = "UPDATE mtc SET cni = ?, cbs = ?, blob = ? WHERE LOWER(url) = LOWER(?);";
        
        rc = sqlite3_prepare_v2(db, updateSql, -1, &stmt, NULL);
        if (rc != SQLITE_OK) {
            free(result);
            fprintf(stderr,"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            responseMessage(response, 500, "Internal Server Error", "Could not update the CNT resource.");
            closeDatabase(db);
            return FALSE;
        }

        sqlite3_bind_int(stmt, 1, cni);
        sqlite3_bind_int(stmt, 2, cbs);
        sqlite3_bind_text(stmt, 3, cJSON_Print(cntBlob), strlen(cJSON_Print(cntBlob)), SQLITE_STATIC);
        sqlite3_bind_text(stmt, 4, result, strlen(result), SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            free(result);
            fprintf(stderr,"Failed to execute statement: %s\n", sqlite3_errmsg(db));
            responseMessage(response, 500, "Internal Server Error", "Could not update the CNT resource.");
            rollback_transaction(db); // Rollback transaction
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        sqlite3_finalize(stmt);
        free(result);
    }

    // Delete record from SQLite3 table
    char* sql_mtc = sqlite3_mprintf("DELETE FROM mtc WHERE ri='%q' AND et > datetime('now')", destination->ri);
    int rs = sqlite3_exec(db, sql_mtc, NULL, NULL, &errMsg);
    sqlite3_free(sql_mtc);

    if (rs != SQLITE_OK) {
        responseMessage(response,400,"Bad Request","Error deleting record");
        fprintf(stderr, "Error deleting record: %s\n", errMsg);
        rollback_transaction(db); // Rollback transaction
        sqlite3_free(errMsg);
        closeDatabase(db);
        return FALSE;
    }

    // Commit transaction
    rc = commit_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't commit transaction\n");
        sqlite3_free(errMsg);
        closeDatabase(db);
        return FALSE;
    }

    printf("Resource deleted from the database\n");

    // Imagina que é a primeira
    // Imagina que é a ultima
    // Imagina que é do meio
    // deleteRoute(); {
    // Remove node from ordered list

    char *resourcePath = destination->key;
    
    struct Route *currentNode = destination->right;

    // Handle cases where the node to delete is the first or the last node.
    if (destination->left != NULL) {
        destination->left->right = destination->right;
    }

    if (destination->right != NULL) {
        destination->right->left = destination->left;
    }

    while (currentNode != NULL && strncmp(currentNode->key, resourcePath, strlen(resourcePath)) == 0) {
        printf("Deleting currentNode->key = %s\n", currentNode->key);

        // We need to store the reference of the right node before freeing the current node.
        struct Route *nextNode = currentNode->right;

        // Again, handle cases where the node to delete is the first or the last node.
        if (currentNode->left != NULL) {
            currentNode->left->right = nextNode;
        }

        // printf("Deleting currentNode->key = %s\n", currentNode->key);
        if (nextNode != NULL) {
            nextNode->left = currentNode->left;
        }

        // free(currentNode);  // Free the memory of the deleted node.
        
        currentNode = nextNode;
    }

    printf("Record deleted ri = %s\n", destination->ri);
    free(destination);  // Don't forget to free the memory of the deleted node.
    responseMessage(response,200,"OK","Record deleted");
    
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
        // Check if the subscription eventNotificationCriteria contains "POST"
        if (strstr(enc_temp, "DELETE") == NULL) {
            continue;
        }

        char *suffix = "}}";

        int suffix_length = strlen(suffix);
        int body_length = strlen(data->body);
        int enc_temp_length = strlen(enc_temp); // assuming enc_temp is a string
        int topic_length = strlen(data->topic);

        // Here we construct the prefix dynamically with sprintf. 
        char prefix[256];  // Make sure this size is enough for your string
        sprintf(prefix, "{\"m2m:sgn\":{\"cr\":\"admin:admin\",\"nev\":{\"net\":\"%s\",\"om\":null,\"rep\":", "DELETE");
        
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

char put_cnt(struct Route* destination, cJSON *content, char** response) {

    const char *allowed_keys[] = {"et", "acpi", "lbl", "daci", "ch", "aa", "at", "mbs", "mni", "mia", "or", "dr"};
	size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    pthread_mutex_t db_mutex;

    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }
	
	if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
	short rs = update_cnt(destination, content, response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
	return TRUE;
}

char put_ae(struct Route* destination, cJSON *content, char** response) {

    const char *allowed_keys[] = {"rr", "et", "apn","nl", "or", "acpi", "lbl", "daci", "poa", "ch", "aa", "csz", "at"};
	size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    pthread_mutex_t db_mutex;

    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }
	
	if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
	short rs = update_ae(destination, content, response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
	return TRUE;
}

char put_sub(struct Route* destination, cJSON *content, char** response) {

    const char *allowed_keys[] = {"et", "acpi", "lbl", "daci", "nu", "enc"};
	size_t num_allowed_keys = sizeof(allowed_keys) / sizeof(allowed_keys[0]);
    char disallowed = has_disallowed_keys(content, allowed_keys, num_allowed_keys);
    pthread_mutex_t db_mutex;

    if (disallowed == TRUE) {
        fprintf(stderr, "The cJSON object has disallowed keys.\n");
        responseMessage(response, 400, "Bad Request", "Found keys not allowed");
        return FALSE;
    }
	
	if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }
	short rs = update_sub(destination, content, response);
    if (rs == FALSE) {
        // responseMessage(response, 400, "Bad Request", "Verify the request body");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }
	return TRUE;
}

char *get_element_value_as_string(cJSON *element) {
    if (element == NULL) {
        return NULL;
    }

    char *value_str = NULL;

    if (cJSON_IsString(element)) {
        value_str = strdup(element->valuestring);
    } else if (cJSON_IsNumber(element)) {
        char buffer[64];
        if (element->valuedouble == (double)element->valueint) {
            snprintf(buffer, sizeof(buffer), "%d", element->valueint);
        } else {
            snprintf(buffer, sizeof(buffer), "%lf", element->valuedouble);
        }
        value_str = strdup(buffer);
    } else if (cJSON_IsBool(element)) {
        value_str = strdup(cJSON_IsTrue(element) ? "true" : "false");
    }

    return value_str;
}
