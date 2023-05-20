/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <regex.h>

#include "cJSON.h"
#include "Utils.h"
#include "mongoose.h"

extern int DAYS_PLUS_ET;
extern int PORT;
extern char DB_MEM[MAX_CONFIG_LINE_LENGTH];
extern char BASE_RI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_RN[MAX_CONFIG_LINE_LENGTH];
extern char BASE_CSI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_POA[MAX_CONFIG_LINE_LENGTH];

static int is_mqtt_connected = 0;
static const char *s_url = NULL;        // URL for the HTTP request
static const char *s_post_data = NULL;  // POST data for the HTTP request

char* getCurrentTime() {
    static char timestamp[30];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%S", timeinfo);
    return timestamp;
}

char* getCurrentTimeLong() {
    static char timestamp[30];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
    return timestamp;
}

void to_lowercase(char* str) {
    int i = 0;
    while (str[i]) {
        str[i] = tolower(str[i]);
        i++;
    }
}

char* get_datetime_days_later(int days) {
    // Allocate memory for the datetime string
    char* datetime_str = (char*) malloc(20 * sizeof(char));

    // Get the current datetime
    time_t now = time(NULL);
    struct tm *tm_now = localtime(&now);

    // Add one month to the current datetime
    tm_now->tm_mon += 1;
    mktime(tm_now);

    // Create a datetime string in the desired format
    strftime(datetime_str, 20, "%Y%m%dT%H%M%S", tm_now);

    // Return the datetime string
    return datetime_str;
}

void parse_config_line(char* line) {
    char key[MAX_CONFIG_LINE_LENGTH] = "";
    char value[MAX_CONFIG_LINE_LENGTH] = "";

    if (sscanf(line, "%[^= ] = %s", key, value) == 2) {
        if (strcmp(key, "DAYS_PLUS_ET") == 0) {
            DAYS_PLUS_ET = atoi(value);
        } else if (strcmp(key, "PORT") == 0) {
            PORT = atoi(value);
        } else if (strcmp(key, "DB_MEM") == 0) {
            strcpy(DB_MEM, value);
        } else if (strcmp(key, "BASE_RI") == 0) {
            strcpy(BASE_RI, value);
        } else if (strcmp(key, "BASE_RN") == 0) {
            remove_unauthorized_chars(value);
            strcpy(BASE_RN, value);
        } else if (strcmp(key, "BASE_CSI") == 0) {
            strcpy(BASE_CSI, value);
        } else if (strcmp(key, "BASE_POA") == 0) {
            strcpy(BASE_POA, value);
        } else {
            printf("Unknown key: %s\n", key);
        }
    }
}

void load_config_file(const char* filename) {
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        fprintf(stderr, "Error opening config file: %s\n", filename);
        exit(1);
    }

    char line[MAX_CONFIG_LINE_LENGTH];
    while (fgets(line, sizeof(line), file)) {
        if (line[0] == '#' || strlen(line) <= 1) {
            continue;
        }
        parse_config_line(line);
    }

    fclose(file);
}

void generate_unique_id(char *id_str) {
    static int counter = 0;
    
    // Get the current time
    time_t t = time(NULL);
    
    // Get the process ID
    pid_t pid = getpid();
    
    // Create the unique ID string
    snprintf(id_str, MAX_CONFIG_LINE_LENGTH, "%lx%lx%x", (unsigned long) t, (unsigned long) pid, counter++);
}

char is_number(const char *str) {
    if (*str == '\0') {
        return 0;
    }

    while (*str) {
        if (!isdigit((unsigned char)*str)) {
            return 0;
        }
        str++;
    }

    return 1;
}

