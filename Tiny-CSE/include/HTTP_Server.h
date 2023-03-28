/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */


#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

typedef struct HTTP_Server {
	int socket;
	int port;	
} HTTP_Server;


void init_server(HTTP_Server* http_server, int port);

#endif
