/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <pthread.h>
#include "sqlite3.h"
#include <unistd.h>
#include <regex.h>

#include "Common.h"

extern char BASE_RI[MAX_CONFIG_LINE_LENGTH];

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

void responseMessage(char** response, int status_code, char* status_message, char* message) {
    printf("Creating the json response\n");

    // Calculate the required response size
    size_t response_size = snprintf(NULL, 0, "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n\r\n{\"status_code\": %d, \"message\":\"%s\"}", status_code, status_message, status_code, message) + 1;

    // Allocate memory for the response
    *response = (char *)malloc(response_size * sizeof(char));
    if (*response == NULL) {
        // Handle memory allocation error
        fprintf(stderr, "Memory allocation error\n");
        return;
    }

    // Fill in the response buffer
    sprintf(*response, "HTTP/1.1 %d %s\r\nContent-Type: application/json\r\n\r\n{\"status_code\": %d, \"message\":\"%s\"}", status_code, status_message, status_code, message);
}

void close_socket_and_exit(ConnectionInfo *info) {
    close(info->socket_desc);
    free(info);
    pthread_exit(NULL);
}

void handle_get(ConnectionInfo *info, const char *queryString, struct Route *destination, char **response) {
	
	if (queryString != NULL && strlen(queryString) > 0 && strstr(queryString, "fu=1") != NULL) {
		char rs = discovery(info->route, destination, queryString, response);
		if (rs == FALSE) {
			responseMessage(response,500,"Internal Server Error","Error retrieving the data");
			fprintf(stderr,"Could not discover resource\n");
		}
		return;
	}
    switch (destination->ty) {
		case CSEBASE: {
			char rs = retrieve_csebase(destination,response);
			if (rs == FALSE) {
				responseMessage(response,500,"Internal Server Error","Error retrieving the data");
				fprintf(stderr,"Could not retrieve CSE_Base resource\n");
			}
			break;
			}
		case AE: {
			char rs = retrieve_ae(destination,response);
			if (rs == FALSE) {
				responseMessage(response,500,"Internal Server Error","Error retrieving the data");
				fprintf(stderr,"Could not retrieve AE resource\n");
			}
			break;
			}
		case CNT: {
			char rs = retrieve_cnt(destination,response);
			if (rs == FALSE) {
				responseMessage(response,500,"Internal Server Error","Error retrieving the data");
				fprintf(stderr,"Could not retrieve CNT resource\n");
			}
			break;
			}
		case CIN: {
			char rs = retrieve_cin(destination,response);
			if (rs == FALSE) {
				responseMessage(response,500,"Internal Server Error","Error retrieving the data");
				fprintf(stderr,"Could not retrieve CIN resource\n");
			}
			break;
			}
		case SUB: {
			char rs = retrieve_sub(destination,response);
			if (rs == FALSE) {
				responseMessage(response,500,"Internal Server Error","Error retrieving the data");
				fprintf(stderr,"Could not retrieve CIN resource\n");
			}
			break;
			}
		default:
			break;
	}
}

