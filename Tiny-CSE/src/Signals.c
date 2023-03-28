/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

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