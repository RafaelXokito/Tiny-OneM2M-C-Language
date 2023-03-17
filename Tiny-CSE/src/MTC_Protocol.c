#define _XOPEN_SOURCE 700
#include <time.h>

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

    CSEBaseStruct *csebase = malloc(sizeof(CSEBaseStruct));
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
        
        char rs = init_cse_base(csebase, FALSE);
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

            char rs = init_cse_base(csebase, TRUE);
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

char retrieve_csebase(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, ct, lt FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    printf("Creating the json object\n");
    CSEBaseStruct csebase;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        csebase.ty = sqlite3_column_int(stmt, 0);
        strncpy(csebase.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(csebase.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(csebase.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(csebase.ct, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(csebase.lt, (char *)sqlite3_column_text(stmt, 5), 25);
        break;
    }

    cJSON *root = csebase_to_json(&csebase);

    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);
    
    snprintf(response, 1024, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n%s", json_str);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);

    return TRUE;
}

char post_ae(struct Route** head, struct Route* destination, cJSON *content, char* response) {

    // JSON Validation

    // "rn" is an optional, but if dont come with it we need to generate a resource name
    char *keys_rn[] = {"rn"};  // Resource Name
    int num_keys = 1;  // number of keys in the array
    char aux_response[300] = "";
    char rs = validate_keys(content, keys_rn, num_keys, aux_response);
    if (rs == FALSE) {
        // Se não tiver "rn" geramos um com "AE-<UniqueID>"
        char unique_id[MAX_CONFIG_LINE_LENGTH];
        generate_unique_id(unique_id);

        char unique_name[MAX_CONFIG_LINE_LENGTH];
        snprintf(unique_name, sizeof(unique_name), "AE-%s", unique_id);
        cJSON_AddStringToObject(content, "rn", unique_name);
    }

    // Mandatory Atributes
    char *keys_m[] = {"api", "rr"};  // App-ID, requestReachability
    num_keys = 2;  // number of keys in the array
    strcpy(aux_response, "");
    rs = validate_keys(content, keys_m, num_keys, aux_response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", aux_response);
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
    char ri[50] = "";
    int countAE = count_same_types(*head, AE);
    snprintf(ri, sizeof(ri), "CAE%d", countAE);
    printf("%s\n", ri);
    cJSON_AddStringToObject(content, "ri", ri);
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    rs = create_ae(ae, content, response);
    if (rs == FALSE) {
        //responseMessage(response, 400, "Bad Request", "Verify the request body");
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
    char *str = cJSON_Print(root);
    if (str == NULL) {
        responseMessage(response, 500, "Internal Server Error", "Could not print cJSON object");
        cJSON_Delete(root);
        return FALSE;
    }
    strcat(response, str);

    // Free allocated resources
    cJSON_Delete(root);
    cJSON_free(str);

    // // access database here
    pthread_mutex_unlock(&db_mutex);

    // // clean up
    pthread_mutex_destroy(&db_mutex);

    return TRUE;
}

char retrieve_ae(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, aei, api, rr, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
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
    AEStruct *ae = init_ae();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae->ty = sqlite3_column_int(stmt, 0);
        strncpy(ae->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(ae->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        strncpy(ae->aei, (char *)sqlite3_column_text(stmt, 4), 5);
        strncpy(ae->api, (char *)sqlite3_column_text(stmt, 5), 20);
        strncpy(ae->rr, (char *)sqlite3_column_text(stmt, 6), 5);
        strncpy(ae->et, (char *)sqlite3_column_text(stmt, 7), 20);
        strncpy(ae->ct, (char *)sqlite3_column_text(stmt, 8), 20);
        strncpy(ae->lt, (char *)sqlite3_column_text(stmt, 9), 20);
        break;
    }

    cJSON* root = ae_to_json(ae);
    if (root == NULL) {
        fprintf(stderr, "Failed to convert AEStruct to JSON.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }


    // Create the parent JSON object
    add_arrays_to_json(db, ae, cJSON_GetObjectItem(root, "m2m:ae"));

    char *json_str = cJSON_PrintUnformatted(root);
    if (json_str == NULL) {
        fprintf(stderr, "Failed to print JSON as a string.\n");
        cJSON_Delete(root);
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    char * response_data = json_str;
    
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);

    return TRUE;
}

char validate_keys(cJSON *object, char *keys[], int num_keys, char *response) {
    cJSON *value = NULL;

    for (int i = 0; i < num_keys; i++) {
        value = cJSON_GetObjectItem(object, keys[i]);  // retrieve the value associated with the key
        if (value == NULL) {
            // concat each error key not found in object
            sprintf(response, "%s%s key not found; ", response, keys[i]);
        }
    }

    return strcmp(response, "") == 0 ? TRUE : FALSE;  // all keys were found in object
}

char delete_resource(struct Route * destination, char *response) {

    char* errMsg = NULL;

    // Sqlite3 initialization opening/creating database
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return FALSE;
	}

    // Delete record from SQLite3 table
    char* sql = sqlite3_mprintf("DELETE FROM mtc WHERE ri='%q'", destination->ri);
    int rs = sqlite3_exec(db, sql, NULL, NULL, &errMsg);
    sqlite3_free(sql);

    if (rs != SQLITE_OK) {
        responseMessage(response,400,"Bad Request","Error deleting record");
        fprintf(stderr, "Error deleting record: %s\n", errMsg);
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

    printf("ola\n");

    printf("Record deleted ri = %s\n", destination->ri);
    
    responseMessage(response,200,"OK","Record deleted");
    
    return TRUE;
}

char update_ae(struct Route* destination, cJSON *content, char* response) {
    char *keys[] = {"et"};  // array of keys to validate
    int num_keys = 1;  // number of keys in the array
    char aux_response[300] = "";

    // Validate et key exists
    char rs = validate_keys(content, keys, num_keys, aux_response);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", aux_response);
        return FALSE;
    }
    // Retrieve the AE
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n", sql);
    sqlite3_stmt *stmt;
    struct sqlite3 *db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    AEStruct *ae = init_ae();
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae->ty = sqlite3_column_int(stmt, 0);
        strncpy(ae->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(ae->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        strncpy(ae->et, (char *)sqlite3_column_text(stmt, 4), 20);
        strncpy(ae->ct, (char *)sqlite3_column_text(stmt, 5), 20);
        strncpy(ae->lt, (char *)sqlite3_column_text(stmt, 6), 20);
        break;
    }
    
    // Validate if the et value is different than current
    cJSON *value_et = cJSON_GetObjectItem(content, "et");  // retrieve the value associated with "key_name"
    if (strcmp(ae->et, value_et->valuestring) == 0) {
        responseMessage(response, 400, "Bad Request", "Resource expiration time is equal to the current one");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    struct tm parsed_time;
    time_t datetime_timestamp, current_time;

    // Clear the struct to avoid garbage values
    memset(&parsed_time, 0, sizeof(parsed_time));

    // Parse the datetime string
    char *parse_result;
    parse_result = strptime(value_et->valuestring, "%Y%m%dT%H%M%S", &parsed_time);
    if (parse_result == NULL) {
        // The date string did not match the expected format
        responseMessage(response, 400, "Bad Request", "Invalid date format");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    // Convert the parsed time to a timestamp
    datetime_timestamp = mktime(&parsed_time);

    // Get the current time
    current_time = time(NULL);

    // Compare the times
    if (difftime(datetime_timestamp, current_time) < 0) {
        responseMessage(response, 400, "Bad Request", "Expiration time is in the past");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }


    // Update the expiration time
    sql = sqlite3_mprintf("UPDATE mtc SET et='%s' WHERE ri = '%s' AND ty = %d;", value_et->valuestring, destination->ri, destination->ty);
    printf("%s\n", sql);

    rc = sqlite3_exec(db, sql, NULL, NULL, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        closeDatabase(db);
        return FALSE;
    }

    // Retrieve the AE with the updated expiration time
    sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n", sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae->ty = sqlite3_column_int(stmt, 0);
        strncpy(ae->ri, (char *)sqlite3_column_text(stmt, 1), 10);
        strncpy(ae->rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae->pi, (char *)sqlite3_column_text(stmt, 3), 10);
        strncpy(ae->et, (char *)sqlite3_column_text(stmt, 4), 20);
        strncpy(ae->ct, (char *)sqlite3_column_text(stmt, 5), 20);
        strncpy(ae->lt, (char *)sqlite3_column_text(stmt, 6), 20);
        break;
    }

    cJSON *root = ae_to_json(ae);

    printf("Convert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    char *response_data = json_str;
    printf("%s\n", json_str);
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);

    return TRUE;
}

static int insert_element_into_multivalue_table(sqlite3 *db, const char *mtc_ri, int parent_id, const char *key, const char *value, const char *type) {

    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO multivalue (mtc_ri, parent_id, key, value, type) VALUES (?, ?, ?, ?, ?);";

    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Cannot prepare statement: %s\n", sqlite3_errmsg(db));
        return rc;
    }

    sqlite3_bind_text(stmt, 1, mtc_ri, strlen(mtc_ri), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, parent_id);
    sqlite3_bind_text(stmt, 3, key, strlen(key), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, value, strlen(value), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 5, type, strlen(type), SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Execution failed: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return rc;
    }

    sqlite3_finalize(stmt);
    return SQLITE_OK;
}

char insert_multivalue_element(cJSON *element, const char *mtc_ri, int parent_id, const char *key, sqlite3 *db) {
    if (cJSON_IsObject(element)) {
        // Insert root entry for the object
        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, key, "root", "object") != SQLITE_OK) {
            fprintf(stderr, "Failed to insert root entry for the object\n");
            return FALSE;
        }
        // Retrieve the ID of the root entry
        int root_id = (int)sqlite3_last_insert_rowid(db);

        cJSON *item;
        cJSON_ArrayForEach(item, element) {
            insert_multivalue_element(item, mtc_ri, root_id, item->string, db);
        }
    } else if (cJSON_IsArray(element)) {

        // Insert root entry for the object
        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, key, "root", "array") != SQLITE_OK) {
            fprintf(stderr, "Failed to insert root entry for the object\n");
            return FALSE;
        }

        // Retrieve the ID of the root entry
        int root_id = (int)sqlite3_last_insert_rowid(db);

        cJSON *item;
        cJSON_ArrayForEach(item, element) {
            insert_multivalue_element(item, mtc_ri, root_id, "", db);
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

        if (insert_element_into_multivalue_table(db, mtc_ri, parent_id, key, value_str, type) != SQLITE_OK) {
            fprintf(stderr, "Failed to insert multivalue element\n");
            return FALSE;
        }
    }
    return TRUE;
}

char insert_multivalue_elements(sqlite3 *db, const char *parent_ri, const char *key, cJSON *atr_array) {
    // Insert root entry for the array attribute
    if (insert_element_into_multivalue_table(db, parent_ri, 0, key, "root", "root") != SQLITE_OK) {
        fprintf(stderr, "Failed to insert root entry for the multivalue element\n");
        return FALSE;
    }
    // Retrieve the ID of the root entry
    int root_id = (int)sqlite3_last_insert_rowid(db);

    for (int i = 0; i < cJSON_GetArraySize(atr_array); i++) {
        cJSON *element = cJSON_GetArrayItem(atr_array, i);
        if (element) {
            if (!insert_multivalue_element(element, parent_ri, root_id, "", db)) {
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


void add_arrays_to_json(sqlite3 *db, const AEStruct *ae, cJSON *parent_json) {
    // Array of keys to iterate
    char *keys[] = {"acpi", "lbl", "daci", "poa", "ch", "at"};
    int num_keys = sizeof(keys) / sizeof(keys[0]);

    // Loop through the keys array
    for (int i = 0; i < num_keys; i++) {
        char *current_key = keys[i];

        cJSON *labels = retrieve_multivalue_elements(db, ae->ri, current_key);

        if (labels != NULL) {
            // Add the labels cJSON object to the parent_json object
            cJSON_AddItemToObject(parent_json, current_key, labels);
        }
    }
}
