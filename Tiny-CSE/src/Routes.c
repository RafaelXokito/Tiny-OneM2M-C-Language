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
#include <stdio.h>

#include "Common.h"

struct Route * initRoute(char* key, char* ri, short ty, char* value) {
	struct Route * temp = (struct Route *) malloc(sizeof(struct Route));

	temp->key = (char*) malloc(strlen(key) + 1);
	strcpy(temp->key, key);
    to_lowercase(temp->key);

	temp->ri = (char*) malloc(strlen(ri) + 1);
	strcpy(temp->ri, ri);

	temp->ty = ty;
	
	temp->value = (char*) malloc(strlen(value) + 1);
	strcpy(temp->value, value);

	temp->left = temp->right = NULL;
	return temp;
}

// function to recursively construct the string for a resource
//__deprecated
char * constructPath(char * result, char * resourceName, char * parentName, struct sqlite3 *db) {
	/*
	#pseudo-code from the whole algorithm#

	select every records by resourceName and parentName
	iterate every record that have parentName
		calls a function that do:
			initializate a blank string
			if parentName exists
				get the parent record
				calls it self with the parent data
				concatenate the resourceName with the string
			else
				return resourceName
			return string concatenated
	*/
	if (strcmp("", parentName) != 0) {
		// get the parent record
		char *sql = sqlite3_mprintf("SELECT rn, pi FROM mtc WHERE ri='%s'", parentName);
		sqlite3_stmt *stmt;
		int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
		sqlite3_free(sql);
		if(rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
			exit(0);
		}
		
		if(sqlite3_step(stmt) == SQLITE_ROW) {

			// calls it self with the parent data
			char * parentResourceName = (char *) sqlite3_column_text(stmt, 0);
			char * parentParentId = (char *) sqlite3_column_text(stmt, 1);
			strcat(result, constructPath(result, parentResourceName,parentParentId,db));
			// concatenate the resourceName with the string
		} else {
			fprintf(stderr, "Resource not found: %s\n", parentName);
			sqlite3_finalize(stmt);
			exit(0);
		}
	} else {
    	strcat(result, "/");
		return resourceName;
	}
	strcat(result, "/");
	strcat(result, resourceName);
	return result;
}

char init_routes(struct Route** head) {
    printf("Initializing routes\n");
    // call the constructPath function for each resource in the database

	// Sqlite3 initialization opening/creating database
	sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return FALSE;
	}

    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT ri, pi, ty, rn, url FROM mtc;", -1, &stmt, 0);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return FALSE;
    }
    
    while(sqlite3_step(stmt) == SQLITE_ROW) {
		if (strcmp("", (char *) sqlite3_column_text(stmt, 1)) == 0) {
			continue;
		}
		

		char * resourceName = (char *) sqlite3_column_text(stmt, 3);
		printf("Initializing route: %s\n", resourceName);
        char * parentName = (char *) sqlite3_column_text(stmt, 1);
        // constructPath(uri, resourceName, parentName, db);
		char * uri = (char *) sqlite3_column_text(stmt, 4);

		char * resourceId = (char *) sqlite3_column_text(stmt, 0);
		short resourceType = sqlite3_column_int(stmt, 2);
		
		// Add New Routes
		to_lowercase(uri);
		addRoute(head, uri, resourceId, resourceType, resourceName);

		// when we are creating CNT routes we need to make available the route 'la' and 'li' routes
		if (resourceType == CNT) {
			// Creating ol(dest) and la(test) routes
			char *url_ol;
			char *url_la;

			// Allocate memory for the new URLs
			url_ol = malloc(strlen(uri) + strlen("/ol") + 1);  // +1 for the null-terminator
			url_la = malloc(strlen(uri) + strlen("/la") + 1);  // +1 for the null-terminator

			if (url_ol == NULL || url_la == NULL) {
				fprintf(stderr, "Memory allocation failed. \n'la' and 'li' CNT routes not available\n");
				return FALSE;
			}

			// Copy the original URL and append /ol and /la
			sprintf(url_ol, "%s/ol", uri);
			sprintf(url_la, "%s/la", uri);

			addRoute(head, url_ol, resourceId, CIN, "ol");
			addRoute(head, url_la, resourceId, CIN, "la");
		}
		

		printf("Route created: %s\n", uri);
    }

    sqlite3_finalize(stmt);

	// The DB connection should exist in each thread and should not be shared
    if (closeDatabase(db) == FALSE) {
        fprintf(stderr, "Error closing database.\n");
        return FALSE;
    }

	return TRUE;
}

void inorder(struct Route* head)
{
    struct Route* current = head;

    while (current != NULL) {
        printf("%s -> %s -> %d -> %s \n", current->key, current->ri, current->ty, current->value);
        current = current->right;
    }
}

int count_same_types(struct Route* head, int type) {
    int count = 0;
    struct Route* current = head;
    while (current != NULL) {
        if (current->ty == type) {
            count++;
        }
        current = current->right;
    }
    return count;
}

struct Route * addRoute(struct Route **head, char* key, char* ri, short ty, char* value) {
	// create a new node with the given fields
	struct Route *newNode = initRoute(key, ri, ty, value);

	// check if the list is empty
	if (*head == NULL) {
		*head = newNode;
		return newNode;
	}

	// traverse the list to find the correct position to insert the new node
	struct Route *current = *head;
	while (current != NULL) {
		if (strcmp(key, current->key) == 0) {
			printf("A Route For \"%s\" Already Exists\n", key);
			return current; // return the existing node with the same key
		} else if (strcmp(key, current->key) > 0) {
			// move to the next node in the list
			if (current->right == NULL) {
				// insert the new node at the end of the list
				current->right = newNode;
				newNode->left = current;
				return newNode;
			}
			current = current->right;
		} else {
			// insert the new node before the current node
			if (current->left == NULL) {
				// insert the new node at the beginning of the list
				newNode->right = current;
				current->left = newNode;
				*head = newNode;
				return newNode;
			} else {
				current->left->right = newNode;
				newNode->left = current->left;
				newNode->right = current;
				current->left = newNode;
				return newNode;
			}
		}
	}
	return NULL;
}

struct Route* search(struct Route *root, const char *key) {
    struct Route *current = root;
    while (current != NULL) {
        if (strcmp(key, current->key) == 0) {
            return current;
        } else if (strcmp(key, current->key) > 0) {
            current = current->right;
        } else {
            break;
        }
    }
    return NULL;
}

struct Route* search_byri(struct Route* head, const char* ri) {
    struct Route* current = head;

    while (current != NULL) {
        if (strcmp(current->ri, ri) == 0) {
            return current;
        }
        current = current->right;
    }

    return NULL;
}

struct Route * search_byrn_ty(struct Route * root, char* rn, short ty) {
	if (root == NULL) {
		return NULL;
	}
	
	if (ty == root->ty && strcmp(rn, root->value) == 0){
		return root;
	} else {
		// Temos de garantir que o root que chega aqui é o primeiro, se não isto não faz sentido e teria de ser garantido de outra maneira
		return search_byrn_ty(root->right, rn, ty);
	}

	return root;
}

