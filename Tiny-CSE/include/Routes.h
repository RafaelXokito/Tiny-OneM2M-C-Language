/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#include <stdlib.h>
#include <string.h>

struct Route {
	char* key;
	char* ri; // resource ID
	short ty; // resource type
	char* value;

	struct Route *left, *right;
};

struct Route * initRoute(char* key, char* ri, short ty, char* value);

struct Route *addRoute(struct Route **head, char *key, char *ri, short ty, char *value);

struct Route* search(struct Route *root, const char *key);
struct Route* search_byri(struct Route* head, const char* ri);
struct Route * search_byrn_ty(struct Route * root, char* rn, short ty);
char* get_last_ri_of_type(struct Route* head, short target_type);

void inorder(struct Route * root );

char init_routes(struct Route** head);