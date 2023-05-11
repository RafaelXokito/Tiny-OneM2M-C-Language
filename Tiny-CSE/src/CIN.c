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

CINStruct *init_cin() {
    CINStruct *cin = (CINStruct *) malloc(sizeof(CINStruct));
    if (cin) {
        cin->url = NULL;
        cin->ct[0] = '\0';
        cin->ty = CIN;
        cin->et[0] = '\0';
        cin->json_lbl = NULL;
        cin->pi[0] = '\0';
        cin->aa[0] = '\0';
        cin->rn[0] = '\0';
        cin->ri[0] = '\0';
        cin->json_at = NULL;
        cin->or[0] = '\0';
        cin->lt[0] = '\0';
        cin->blob = NULL;
        cin->st = 0;
        cin->cnf[0] = '\0';
        cin->cs = 0;
        cin->cr[0] = '\0';
        cin->con = NULL;
        cin->dc = NULL;
        cin->dr[0] = '\0';
    }
    return cin;
}

char create_cin(CINStruct * cin, cJSON *content, char** response) {
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
    const char *query = "SELECT COALESCE(MAX(CAST(substr(ri, 4) AS INTEGER)), 0) + 1 as result FROM mtc WHERE ty = 3";
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
        size_t ri_size = snprintf(NULL, 0, "CCIN%d", result) + 1; // Get the required size for the ri string

        ri = malloc(ri_size * sizeof(char)); // Allocate dynamic memory for the ri string
        if (ri == NULL) {
            fprintf(stderr, "Failed to allocate memory for ri\n");
            closeDatabase(db);
            return FALSE;
        }
        snprintf(ri, ri_size, "CCIN%d", result); // Write the formatted string to ri
        cJSON_AddStringToObject(content, "ri", ri);
    } else {
        fprintf(stderr, "Failed to fetch the result\n");
        closeDatabase(db);
        return FALSE;
    }

    cin->ty = CIN;
    strcpy(cin->ri, cJSON_GetObjectItemCaseSensitive(content, "ri")->valuestring);
    strcpy(cin->rn, cJSON_GetObjectItemCaseSensitive(content, "rn")->valuestring);
    strcpy(cin->pi, cJSON_GetObjectItemCaseSensitive(content, "pi")->valuestring);
    cin->st = cJSON_GetObjectItemCaseSensitive(content, "st")->valueint;
    strcpy(cin->con, cJSON_GetObjectItemCaseSensitive(content, "con")->valuestring);
    strcpy(cin->cnf, cJSON_GetObjectItemCaseSensitive(content, "cnf")->valuestring);
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
        strcpy(cin->et, et->valuestring);
    } else {
        strcpy(cin->et, get_datetime_days_later(DAYS_PLUS_ET));
    }
    strcpy(cin->ct, getCurrentTime());
    strcpy(cin->lt, getCurrentTime());

    const char *keys[] = {"lbl"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    for (int i = 0; i < num_keys; i++) {
        cJSON *json_array = cJSON_GetObjectItemCaseSensitive(content, keys[i]);
        if (json_array) {
            char *json_str = cJSON_Print(json_array);
            if (json_str) {
                size_t len = strlen(json_str);
                if(keys[i]=="lbl"){
                    cin->json_lbl = (char *)malloc(len+1);
                    strcpy(cin->json_lbl, json_str);
                }
            }           
        }else if (json_array == NULL){
            cJSON *empty_array = cJSON_CreateArray();
            size_t len = strlen(cJSON_Print(empty_array));
            
            if(keys[i]=="lbl"){
                cin->json_lbl = (char *)malloc(len+1);
                strcpy(cin->json_lbl, cJSON_Print(empty_array));
            }
            
        }       
    }
    
    //blob
    content = cin_to_json(cin);
    size_t rnLengthBlob = strlen(cJSON_Print(content));
    cin->blob = (char *)malloc(rnLengthBlob);
    if (cin->blob == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return FALSE;
    }
    strcpy(cin->blob, cJSON_Print(cin_to_json(cin)));  
    short rc = begin_transaction(db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Can't begin transaction\n");
        closeDatabase(db);
        return FALSE;
    }
    // Prepare the insert statement
    const char *insertSQL = "INSERT INTO mtc (ty, ri, rn, pi, st, cnf, cs, con, et, ct, lt, url, blob, lbl) "
                        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);"
                        "UPDATE mtc SET cni = cni + 1, cbs = cbs + ? WHERE ri = ?;";

    rc = sqlite3_prepare_v2(db, insertSQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr,"Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        responseMessage(response, 400, "Bad Request", "Verify the request body");
        closeDatabase(db);
        return FALSE;
    }

    // Bind the values to the statement
    sqlite3_bind_int(stmt, 1, cin->ty);
    sqlite3_bind_text(stmt, 2, cin->ri, strlen(cin->ri), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, cin->rn, strlen(cin->rn), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, cin->pi, strlen(cin->pi), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, cin->st);
    sqlite3_bind_text(stmt, 6, cin->cnf, strlen(cin->cnf), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 7, cin->cs);
    sqlite3_bind_text(stmt, 8, cin->con, strlen(cin->cnf), SQLITE_STATIC);
    struct tm ct_tm, lt_tm, et_tm;
    strptime(cin->ct, "%Y%m%dT%H%M%S", &ct_tm);
    strptime(cin->lt, "%Y%m%dT%H%M%S", &lt_tm);
    strptime(cin->et, "%Y%m%dT%H%M%S", &et_tm);
    char ct_iso[30], lt_iso[30], et_iso[30];
    strftime(ct_iso, sizeof(ct_iso), "%Y-%m-%d %H:%M:%S", &ct_tm);
    strftime(lt_iso, sizeof(lt_iso), "%Y-%m-%d %H:%M:%S", &lt_tm);
    strftime(et_iso, sizeof(et_iso), "%Y-%m-%d %H:%M:%S", &et_tm);
    sqlite3_bind_text(stmt, 9, et_iso, strlen(et_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 10, ct_iso, strlen(ct_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 11, lt_iso, strlen(lt_iso), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 12, cin->url, strlen(cin->url), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 13, cin->blob, strlen(cin->blob), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 14, cin->json_lbl, strlen(cin->json_lbl), SQLITE_STATIC);
    sqlite3_bind_int(stmt, 15, cin->cs);
    sqlite3_bind_text(stmt, 16, cin->pi, strlen(cin->pi), SQLITE_STATIC);

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

    // Actions that need to done in the CNT resource update the cni and cbs
    // Step 2: Check if cni > mni or cbs > mbs
    int cni, mni, cbs, mbs;
    char *sql = sqlite3_mprintf("SELECT cni, mni, cbs, mbs FROM mtc WHERE ri = %s;", cin->pi);
    sqlite3_stmt *stmt;
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

    if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        cni = sqlite3_column_int(stmt, 0);
        mni = sqlite3_column_int(stmt, 1);
        cbs = sqlite3_column_int(stmt, 2);
        mbs = sqlite3_column_int(stmt, 3);
    }

    sqlite3_finalize(stmt);

    // Step 3: If cni > mni or cbs > mbs, delete instances as needed
    while (cni > mni || cbs > mbs) {
        // Get the id and size of the earliest instance
        char instance_id[30];
        int instance_size;
        sql = sqlite3_mprintf("SELECT ri, cs FROM mtc WHERE pi = %s ORDER BY ct LIMIT 1;", cin->pi);
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

        if ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            strcpy(instance_id, (char *)sqlite3_column_text(stmt, 0));
            instance_size = sqlite3_column_int(stmt, 1);
        }

        sqlite3_finalize(stmt);

        // Delete the earliest instance
        sql = sqlite3_mprintf("DELETE FROM mtc WHERE ri = %s;", instance_id);
        char* err_msg = NULL;
        rc = sqlite3_exec(db, sql, 0, 0, &err_msg);
        sqlite3_free(sql);
        if (rc != SQLITE_OK) {
            fprintf(stderr,"Failed to execute statement: %s\n", err_msg);
            responseMessage(response, 400, "Bad Request", "Verify the request body");
            rollback_transaction(db); // Rollback transaction
            sqlite3_finalize(stmt);
            closeDatabase(db);
            return FALSE;
        }

        // Update cni and cbs
        cni--;
        cbs -= instance_size;
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
    printf("CIN data inserted successfully.\n");
    return TRUE;
}

