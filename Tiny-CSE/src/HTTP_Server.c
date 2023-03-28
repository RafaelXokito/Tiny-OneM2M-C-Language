/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include "HTTP_Server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

void init_server(HTTP_Server * http_server, int port) {
	http_server->port = port;

	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

	// Enable reuse of the port
    int optval = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }

	// Bind the socket to the desired port
	struct sockaddr_in server_address;
    memset(&server_address, 0, sizeof(server_address));
	server_address.sin_family = AF_INET;
	server_address.sin_port = htons(port);
	server_address.sin_addr.s_addr = INADDR_ANY;

	if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        fprintf(stderr, "Failed to bind to port %d: %s\n", port, strerror(errno));
        // Try another port
        port++;
        server_address.sin_port = htons(port);
        if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
            perror("bind");
            exit(EXIT_FAILURE);
        }
    }

	listen(server_socket, 5);

	http_server->socket = server_socket;
	printf("HTTP Server Initialized\nPort: %d\n", http_server->port);
}
