#include "Common.h"
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
    read(info->socket_desc, client_msg, 1024);
    
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

	struct Route * destination = search(info->route, urlRoute);

	printf("Check if route was founded\n");
	if (destination == NULL) {
		char template[100] = "templates/";

		strcat(template, "404.html");
		char * response_data = render_static_file(template);
		char response[4096] = "HTTP/1.1 404 Not Found\r\n\r\n";
		strcat(response, response_data);
		strcat(response, "\r\n\r\n");

		printf("http_header: %s\n", response);

		send(info->socket_desc, response, sizeof(response), 0);

		// close the client socket
		close(info->socket_desc);
		// free the socket descriptor pointer
		free(info);
		// exit the thread
		pthread_exit(NULL);
	}

	printf("Check if is the default route\n");
	if (strcmp(destination->key, "/") == 0) {
		char template[100] = "templates/";

		strcat(template, destination->value);
		char * response_data = render_static_file(template);

		char response[4096] = "HTTP/1.1 200 OK\r\n\r\n";
		strcat(response, response_data);
		strcat(response, "\r\n\r\n");

		printf("http_header: %s\n", response);

		send(info->socket_desc, response, sizeof(response), 0);

		// close the client socket
		close(info->socket_desc);
		// free the socket descriptor pointer
		free(info);
		// exit the thread
		pthread_exit(NULL);
	}

	// Creating the response
	char response[4096] = "";
	
	if (strcmp(method, "GET") == 0) {
        // Get Request
		char *sql = sqlite3_mprintf("SELECT * FROM mtc WHERE ri = '%s' AND ty = %d;", destination->ri, destination->ty);
		printf("%s\n",sql);
		sqlite3_stmt *stmt;
		struct sqlite3 * db = initDatabase("tiny-oneM2M.db");
		short rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
		if (rc != SQLITE_OK) {
			printf("Failed to prepare statement: %s\n", sqlite3_errmsg(db));
			sqlite3_finalize(stmt);
			sqlite3_close(db);
			// close the client socket
			close(info->socket_desc);
			// free the socket descriptor pointer
			free(info);
			// exit the thread
			pthread_exit(NULL);
		}

		printf("Creating the json object\n");
		cJSON *root = cJSON_CreateArray();
		while (sqlite3_step(stmt) == SQLITE_ROW) {
			CSEBase csebase;
			csebase.ty = sqlite3_column_int(stmt, 0);
			strncpy(csebase.ri, (char *)sqlite3_column_text(stmt, 1), 50);
			strncpy(csebase.rn, (char *)sqlite3_column_text(stmt, 2), 50);
			strncpy(csebase.pi, (char *)sqlite3_column_text(stmt, 3), 50);
			strncpy(csebase.ct, (char *)sqlite3_column_text(stmt, 4), 25);
			strncpy(csebase.lt, (char *)sqlite3_column_text(stmt, 5), 25);
			cJSON_AddItemToArray(root, csebase_to_json(&csebase));
			break;
		}

		printf("Coonvert to json string\n");
		char *json_str = cJSON_PrintUnformatted(root);
		printf("%s\n", json_str);

		char * response_data = json_str;
		
		strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");

		strcat(response, response_data);

		cJSON_Delete(root);
		sqlite3_finalize(stmt);
		sqlite3_close(db);
		free(json_str);
    } else if (strcmp(method, "POST") == 0) {
        printf("Command 2\n");
    }

	printf("response: %s\n", response);

    int response_len = strlen(response);

	send(info->socket_desc, response, response_len, 0);

    // close the children socket
    close(info->socket_desc);
    
    // free the socket descriptor pointer
    free(info);
    
    // exit the thread
    pthread_exit(NULL);
}
