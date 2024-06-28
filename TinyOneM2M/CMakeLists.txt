cmake_minimum_required(VERSION 3.28)
project(Tiny_OneM2M_C_Language C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

add_executable(Tiny_OneM2M_C_Language
        include/AE.h
        include/CIN.h
        include/cJSON.h
        include/CNT.h
        include/Common.h
        include/CSE_Base.h
        include/HTTP_Server.h
        include/mongoose.h
        include/mqtt.h
        include/mqtt_pal.h
        include/MTC_Protocol.h
        include/posix_sockets.h
        include/Response.h
        include/Routes.h
        include/Signals.h
        include/Sqlite.h
        include/sqlite3.h
        include/SUB.h
        include/Types.h
        include/Utils.h
        src/AE.c
        src/CIN.c
        src/cJSON.c
        src/CNT.c
        src/CSE_Base.c
        src/HTTP_Server.c
        src/main.c
        src/mongoose.c
        src/mqtt.c
        src/mqtt_pal.c
        src/MTC_Protocol.c
        src/Response.c
        src/Routes.c
        src/Signal.c
        src/Sqlite.c
        src/sqlite3.c
        src/SUB.c
        src/Types.c
        src/Utils.c)
