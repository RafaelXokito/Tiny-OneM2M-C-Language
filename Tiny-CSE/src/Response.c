#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sqlite3.h>
#include <unistd.h>
#include <regex.h>

#include "Common.h"

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

void responseMessage(char* response, int status_code, char* status_message, char* message) {
        printf("Creating the json response\n");

        sprintf(response, "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n\r\n{\"status_code\": %d, \"message\":\"%s\"}", status_code, status_message, status_code, message);
}

void *handle_connection(void *connectioninfo) {
	ConnectionInfo* info = (ConnectionInfo*) connectioninfo;

	char client_msg[4096] = "";

    char buffer[1024] = {0};
    int valread;
    
    // read data from the client
    read(info->socket_desc, client_msg, 1024);
	char request[1024];
	strcpy(request, client_msg);

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

	to_lowercase(urlRoute);
	printf("The method is %s\n", method);
	printf("The route is %s\n", urlRoute);

	struct Route * destination = search(info->route, urlRoute);

	printf("Check if route was founded\n");
	if (destination == NULL) {
		char response[4096] = "";

        responseMessage(response,404,"Not found","Resource not found");

        printf("http_header: %s\n", response);

        send(info->socket_desc, response, strlen(response), 0);

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
		switch (destination->ty)
		{
		case CSEBASE: {
			char rs = retrieve_csebase(destination,response);
			if (rs == false) {
				// TODO - Error 400 Bad request
				printf("Could not retrieve CSE_Base resource\n");
				// close the client socket
				close(info->socket_desc);
				// free the socket descriptor pointer
				free(info);
				// exit the thread
				pthread_exit(NULL);
			}
			break;
			}
		case AE: {
			char rs = retrieve_ae(destination,response);
			if (rs == false) {
				// TODO - Error 400 Bad request
				printf("Could not retrieve AE resource\n");
				// close the client socket
				close(info->socket_desc);
				// free the socket descriptor pointer
				free(info);
				// exit the thread
				pthread_exit(NULL);
			}
			}
			break;
		default:
			break;
		}
		
    } else if (strcmp(method, "POST") == 0) {
		char* json_start = strstr(request, "{"); // find the start of the JSON data
		if (json_start != NULL) {
			size_t json_length = strlen(json_start); // calculate the length of the JSON data
			char json_data[json_length + 1]; // create a buffer to hold the JSON data
			strncpy(json_data, json_start, json_length); // copy the JSON data to the buffer
			json_data[json_length] = '\0'; // add a null terminator to the end of the buffer

			// Parse the JSON string into a cJSON object
    		cJSON* json_object = cJSON_Parse(json_data);

			// Retrieve the first key-value pair in the object
			cJSON* first = json_object->child;
			if (first != NULL) {
				// Print the key and value
				printf("First key: %s\n", first->string);

				char* pattern = "^m2m:.*$";  // Regex pattern to match
				// Compile the regex pattern
				regex_t regex;
				int ret = regcomp(&regex, pattern, 0);
				if (ret) {
					// TODO - Error bad request
					fprintf(stderr, "Error compiling regex\n");
				}

				// Test if the string matches the regex pattern
				ret = regexec(&regex, first->string, 0, NULL, 0);
				if (!ret) {
					printf("String matches regex pattern\n");
					// first->string comes like "m2m:ae"
					char aux[50];
					strcpy(aux, first->string);
					char *key = strtok(aux, ":");
					key = strtok(NULL, ":");
					to_lowercase(key);
					// search in the types hash table for the 'ty' (resourceType) by the key (resourceName)
					short ty = search_type(&types, key);
					
					// Get the JSON string from a specific element (let's say "age")
					cJSON *content = cJSON_GetObjectItemCaseSensitive(json_object, first->string);
					if (content == NULL) {
						// TODO - Error 500
						fprintf(stderr, "Error while getting the content from request\n");
					}

					switch (ty) {
					case AE: {
						strcpy(response, "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\n\r\n");
						char rs = create_ae(info->route, destination, content, response);
						if (rs == false) {
							// TODO - Error 400 Bad request
							printf("Could not create AE resource\n");
						}
						break;
					}
					default:
						// TODO - Error bad request
						printf("Theres no available resource for %s\n", key);
						break;
					}
				} else if (ret == REG_NOMATCH) {
					// TODO - Error bad request
					printf("String does not match regex pattern\n");
				} else {
					char buf[100];
					regerror(ret, &regex, buf, sizeof(buf));
					// TODO - Error 500
					fprintf(stderr, "Error matching regex: %s\n", buf);
				}
			} else {
				// TODO - Error bad request
				printf("Object is empty\n");
			}

		} else {
			// TODO - Error bad request
			printf("JSON data not found.\n");
		}
    }

	printf("response: %s\n\n", response);

    int response_len = strlen(response);

	send(info->socket_desc, response, response_len, 0);

    // close the children socket
    close(info->socket_desc);
    
    // free the socket descriptor pointer
    free(info);
    
    // exit the thread
    pthread_exit(NULL);
}
