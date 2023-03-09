#include <stdio.h>
#include <stdlib.h>
#include "Signal.h"

void sigint_handler(int sig) {
    // Code to execute when SIGINT is received
    printf("Ctrl+C pressed\n");
    // Do any necessary cleanup or other tasks here
    // ...
    // Exit the program
    exit(0);
}