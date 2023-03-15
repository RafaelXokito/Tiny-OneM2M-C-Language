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

char create_ae(struct Route** head, struct Route* destination, cJSON *content, char* response) {

    char *keys[] = {"ri", "rn"};  // array of keys to validate
    int num_keys = 2;  // number of keys in the array
    char aux_response[300] = "";
    char rs = validate_keys(content, keys, num_keys, aux_response);
    if (rs == FALSE) {
        responseMessage(response,400,"Bad Request",aux_response);
        return FALSE;
    }

    cJSON *value_ri = cJSON_GetObjectItem(content, "ri");  // retrieve the value associated with "key_name"
    if (value_ri == NULL) {
        responseMessage(response,400,"Bad Request","ri (resource id) key not found");
        return FALSE;
    }
    to_lowercase(value_ri->valuestring);
    if (search_byri(*head, value_ri->valuestring) != NULL) {
        responseMessage(response,400,"Bad Request","ri (resource id) key already exist");
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

    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response,500,"Internal Server Error","Could not open the database");
		return FALSE;
	}

    printf("Creating AE\n");
    AEStruct ae;
    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    rs = init_ae(&ae, content, db);
    if (rs == FALSE) {
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        pthread_mutex_unlock(&db_mutex);
        pthread_mutex_destroy(&db_mutex);
        closeDatabase(db);
        return FALSE;
    }

    // Add New Routes
    to_lowercase(uri);
    addRoute(head, uri, ae.ri, ae.ty, ae.rn);
    printf("New Route: %s -> %s -> %d -> %s \n", uri, ae.ri, ae.ty, ae.rn);

    // access database here
    pthread_mutex_unlock(&db_mutex);

    // clean up
    pthread_mutex_destroy(&db_mutex);

    closeDatabase(db);

    // Convert the AE struct to json and the Json Object to Json String
    cJSON *root = ae_to_json(&ae);
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

    return TRUE;
}

char retrieve_ae(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
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
        AEStruct ae;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae.ty = sqlite3_column_int(stmt, 0);
        strncpy(ae.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(ae.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(ae.et, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.ct, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.lt, (char *)sqlite3_column_text(stmt, 5), 25);
        break;
    }

    cJSON* root = ae_to_json(&ae);
    if (root == NULL) {
        fprintf(stderr, "Failed to convert AEStruct to JSON.\n");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return FALSE;
    }

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

    AEStruct ae;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ae.ty = sqlite3_column_int(stmt, 0);
        strncpy(ae.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(ae.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(ae.et, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.ct, (char *)sqlite3_column_text(stmt, 5), 25);
        strncpy(ae.lt, (char *)sqlite3_column_text(stmt, 6), 25);
        break;
    }
    
    // Validate if the et value is different than current
    cJSON *value_et = cJSON_GetObjectItem(content, "et");  // retrieve the value associated with "key_name"
    if (strcmp(ae.et, value_et->valuestring) == 0) {
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
    if (strptime(value_et->valuestring, "%Y%m%dT%H%M%S", &parsed_time) == NULL) {
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
        ae.ty = sqlite3_column_int(stmt, 0);
        strncpy(ae.ri, (char *)sqlite3_column_text(stmt, 1), 50);
        strncpy(ae.rn, (char *)sqlite3_column_text(stmt, 2), 50);
        strncpy(ae.pi, (char *)sqlite3_column_text(stmt, 3), 50);
        strncpy(ae.et, (char *)sqlite3_column_text(stmt, 4), 25);
        strncpy(ae.ct, (char *)sqlite3_column_text(stmt, 5), 25);
        strncpy(ae.lt, (char *)sqlite3_column_text(stmt, 6), 25);
        break;
    }

    cJSON *root = ae_to_json(&ae);

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
