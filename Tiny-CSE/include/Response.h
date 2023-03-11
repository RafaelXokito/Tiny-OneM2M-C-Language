#include <stdlib.h>

char * render_static_file(char* fileName);

void *handle_connection(void *connectioninfo);

void responseMessage(char* response, int status_code, char* status_message, char* message);

typedef struct {
    int socket_desc;
    struct Route * route;
} ConnectionInfo;