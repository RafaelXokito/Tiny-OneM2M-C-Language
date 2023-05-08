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
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, csi, cst, ct, lt FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
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
        csebase->ty = sqlite3_column_int(stmt, 0);
        strncpy(csebase->ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(csebase->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(csebase->pi, (char *)sqlite3_column_text(stmt, 3), 50);
        csebase->cst = sqlite3_column_int(stmt, 4);
        strncpy(csebase->csi, (char *)sqlite3_column_text(stmt, 5), 50);
        const char *ct_iso = (const char *)sqlite3_column_text(stmt, 6);
        const char *lt_iso = (const char *)sqlite3_column_text(stmt, 7);
        struct tm ct_tm, lt_tm;
        strptime(ct_iso, "%Y-%m-%d %H:%M:%S", &ct_tm);
        strptime(lt_iso, "%Y-%m-%d %H:%M:%S", &lt_tm);
        char ct_str[20], lt_str[20];
        strftime(ct_str, sizeof(ct_str), "%Y%m%dT%H%M%S", &ct_tm);
        strftime(lt_str, sizeof(lt_str), "%Y%m%dT%H%M%S", &lt_tm);
        strncpy(csebase->ct, ct_str, 25);
        strncpy(csebase->lt, lt_str, 25);
        break;
    }

    cJSON *root = csebase_to_json(csebase);
    if (root == NULL) {
        fprintf(stderr, "Failed to convert CSEBaseStruct to JSON.\n");
        responseMessage(response, 500, "Internal Server Error", "Failed to prepare statement");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char *keys[] = {"srt", "lbl", "poa", "acpi"};
    int num_keys = sizeof(keys) / sizeof(keys[0]);
    add_arrays_to_json(db, csebase->ri, cJSON_GetObjectItem(root, "m2m:cb"), keys, num_keys);

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        cJSON_Delete(root);
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
        cJSON_Delete(root);
        free(json_str);
        return FALSE;
    }
    sprintf(*response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", json_str);

    cJSON_Delete(root);
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
            char *newMVconditions = sqlite3_mprintf("%s %s EXISTS (SELECT 1 FROM multivalue WHERE mtc_ri = m.mtc_ri AND atr = %Q AND value = %Q)", MVconditions ? MVconditions : "", filter_operation, key, value);
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
                              "SELECT mtc.url FROM mtc "
                              "INNER JOIN ( "
                              "SELECT DISTINCT mtc_ri as ri FROM multivalue AS m WHERE 1 = 1 %s LIMIT %d"
                              ") AS res ON res.ri = mtc.ri "
                              "WHERE mtc.pi = \"%s\" AND mtc.et > datetime('now') AND 1=1%s LIMIT %d",
                              destination->ri,
                              MTCconditions ? MTCconditions : "",
                              limit,
                              MVconditions ? MVconditions : "",
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

    const char *allowed_keys[] = {"apn", "ct", "ty", "acpi", "et", "lbl", "pi", "daci", "poa", "ch", "aa", "aei", "rn", "api", "rr", "csz", "ri", "nl", "at", "or", "lt"};
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
    if (search_byrn_ty(*head, value_rn->valuestring, AE) != NULL) {
        responseMessage(response, 400, "Bad Request", "rn (resource name) key already exist in this ty (resource type)");
        return FALSE;
    }

    pthread_mutex_t db_mutex;

    // initialize mutex
     if (pthread_mutex_init(&db_mutex, NULL) != 0) {
         responseMessage(response, 500, "Internal Server Error", "Could not initialize the mutex");
         return FALSE;
    }

    printf("Creating AE\n");
    AEStruct *ae = init_ae();
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
        fprintf(stderr, "Memory allocation error\n");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        return FALSE;
    }

    // Copy the destination key into ae->url
    strncpy(ae->url, destination->key, destinationKeyLength);
    ae->url[destinationKeyLength] = '\0'; // Add null terminator

    // Append "/<rn value>" to ae->url
    sprintf(ae->url + destinationKeyLength, "/%s", cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    
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
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        fprintf(stderr, "Failed to initialize the database.\n");
        return FALSE;
    }

    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }

    // Delete record from SQLite3 table
    char* sql_mtc = sqlite3_mprintf("DELETE FROM mtc WHERE ri='%q'", destination->ri);
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

    if (destination->left == NULL) {
        destination->right->left = NULL;
        responseMessage(response,200,"OK","Record deleted");
        return TRUE;
    }


    if (destination->right == NULL) {
        destination->left->right = NULL;
        responseMessage(response,200,"OK","Record deleted");
        return TRUE;
    }

    
    destination->left->right = destination->right;
    destination->right->left = destination->left;
    responseMessage(response,200,"OK","Record deleted");

    printf("Record deleted ri = %s\n", destination->ri);
    
    responseMessage(response,200,"OK","Record deleted");
    
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

