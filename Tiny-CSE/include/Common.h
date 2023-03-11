#include "HTTP_Server.h"
#include "Response.h"
#include "Sqlite.h"
#include "Signals.h"
#include "cJSON.h"
#include "MTC_Protocol.h"
#include "Utils.h"
#include "Routes.h"

#include <pthread.h>
#include <string.h>

#define true 1
#define false 0
    