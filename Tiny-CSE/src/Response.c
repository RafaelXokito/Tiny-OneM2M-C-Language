#include "Response.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sqlite3.h>
#include <unistd.h>

char * render_static_file(char * fileName) {
	FILE* file = fopen(fileName, "r");

	if (file == NULL) {
		return NULL;
	}else {
		printf("%s does exist \n", fileName);
	}

	fseek(file, 0, SEEK_END);
	long fsize = ftell(file);
	fseek(file, 0, SEEK_SET);

	char* temp = malloc(sizeof(char) * (fsize+1));
	char ch;
	int i = 0;
	while((ch = fgetc(file)) != EOF) {
		temp[i] = ch;
		i++;
	}
	fclose(file);
	return temp;
}

void *handle_connection(void *connectioninfo) {
	ConnectionInfo* info = (ConnectionInfo*) connectioninfo;

	char client_msg[4096] = "";

    char buffer[1024] = {0};
    int valread;
    
    // read data from the client
    valread = read(info->socket_desc, buffer, 1024);
    
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
			struct Route * destination = search(info->route, urlRoute);
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

		send(info->socket_desc, http_header, sizeof(http_header), 0);
    
    // close the client socket
    close(info->socket_desc);
    
    // free the socket descriptor pointer
    free(info);
    
    // exit the thread
    pthread_exit(NULL);
}
