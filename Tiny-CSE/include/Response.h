/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include <stdlib.h>

typedef struct {
    int socket_desc;
    struct Route * route;
} ConnectionInfo;

char * render_static_file(char* fileName);

void *handle_connection(void *connectioninfo);

void responseMessage(char** response, int status_code, char* status_message, char* message);

cJSON *get_json_from_request(const char *request);
cJSON *get_first_child(cJSON *json_object);
