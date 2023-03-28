/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#define _XOPEN_SOURCE 700

#define TRUE 1
#define FALSE 0

#include <pthread.h>
#include <string.h>


#include "Utils.h"
#include "HTTP_Server.h"
#include "cJSON.h"
#include "Sqlite.h"
#include "Response.h"
#include "Signals.h"
#include "Routes.h"
#include "MTC_Protocol.h"



    