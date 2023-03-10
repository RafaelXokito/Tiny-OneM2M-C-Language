#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <stdlib.h>

#include "Utils.h"

char* getCurrentTime() {
    static char timestamp[30];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%S", timeinfo);
    return timestamp;
}

void to_lowercase(char* str) {
    int i = 0;
    while (str[i]) {
        str[i] = tolower(str[i]);
        i++;
    }
}

char* get_datetime_one_month_later() {
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