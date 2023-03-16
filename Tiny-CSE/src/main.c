#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#include "Common.h"

int client_socket;

int DAYS_PLUS_ET = 0;
char BASE_RI[MAX_CONFIG_LINE_LENGTH] = "";
char BASE_RN[MAX_CONFIG_LINE_LENGTH] = "";

int main() {

    // Register the SIGINT signal handler
    signal(SIGINT, sigint_handler);

	load_config_file(".config");
	if (DAYS_PLUS_ET == 0 || strcmp(BASE_RI, "") == 0 || strcmp(BASE_RN, "") == 0) {
		perror("Configuration variables are missing. Should follow this regex %[^= ] = %s (e.g. BASE_RI = onem2m)");
		exit(EXIT_FAILURE);
	}

    pthread_t thread_id;

    // registering Routes
    struct Route *head = NULL; // initialize the head pointer to NULL
    head = addRoute(&head, "/", "", -1, "index.html"); // add the first node to the list

    short rs = init_protocol(&head);
    if (rs == FALSE) {
		perror("Error initializing protocol.");
        exit(EXIT_FAILURE);
    }

    rs = init_routes(&head);
    if (rs == FALSE) {
		perror("Error initializing routes.");
        exit(EXIT_FAILURE);
    }

    printf("\n====================================\n");
    printf("=========ALL AVAILABLE ROUTES========\n");
    // display all available routes
    inorder(head);

    // initiate HTTP_Server
    HTTP_Server http_server;
    init_server(&http_server, 6969);

    // accept incoming client connections and handle them in separate threads
    while (TRUE) {
        client_socket = accept(http_server.socket, NULL, NULL);
        if (client_socket < 0) {
            perror("accept failed");
            exit(EXIT_FAILURE);
        }

        ConnectionInfo* info = malloc(sizeof(ConnectionInfo));
        if (info == NULL) {
            perror("malloc failed");
            exit(EXIT_FAILURE);
        }

        info->socket_desc = client_socket;
        info->route = head;

        if (pthread_create(&thread_id, NULL, handle_connection, (void*)info) < 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }

        printf("New client connected, thread created for handling.\n");
    }

    // Free allocated memory
    free(head);
    head = NULL;

    return 0;
}