static int insert_element_into_multivalue_table(sqlite3 *db, const char *mtc_ri, int parent_id, const char* atr, const char *key, const char *value, const char *type) {

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO multivalue (mtc_ri, parent_id, atr, key, value, type) VALUES (?, ?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, mtc_ri, strlen(mtc_ri), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, parent_id);
    sqlite3_bind_text(stmt, 3, atr, strlen(atr), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, key, strlen(key), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, value, strlen(value), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, type, strlen(type), SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return rc;
    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

char insert_multivalue_element(cJSON *element, const char *mtc_ri, int parent_id, const char *atr, const char *key, sqlite3 *db) {
    if (cJSON_IsObject(element)) {
        // Insert root entry for the object
        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, atr, key, "root", "object") != SQLITE_OK) {
            fprintf(stderr, "Failed to insert root entry for the object\n");
            return FALSE;
        }
        // Retrieve the ID of the root entry
        int root_id = (int)sqlite3_last_insert_rowid(db);

        cJSON *item;
        cJSON_ArrayForEach(item, element) {
            insert_multivalue_element(item, mtc_ri, root_id, atr, item->string, db);
        }
    } else if (cJSON_IsArray(element)) {

        // Insert root entry for the object
        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, atr, key, "root", "array") != SQLITE_OK) {
            fprintf(stderr, "Failed to insert root entry for the object\n");
            return FALSE;
        }

        // Retrieve the ID of the root entry
        int root_id = (int)sqlite3_last_insert_rowid(db);

        cJSON *item;
        cJSON_ArrayForEach(item, element) {
            insert_multivalue_element(item, mtc_ri, root_id, atr, "", db);
        }
    } else {
        const char *type;
        char value_str[64];

        if (cJSON_IsString(element)) {
            type = "string";
            strncpy(value_str, element->valuestring, sizeof(value_str) - 1);
            value_str[sizeof(value_str) - 1] = '\0';
        } else if (cJSON_IsNumber(element)) {
            type = "number";
            snprintf(value_str, sizeof(value_str), "%lf", element->valuedouble);
        } else {
            fprintf(stderr, "Unsupported cJSON type\n");
            return FALSE;
        }

        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, atr, key, value_str, type) != SQLITE_OK) {
            fprintf(stderr, "Failed to insert multivalue element\n");
            return FALSE;
        }
    }
    return TRUE;
}

char insert_multivalue_elements(sqlite3 *db, const char *parent_ri, const char *atr, const char *key, cJSON *atr_array) {
    // Insert root entry for the array attribute
    if (insert_element_into_multivalue_table(db, parent_ri, 0, atr, key, "root", "root") != SQLITE_OK) {
        fprintf(stderr, "Failed to insert root entry for the multivalue element\n");
        return FALSE;
    }
    // Retrieve the ID of the root entry
    int root_id = (int)sqlite3_last_insert_rowid(db);

    for (int i = 0; i < cJSON_GetArraySize(atr_array); i++) {
        cJSON *element = cJSON_GetArrayItem(atr_array, i);
        if (element) {
            if (!insert_multivalue_element(element, parent_ri, root_id, atr, "", db)) {
                return FALSE;
            }
        }
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

cJSON *build_json_recursively(sqlite3 *db, int parent_rowid, char is_root_array) {
    sqlite3_stmt *stmt;
    cJSON *result = NULL;

    const char *sql = "SELECT rowid, parent_id, type, key, value FROM multivalue WHERE parent_id = ? ORDER BY rowid";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_int(stmt, 1, parent_rowid);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int rowid = sqlite3_column_int(stmt, 0);
        const char *value_type = (const char *)sqlite3_column_text(stmt, 2);
        const char *key = (const char *)sqlite3_column_text(stmt, 3);
        const char *value = (const char *)sqlite3_column_text(stmt, 4);

        cJSON *item = NULL;

        if (strcmp(value_type, "object") == 0) {
            item = build_json_recursively(db, rowid, FALSE);
            if (strcmp(key, "root") == 0) {
                cJSON *temp = item;
                item = cJSON_Duplicate(cJSON_GetObjectItem(temp, "root"), 1);
                cJSON_Delete(temp);
            }
        } else if (strcmp(value_type, "array") == 0) {
            item = build_json_recursively(db, rowid, TRUE);
        } else if (strcmp(value_type, "number") == 0) {
            double num_value = atof(value);
            item = cJSON_CreateNumber(num_value);
        } else {
            item = cJSON_CreateString(value);
        }

        if (!result) {
            if (is_root_array == TRUE) {
                result = cJSON_CreateArray();
            } else {
                result = cJSON_CreateObject();
            }
        }

        if (is_root_array) {
            cJSON_AddItemToArray(result, item);
        } else {
            cJSON_AddItemToObject(result, key, item);
        }
    }

    sqlite3_finalize(stmt);
    return result;
}

cJSON *retrieve_multivalue_elements(sqlite3 *db, const char *parent_ri, const char *key) {
    sqlite3_stmt *stmt;

    const char *sql = "SELECT rowid FROM multivalue WHERE mtc_ri = ? AND key = ?";

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }

    sqlite3_bind_text(stmt, 1, parent_ri, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, key, -1, SQLITE_TRANSIENT);

    int rowid = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        rowid = sqlite3_column_int(stmt, 0);
    }

    sqlite3_finalize(stmt);

    if (rowid == 0) {
        return NULL;
    }

    cJSON *result = build_json_recursively(db, rowid, TRUE);
    return result;
}

void add_arrays_to_json(sqlite3 *db, const char *ri, cJSON *parent_json, char **keys, int num_keys) {
    // Loop through the keys array
    for (int i = 0; i < num_keys; i++) {
        char *current_key = keys[i];

        cJSON *arrays = retrieve_multivalue_elements(db, ri, current_key);

        if (arrays != NULL) {
            // Add the arrays cJSON object to the parent_json object
            cJSON_ReplaceItemInObject(parent_json, current_key, arrays);
        }
    }
}