cJSON *cin_to_json(const CINStruct *cin) {
    cJSON *innerObject = cJSON_CreateObject();
    cJSON_AddStringToObject(innerObject, "ct", cin->ct);
    cJSON_AddNumberToObject(innerObject, "ty", cin->ty);
    cJSON_AddStringToObject(innerObject, "ri", cin->ri);
    cJSON_AddStringToObject(innerObject, "rn", cin->rn);
    cJSON_AddStringToObject(innerObject, "pi", cin->pi);
    cJSON_AddStringToObject(innerObject, "aa", cin->aa);
    cJSON_AddNumberToObject(innerObject, "st", cin->st);
    cJSON_AddStringToObject(innerObject, "cnf", cin->cnf);
    cJSON_AddNumberToObject(innerObject, "cs", cin->cs);
    cJSON_AddStringToObject(innerObject, "con", cin->con);
    cJSON_AddStringToObject(innerObject, "et", cin->et);
    cJSON_AddStringToObject(innerObject, "or", cin->or);
    cJSON_AddStringToObject(innerObject, "lt", cin->lt);

    // Add JSON string attributes back into cJSON object
    const char *keys[] = {"lbl", "at"};
    short num_keys = sizeof(keys) / sizeof(keys[0]);
    const char *json_strings[] = {cin->json_lbl, cin->json_at};

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

    // Create the outer JSON object with the key "m2m:cin" and the value set to the inner object
    cJSON* root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "m2m:cin", innerObject);

    return root;
}

char get_cin(struct Route* destination, char** response){
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
    char *response_data = (char *)sqlite3_column_text(stmt, 0);
    if (response_data == NULL) {
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
    free(response_data);
    return TRUE;
}