#include "Common.h"

char init_protocol(struct Route** head) {

    char rs = init_types();
    if (rs == false) {
        return false;
    }

    pthread_mutex_t db_mutex;

    // initialize mutex
    pthread_mutex_init(&db_mutex, NULL);

    // Sqlite3 initialization opening/creating database
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    // perform database operations
    pthread_mutex_lock(&db_mutex);

    // Check if the cse_base exists
    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT name FROM sqlite_master WHERE type='table' AND name='mtc';", -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare query: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return false;
    }

    CSEBaseStruct * csebase = malloc(sizeof(CSEBaseStruct));
    
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_ROW) {
        // If table dosnt exist we create it and populate it
        printf("The table does not exist.\n");
        sqlite3_finalize(stmt);
        
        char rs = init_cse_base(csebase, false);
        if (rs == false) {
            return false;
        }
    } else {

        sqlite3_finalize(stmt);

        // Check if the table has any data
        sqlite3_stmt *stmt;
        char *sql = sqlite3_mprintf("SELECT COUNT(*) FROM mtc WHERE ty = %d ORDER BY ROWID DESC LIMIT 1;", CSEBASE);
        rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_free(sql);
        if (rc != SQLITE_OK) {
            printf("Failed to prepare 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            closeDatabase(db);
            return false;
        }

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_ROW) {
            printf("Failed to execute 'SELECT COUNT(*) FROM mtc WHERE ty = %d;' query: %s\n", CSEBASE, sqlite3_errmsg(db));
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return false;
        }

        int rowCount = sqlite3_column_int(stmt, 0);

        sqlite3_finalize(stmt);

        if (rowCount == 0) {
            printf("The mtc table dont have CSE_Base resources.\n");

            char rs = init_cse_base(csebase, true);
            if (rs == false) {
                return false;
            }
        } else {
            printf("CSE_Base already inserted.\n");
            // In case of the table and data exists, get the 
            char rs = getLastCSEBaseStruct(csebase, db);
            if (rs == false) {
                return false;
            }
        }
    }

    // access database here
    pthread_mutex_unlock(&db_mutex);

    // clean up
    pthread_mutex_destroy(&db_mutex);

    // Add New Routes
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s", csebase->rn);
    to_lowercase(uri);
    addRoute(head, uri, csebase->ri, csebase->ty, csebase->rn);

    // The DB connection should exist in each thread and should not be shared
    if (closeDatabase(db) == false) {
        fprintf(stderr, "Error closing database.\n");
        return false;
    }

    return true;
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
        sqlite3_close(db);
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

    // Print the resulting JSON string
    char* jsonString = cJSON_Print(root);
    printf("%s", jsonString);

    printf("Coonvert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);

    char * response_data = json_str;
    
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(json_str);

    return true;
}

char create_ae(struct Route** head, struct Route* destination, cJSON *content, char* response) {

    char *keys[] = {"ri", "rn"};  // array of keys to validate
    int num_keys = 2;  // number of keys in the array
    char aux_response[300] = "";
    char rs = validate_keys(content, keys, num_keys, aux_response);
    if (rs == false) {
        responseMessage(response,400,"Bad Request",aux_response);
        return false;
    }

    cJSON *value_ri = cJSON_GetObjectItem(content, "ri");  // retrieve the value associated with "key_name"
    if (value_ri == NULL) {
        responseMessage(response,400,"Bad Request","ri (resource id) key not found");
        return false;
    }
    to_lowercase(value_ri->valuestring);
    if (search_byri(*head, value_ri->valuestring) != NULL) {
        responseMessage(response,400,"Bad Request","ri (resource id) key already exist");
        return false;
    }

    
    char uri[60];
    snprintf(uri, sizeof(uri), "/%s",destination->value);
    cJSON *value_rn = cJSON_GetObjectItem(content, "rn");  // retrieve the value associated with "key_name"
    snprintf(uri, sizeof(uri), "%s/%s",uri ,value_rn->valuestring);
    to_lowercase(uri);
    if (value_rn == NULL) {
        responseMessage(response,400,"Bad Request","rn (resource name) key not found");
        return false;
    }
    if (search_byrn_ty(*head, value_rn->valuestring, AE) != NULL) {
        responseMessage(response,400,"Bad Request","rn (resource name) key already exist in this ty (resource type)");
        return false;
    }

    pthread_mutex_t db_mutex;

    // initialize mutex
    pthread_mutex_init(&db_mutex, NULL);

    // Sqlite3 initialization opening/creating database
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
        responseMessage(response,500,"Internal Server Error","Could not open the database");
		return false;
	}

    printf("Creating AE\n");

    AEStruct ae;

    // Should be garantee that the content (json object) dont have this keys
    cJSON_AddStringToObject(content, "pi", destination->ri);

    // perform database operations
    pthread_mutex_lock(&db_mutex);
    
    rs = init_ae(&ae, content, db);
    if (rs == false) {
        responseMessage(response,400,"Bad Request","Verify the request body");
        return false;
    }

    // Add New Routes

    to_lowercase(uri);
    addRoute(head, uri, ae.ri, ae.ty, ae.rn);
    printf("New Route: %s -> %s -> %d -> %s \n", uri, ae.ri, ae.ty, ae.rn);

    // access database here
    pthread_mutex_unlock(&db_mutex);

    // clean up
    pthread_mutex_destroy(&db_mutex);

    // Convert the AE struct to json and the Json Object to Json String
    cJSON *root = ae_to_json(&ae);
    char *str = cJSON_Print(root);
    strcat(response, str);

    return true;
}