int key_in_array(const char *key, const char **key_array, size_t key_array_len) {
    for (size_t i = 0; i < key_array_len; i++) {
        if (strcmp(key, key_array[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

void remove_unauthorized_chars(char *str) {
    char *src, *dst;
    for (src = dst = str; *src != '\0'; src++) {
        // Only allow alphanumeric, '-', '_', '~', and '.' characters
        if (isalnum((unsigned char)*src) || *src == '-' || *src == '_' || *src == '~' || *src == '.') {
            *dst = *src;
            dst++;
        }
    }
    *dst = '\0';
}

int is_valid_url(const char *url) {
    const char *pattern = "(http|https|mqtt)://([^/]+:[0-9]+)?[^/]*";

    regex_t regex;
    int res;

    if (regcomp(&regex, pattern, REG_EXTENDED) != 0) {
        return FALSE;
    }
    
    res = regexec(&regex, url, 0, NULL, 0);
    regfree(&regex);

    if (res == REG_NOMATCH) {
        return FALSE;
    }

    return TRUE;
}

void* send_notification(void* arg) {
    notificationData* data = (notificationData*) arg;
    const char* nu = data->nu;
    const char* topic = data->topic;
    const char* body = data->body;

    cJSON *root = cJSON_Parse(nu);
    if (root == NULL)
    {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL)
        {
            fprintf(stderr, "Error before: %s\n", error_ptr);
        }
        free(data->nu); // Free the memory for the string
        free(data); // Then free the memory for the struct
        pthread_exit(NULL);
    }

    int i;
    int count = cJSON_GetArraySize(root);
    for (i = 0; i < count; i++) {
        cJSON *item = cJSON_GetArrayItem(root, i);
        
        // check if the value is an valid URL
        char *item_string = item->valuestring;
        // printf("value: %s\n", item_string);
        // printf("URL to validate: %s : '%d'\n", item_string, is_valid_url(item_string));
        if (is_valid_url(item_string)) {
            printf("Item string: '%s'\n", item_string);
            if (strncmp(item_string, "mqtt://", 7) == 0) {
                mqtt_publish(item_string, topic, body);
            } else if ((strncmp(item_string, "http://", 7) == 0) || strncmp(item_string, "https://", 8) == 0) {
                send_http_request(item_string, topic, body);
            }
        } else {
            fprintf(stderr,"send_notification got an invalid URL (%s).\n", item_string);
        }
    }

    cJSON_Delete(root);
    free(data->nu); // Free the memory for the string after we're done with it
    free(data); // Then free the memory for the struct
    pthread_exit(NULL);
}

void fn_mqtt(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  // Handle the rest of your events as needed...
  
  if (ev == MG_EV_MQTT_OPEN) {
    // MQTT connect is successful
    is_mqtt_connected = 1;
    // Handle other MQTT_OPEN related tasks...
  }
  
  (void) fn_data;
}

void fn_http(struct mg_connection *c, int ev, void *ev_data, void *fn_data) {
  // Handle the rest of your events as needed...
  
  if (ev == MG_EV_CONNECT) {
    struct mg_str host = mg_url_host(s_url);

    int content_length = s_post_data ? strlen(s_post_data) : 0;
    mg_printf(c,
              "%s %s HTTP/1.0\r\n"
              "Host: %.*s\r\n"
              "Content-Type: octet-stream\r\n"
              "Content-Length: %d\r\n"
              "\r\n",
              s_post_data ? "POST" : "GET", mg_url_uri(s_url), (int) host.len,
              host.ptr, content_length);
    mg_send(c, s_post_data, content_length);
  }
  
  (void) fn_data;
}

void mqtt_publish(const char* url, const char* topic, const char* message) {
    struct mg_mgr mgr;
    struct mg_connection* conn;
    is_mqtt_connected = 0;

    mg_mgr_init(&mgr);
    
    // Create a connection
    conn = mg_mqtt_connect(&mgr, url, NULL, fn_mqtt, NULL);
    if (conn == NULL) {
        printf("Failed to create MQTT connection.\n");
        return;
    }
    
    // Wait for the MQTT connection to be ready
    while (!is_mqtt_connected) {
        mg_mgr_poll(&mgr, 1000);
    }
    
    // Publish the message
    struct mg_mqtt_opts pub_opts;
    memset(&pub_opts, 0, sizeof(pub_opts));
    pub_opts.topic = mg_str(topic);
    pub_opts.message = mg_str(message);
    pub_opts.qos = 0;
    pub_opts.retain = false;
    mg_mqtt_pub(conn, &pub_opts);
    
    // Wait for a bit to ensure the message is sent
    mg_mgr_poll(&mgr, 1000);
    
    mg_mgr_free(&mgr);
}

// This function sends a POST request to a given URL with the specified content
// Returns 0 on success, non-zero on error
int send_http_request(const char *base_url, const char *resource_url, const char *content) {
    struct mg_mgr mgr;               // Event manager
    bool done = false;               // Event handler flips it to true

    // Construct the full URL
    int full_url_length = strlen(base_url) + strlen(resource_url) + 2;
    char *full_url = malloc(full_url_length);
    if (full_url == NULL) {
        fprintf(stderr, "Failed to allocate memory for the full URL.\n");
        return 1;
    }
    snprintf(full_url, full_url_length, "%s%s", base_url, resource_url);

    // Initialize the Mongoose event manager
    mg_mgr_init(&mgr);

    // Set the global variables for the callback function
    s_url = full_url;
    s_post_data = content;

    // Create the client connection
    mg_http_connect(&mgr, full_url, fn_http, &done);

    // Run the event loop until the request is done
    while (!done) {
        mg_mgr_poll(&mgr, 50);
    }

    // Free resources
    mg_mgr_free(&mgr);
    free(full_url);

    return 0;
}
