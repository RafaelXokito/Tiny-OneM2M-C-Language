#include "HTTP_Server.h"
#include "cJSON.h"
#include "Sqlite.h"
#include "Response.h"
#include "Signals.h"
#include "MTC_Protocol.h"
#include "Utils.h"
#include "Routes.h"

#include <pthread.h>
#include <string.h>

#define TRUE 1
#define FALSE 0


    