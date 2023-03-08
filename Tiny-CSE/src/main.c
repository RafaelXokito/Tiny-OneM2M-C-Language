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

#include "HTTP_Server.h"
#include "Routes.h"
#include "Response.h"
#include "Sqlite.h"

#define true 1
#define false 0

int main() {
	// initiate HTTP_Server
	HTTP_Server http_server;
	init_server(&http_server, 6970);

	int client_socket;
	
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

	char *query = "CREATE TABLE mytable (id INT, name TEXT);";
	// When we expect the query to return something such as a SELECT statement isCallback flat should be true
	short rc = execDatabaseScript(query, mydb, false);
	if (rc == 1) {
		exit(0);
	}

	while (1) {
		char client_msg[4096] = "";

		client_socket = accept(http_server.socket, NULL, NULL);

		read(client_socket, client_msg, 4095);
		printf("%s\n", client_msg);

		// parsing client socket header to get HTTP method, route
		char *method = "";
		char *urlRoute = "";

		char *client_http_header = strtok(client_msg, "\n");
		
		printf("\n\n%s\n\n", client_http_header);

		char *header_token = strtok(client_http_header, " ");
		
		int header_parse_counter = 0;

		while (header_token != NULL) {

			switch (header_parse_counter) {
				case 0:
					method = header_token;
				case 1:
					urlRoute = header_token;
			}
			header_token = strtok(NULL, " ");
			header_parse_counter++;
		}

		printf("The method is %s\n", method);
		printf("The route is %s\n", urlRoute);


		char template[100] = "";
		
		if (strstr(urlRoute, "/static/") != NULL) {
			//strcat(template, urlRoute+1);
			strcat(template, "static/index.css");
		}else {
			struct Route * destination = search(route, urlRoute);
			strcat(template, "templates/");

			if (destination == NULL) {
				strcat(template, "404.html");
			}else {
				strcat(template, destination->value);
			}
		}

		// Creating the response
		char * response_data = render_static_file(template);
		
		char http_header[4096] = "HTTP/1.1 200 OK\r\n\r\n";

		strcat(http_header, response_data);
		strcat(http_header, "\r\n\r\n");

		printf("http_header: %s\n", http_header);

		send(client_socket, http_header, sizeof(http_header), 0);
		close(client_socket);
		free(response_data);
	}

	closeDatabase(mydb);

	return 0;
}
