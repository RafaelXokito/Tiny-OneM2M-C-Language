#include <stdio.h>
#include <time.h>

#include "Utils.h"

char* getCurrentTime() {
    static char timestamp[30];
    time_t now = time(NULL);
    struct tm *timeinfo = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y%m%dT%H%M%S,%f", timeinfo);
    return timestamp;
}
