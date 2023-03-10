#include "HTTP_Server.h"
#include "Routes.h"
#include "Response.h"
#include "Sqlite.h"
#include "Signals.h"
#include "cJSON.h"
#include "MTC_Protocol.h"
#include "Utils.h"

#include <string.h>

#define true 1
#define false 0


#ifndef GLOBAL_H
#define GLOBAL_H

extern int client_socket; // declaration of the global variable

#endif // GLOBAL_H