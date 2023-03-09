#include <stdlib.h>

char * render_static_file(char* fileName);

void *handle_connection(void *connectioninfo);

typedef struct {
    int socket_desc;
    struct Route * route;
} ConnectionInfo;