char retrieve_ae(struct Route * destination, char *response) {
    char *sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
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

    printf("Coonvert to json string\n");
    char *json_str = cJSON_PrintUnformatted(root);
    printf("%s\n", json_str);

    char * response_data = json_str;
    
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);

    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    free(json_str);

    return true;
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

    return strcmp(response, "") == 0 ? true : false;  // all keys were found in object
}

char delete_resource(struct Route * destination, char *response) {

    char* errMsg = NULL;

    // Sqlite3 initialization opening/creating database
    sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
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
        return false;
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
        return true;
    }


    if (destination->right == NULL) {
        destination->left->right = NULL;
        responseMessage(response,200,"OK","Record deleted");
        return true;
    }

    
    destination->left->right = destination->right;
    destination->right->left = destination->left;
    responseMessage(response,200,"OK","Record deleted");

    printf("ola\n");

    printf("Record deleted ri = %s\n", destination->ri);
    
    // free(destination); Verificar se o free funciona

    // }

    responseMessage(response,200,"OK","Record deleted");
    
    return true;
}

char update_ae(struct Route* destination, cJSON *content, char* response){
    char *keys[] = {"et"};  // array of keys to validate
    int num_keys = 1;  // number of keys in the array
    char aux_response[300] = "";

    // Validate rn key exists
    char rs = validate_keys(content, keys, num_keys, aux_response);
    if (rs == false) {
        responseMessage(response,400,"Bad Request",aux_response);
        return false;
    }
    // Retrieve the AE
    char * sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);
    sqlite3_stmt *stmt;
    struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
    short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }
    
    AEStruct ae;
    CSEBaseStruct csebase;
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
    
    // Validate if the et value is different than current
    cJSON *value_et = cJSON_GetObjectItem(content, "et");  // retrieve the value associated with "key_name"
    if(strcmp(ae.et,value_et->valuestring) == 0){
        responseMessage(response,400,"Bad Request","Resource expiration time is equal to the current one");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }

    struct tm time_struct;
    int result = strptime(value_et->valuestring, "%Y%m%dT%H%M%S", &time_struct);
    if (result == NULL) {
        // The date string did not match the expected format
        responseMessage(response,400,"Bad Request","Invalid date format");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }

    time_t current_time, input_time;
    // Convert struct tm to time_t
    input_time = mktime(&time_struct);

    // Get current system time
    current_time = time(NULL);

    // Compare input time with current time
    if (current_time >= input_time) {
        responseMessage(response,400,"Bad Request","Expiration time is in the past");
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }

    // verify date is not in past
    sql = sqlite3_mprintf("UPDATE mtc SET et='%s' WHERE ri = '%s' AND ty = %d;", value_et->valuestring, destination->ri, destination->ty);
    printf("%s\n",sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }
    
    // Execute the statement
    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        printf("Failed to execute statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }

    // Retrieve the AE
    sql = sqlite3_mprintf("SELECT ty, ri, rn, pi, et, lt, ct FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
    printf("%s\n",sql);

    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        closeDatabase(db);
        return false;
    }

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

    cJSON * root = ae_to_json(&ae);

    printf("Convert to json string\n");
    char * json_str = cJSON_PrintUnformatted(root);
    char * response_data = json_str;
    printf("%s\n", json_str);
    strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

    strcat(response, response_data);
    
    cJSON_Delete(root);
    sqlite3_finalize(stmt);
    closeDatabase(db);
    free(json_str);
     
    return true;
   
}