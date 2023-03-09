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

#include "Common.h"

#define true 1
#define false 0

int main() {

	// Register the SIGINT signal handler
    signal(SIGINT, sigint_handler);
	

	// initiate HTTP_Server
	HTTP_Server http_server;
	init_server(&http_server, 6970);

	int client_socket;
	pthread_t thread_id;
	
	// registering Routes
	struct Route * route = initRoute("/", "index.html"); 
	addRoute(route, "/about", "about.html");

	printf("\n====================================\n");
	printf("=========ALL VAILABLE ROUTES========\n");
	// display all available routes
	inorder(route);

	// Sqlite3 initialization opening/creating database
	struct sqlite3 * mydb = initDatabase("tiny-oneM2M.db");
	if (mydb == NULL) {
		exit(0);
	}

	// char *query = "CREATE TABLE mytable (id INT, name TEXT);";
	// // When we expect the query to return something such as a SELECT statement isCallback flat should be true
	// short rc = execDatabaseScript(query, mydb, false);
	// if (rc == false) {
	// 	exit(0);
	// }

	// accept incoming client connections and handle them in separate threads
    while (1) {
		client_socket = accept(http_server.socket, NULL, NULL);

		ConnectionInfo* info = malloc(sizeof(ConnectionInfo));
		info->socket_desc = client_socket;
    	info->route = route;
        
        if (pthread_create(&thread_id, NULL, handle_connection, info) < 0) {
            perror("pthread_create failed");
            exit(EXIT_FAILURE);
        }
        
        printf("New client connected, thread created for handling.\n");
    }

	closeDatabase(mydb);

	return 0;
}