void handle_post(ConnectionInfo *info, const char *request, struct Route *destination, char **response) {
	cJSON *json_object = get_json_from_request(request);

	if (json_object == NULL) {
        responseMessage(response, 400, "Bad Request", "Invalid request body");
        fprintf(stderr, "JSON data not found.\n");
    } else {
		cJSON *first = get_first_child(json_object);
		if (first != NULL) {
            // Print the key and value
			char* pattern = "^m2m:.*$";  // Regex pattern to match
			// Compile the regex pattern
			regex_t regex;
			int ret = regcomp(&regex, pattern, 0);
			if (ret) {
				responseMessage(response,500,"Internal Server Error","Error compiling regex");
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
					fprintf(stderr, "Error while getting the content from request\n");
					responseMessage(response,500,"Internal Server Error","Error while getting the content from request");
				} else {
					// If everything was ok, we proced to the creation of resource

					// Verify if is it possible to create the child inside the destination
					if (destination->ty == CSEBASE && !(ty == ACP || ty == AE || ty == CNT || ty == GRP || ty == NOD || ty == FCNT || ty == SUB) ) {
						responseMessage(response,400,"Bad Request","Invalid children type.");
						fprintf(stderr, "Could not create inside CSEBASE resource. Invalid children type.\n");
						return;
					}
					
					if (destination->ty == AE && !(ty == CNT || ty == FCNT || ty == GRP || ty == SUB) ) {
						responseMessage(response,400,"Bad Request","Invalid children type.");
						fprintf(stderr, "Could not create inside AE resource. Invalid children type.\n");
						return;
					}
					
					if (destination->ty == CNT && !(ty == CNT || ty == CIN || ty == SUB) ) {
						responseMessage(response,400,"Bad Request","Invalid children type.");
						fprintf(stderr, "Could not create inside CNT resource. Invalid children type.\n");
						return;
					}

					if (destination->ty == CIN) {
						responseMessage(response,400,"Bad Request","Invalid children type.");
						fprintf(stderr, "Could not create inside CIN resource.\n");
						return;
					}

					if (destination->ty == SUB && !(ty == SUB) ) {
						responseMessage(response,400,"Bad Request","Invalid children type.");
						fprintf(stderr, "Could not create inside SUB resource.\n");
						return;
					}

					switch (ty) {
					case AE: {
						char rs = post_ae(&info->route, destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not create AE resource\n");
						}
						break;
					}
					case CNT: {
						char rs = post_cnt(&info->route, destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not create CNT resource\n");
						}
						break;
					}
					case CIN: {
						char rs = post_cin(&info->route, destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not create CIN resource\n");
						}
						break;
					}
					case SUB: {
						char rs = post_sub(&info->route, destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not create SUB resource\n");
						}
						break;
					}
					default:
						responseMessage(response,400,"Bad Request","Invalid resource");
						fprintf(stderr, "Theres no available resource for %s\n", key);
						break;
					}
				}
			} else if (ret == REG_NOMATCH) {
				responseMessage(response,400,"Bad Request","Invalid json root name, should match m2m:<resource> (e.g m2m:ae)");
				fprintf(stderr, "Invalid json root name\n");
			} else {
				char buf[100];
				regerror(ret, &regex, buf, sizeof(buf));
				responseMessage(response,500,"Bad Request","Error matching regex");
				fprintf(stderr, "Error matching regex: %s\n", buf);
			}
        } else {
            responseMessage(response, 400, "Bad Request", "Invalid request body");
            fprintf(stderr, "Object is empty\n");
        }
	}
}

void handle_delete(ConnectionInfo *info, struct Route *destination, char **response) {
	if (strcmp(destination->ri, BASE_RI) == 0) {
		responseMessage(response,400,"Bad Request","Invalid resource.");
		fprintf(stderr, "Could not delete CSEBASE resource.\n");
		return;
	}
	
	delete_resource(destination, response);
}

void handle_put(ConnectionInfo *info, const char *request, struct Route *destination, char **response) {
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
				responseMessage(response,500,"Internal Server Error","Error compiling regex");
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
					fprintf(stderr, "Error while getting the content from request\n");
					responseMessage(response,500,"Internal Server Error","Error while getting the content from request");
				} else {
					// If everything was ok, we proced to the creation of resource
					switch (ty) {
					case AE: {
						char rs = put_ae(destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not update AE resource\n");
						}
						break;
					}
					case CNT: {
						char rs = put_cnt(destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not update CNT resource\n");
						}
						break;
					}
					case SUB: {
						char rs = put_sub(destination, content, response);
						if (rs == FALSE) {
							// The method it self already change the response properly
							fprintf(stderr, "Could not update SUB resource\n");
						}
						break;
					}
					default:
						responseMessage(response,400,"Bad Request","Invalid resource");
						fprintf(stderr, "Theres no available resource for %s\n", key);
						break;
					}
				}
			} else if (ret == REG_NOMATCH) {
				responseMessage(response,400,"Bad Request","Invalid json root name, should match m2m:<resource> (e.g m2m:ae)");
				fprintf(stderr, "Invalid json root name\n");
			} else {
				char buf[100];
				regerror(ret, &regex, buf, sizeof(buf));
				responseMessage(response,500,"Bad Request","Error matching regex");
				fprintf(stderr, "Error matching regex: %s\n", buf);
			}
		} else {
			responseMessage(response,400,"Bad Request","Invalid request body");
			fprintf(stderr, "Object is empty\n");
		}

	} else {
		responseMessage(response,400,"Bad Request","Invalid request body");
		fprintf(stderr, "JSON data not found.\n");
	}
}

