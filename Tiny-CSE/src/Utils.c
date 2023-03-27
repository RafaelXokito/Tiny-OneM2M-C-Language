/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *
 * Copyright (c) 2023 IPLeiria
 */

#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "Utils.h"

extern int DAYS_PLUS_ET;
extern int PORT;
extern char BASE_RI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_RN[MAX_CONFIG_LINE_LENGTH];
extern char BASE_CSI[MAX_CONFIG_LINE_LENGTH];
extern char BASE_POA[MAX_CONFIG_LINE_LENGTH];

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
        } else if (strcmp(key, "BASE_RI") == 0) {
            strcpy(BASE_RI, value);
        } else if (strcmp(key, "BASE_RN") == 0) {
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