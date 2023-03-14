#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sqlite3.h>
#include <pthread.h>
#include <signal.h>

#include "Common.h"

#define true 1
#define false 0

int client_socket;

int main() {

	// Register the SIGINT signal handler
    signal(SIGINT, sigint_handler);

	pthread_t thread_id;
	
	// registering Routes
	struct Route *head = NULL; // initialize the head pointer to NULL
	head = addRoute(&head, "/", "", -1, "index.html"); // add the first node to the list
	
	short rs = init_protocol(&head);
	if (rs == false) {
        exit(EXIT_FAILURE);
    }

    rs = init_routes(&head);
	if (rs == false) {
        exit(EXIT_FAILURE);
    }

	printf("\n====================================\n");
	printf("=========ALL VAILABLE ROUTES========\n");
	// display all available routes
	inorder(head);

	// initiate HTTP_Server
	HTTP_Server http_server;
	init_server(&http_server, 6969);

	// accept incoming client connections and handle them in separate threads
    while (1) {
		client_socket = accept(http_server.socket, NULL, NULL);

		ConnectionInfo* info = malloc(sizeof(ConnectionInfo));
		info->socket_desc = client_socket;
    	info->route = head;
        
        if (pthread_create(&thread_id, NULL, handle_connection, info) < 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
        
        printf("New client connected, thread created for handling.\n");
    }

	return 0;
}
