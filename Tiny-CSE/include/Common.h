#define _XOPEN_SOURCE 700

#include <pthread.h>
#include <string.h>

#include "HTTP_Server.h"
#include "cJSON.h"
#include "Sqlite.h"
#include "Response.h"
#include "Signals.h"
#include "Utils.h"
#include "Routes.h"
#include "MTC_Protocol.h"

#define TRUE 1
#define FALSE 0


    