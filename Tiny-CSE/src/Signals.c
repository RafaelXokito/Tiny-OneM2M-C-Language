#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include "Common.h"

extern int client_socket; // declare the client_socket variable

void sigint_handler(int sig) {
    // Code to execute when SIGINT is received
    printf("Ctrl+C pressed\n");
    // Do any necessary cleanup or other tasks here
    close(client_socket);
    // Exit the program
    exit(0);
}