void *handle_connection(void *connectioninfo) {
    ConnectionInfo* info = (ConnectionInfo*) connectioninfo;

    printf("\n");

    char client_msg[4096] = "";

    int valread;

    // read data from the client
    valread = read(info->socket_desc, client_msg, sizeof(client_msg));
    if (valread < 0) {
        perror("read");
        close_socket_and_exit(info);
    }

    char request[4096];
    strncpy(request, client_msg, sizeof(request) - 1);
    request[sizeof(request) - 1] = '\0';

    // parsing client socket header to get HTTP method, route
    char *method = "";
    char *urlRoute = "";

    char *client_http_header = strtok(client_msg, "\n");

    char *header_token = strtok(client_http_header, " ");

    int header_parse_counter = 0;

    while (header_token != NULL) {

        switch (header_parse_counter) {
            case 0:
                method = header_token;
                break;
            case 1:
                urlRoute = header_token;
                break;
        }
        header_token = strtok(NULL, " ");
        header_parse_counter++;
    }

    to_lowercase(urlRoute);

	char *queryString = strtok(urlRoute, "?");
	
	urlRoute = queryString;
	queryString = strtok(NULL, "?");

    printf("The method is %s\n", method);
    printf("The route is %s\n", urlRoute);
	if (queryString != NULL && strcmp(queryString, "") != 0) {
		printf("The query string is %s\n", queryString);
	}

    struct Route * destination = search(info->route, urlRoute);

    printf("Check if route was founded\n");
    if (destination == NULL) {
        char *response = NULL;

        responseMessage(&response, 404, "Not found", "Resource not found");

        printf("http_header: %s\n", response);

        send(info->socket_desc, response, strlen(response), 0);

        close_socket_and_exit(info);
    }

    printf("Check if is the default route\n");
    if (strcmp(destination->key, "/") == 0) {
        char template[100] = "templates/";

        strncat(template, destination->value, sizeof(template) - strlen(template) - 1);
        char * response_data = render_static_file(template);
		
        char response[4096] = "HTTP/1.1 200 OK\r\n\r\n";
        strncat(response, response_data, sizeof(response) - strlen(response) - 1);
        strncat(response, "\r\n\r\n", sizeof(response) - strlen(response) - 1);

        send(info->socket_desc, response, strlen(response), 0);
        close_socket_and_exit(info);
    }

    // Creating the response
    char *response = NULL;

	printf("Check the HTTP method\n");
    if (strcmp(method, "GET") == 0) {
        handle_get(info, queryString, destination, &response);
	} else if (strcmp(method, "POST") == 0) {
        handle_post(info, request, destination, &response);
    } else if (strcmp(method, "PUT") == 0) {
        handle_put(info, request, destination, &response);
    } else if (strcmp(method, "DELETE") == 0) {
        handle_delete(info, destination, &response);
    } else {
        responseMessage(&response, 405, "Method Not Allowed", "HTTP method not supported");
    }

    send(info->socket_desc, response, strlen(response), 0);

    close_socket_and_exit(info);
	free(response);
    return NULL;
}

cJSON *get_json_from_request(const char *request) {
    char *json_start = strstr(request, "{");
    if (json_start == NULL) {
        return NULL;
    }

    size_t json_length = strlen(json_start);
    char *json_data = (char *)malloc(json_length + 1);
    if (json_data == NULL) {
        return NULL;
    }

    strncpy(json_data, json_start, json_length);
    json_data[json_length] = '\0';

    cJSON *json_object = cJSON_Parse(json_data);
    free(json_data);
    return json_object;
}

cJSON *get_first_child(cJSON *json_object) {
    if (json_object == NULL) {
        return NULL;
    }

    return json_object->child;
}