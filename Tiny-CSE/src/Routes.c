#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "Common.h"

struct Route * initRoute(char* key, char* ri, short ty, char* value) {
	struct Route * temp = (struct Route *) malloc(sizeof(struct Route));

	temp->key = (char*) malloc(strlen(key) + 1);
	to_lowercase(temp->key);
	strcpy(temp->key, key);

	temp->ri = (char*) malloc(strlen(ri) + 1);
	strcpy(temp->ri, ri);

	temp->ty = ty;
	
	temp->value = (char*) malloc(strlen(value) + 1);
	strcpy(temp->value, value);

	temp->left = temp->right = NULL;
	return temp;
}

// function to recursively construct the string for a resource
char * constructPath(char * result, char * resourceId, char * parentId, struct sqlite3 *db) {
	/*
	#pseudo-code from the whole algorithm#

	select every records by resourceId and parentId
	iterate every record that have parentId
		calls a function that do:
			initializate a blank string
			if parentId exists
				get the parent record
				calls it self with the parent data
				concatenate the resourceId with the string
			else
				return resourceId
			return string concatenated
	*/
	if (strcmp("", parentId) != 0) {
		// get the parent record
		char *sql = sqlite3_mprintf("SELECT ri, pi FROM mtc WHERE ri='%s'", parentId);
		sqlite3_stmt *stmt;
		int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, 0);
		sqlite3_free(sql);
		if(rc != SQLITE_OK) {
			fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
			exit(0);
		}
		
		if(sqlite3_step(stmt) == SQLITE_ROW) {

			// calls it self with the parent data
			char * parentResourceId = (char *) sqlite3_column_text(stmt, 0);
			char * parentParentId = (char *) sqlite3_column_text(stmt, 1);
			strcat(result, constructPath(result, parentResourceId,parentParentId,db));
			// concatenate the resourceId with the string
		} else {
			fprintf(stderr, "Resource not found: %s\n", parentId);
			sqlite3_finalize(stmt);
			exit(0);
		}
	} else {
    	strcat(result, "/");
		return resourceId;
	}
	strcat(result, "/");
	strcat(result, resourceId);
	return result;
}

char init_routes(struct Route* route) {
    printf("Initializing routes\n");
    // call the constructPath function for each resource in the database

	// Sqlite3 initialization opening/creating database
	sqlite3 *db;
    db = initDatabase("tiny-oneM2M.db");
    if (db == NULL) {
		return false;
	}

    sqlite3_stmt *stmt;
    short rc = sqlite3_prepare_v2(db, "SELECT ri, pi, ty, rn FROM mtc;", -1, &stmt, 0);
    if(rc != SQLITE_OK) {
        fprintf(stderr, "SQL error: %s\n", sqlite3_errmsg(db));
        return false;
    }
    
    while(sqlite3_step(stmt) == SQLITE_ROW) {
		if (strcmp("", (char *) sqlite3_column_text(stmt, 1)) == 0) {
			continue;
		}
		

		char * resourceId = (char *) sqlite3_column_text(stmt, 0);
		printf("Initializing route: %s\n", resourceId);
        char * parentId = (char *) sqlite3_column_text(stmt, 1);
		char uri[60] = "";
        constructPath(uri, resourceId, parentId, db);

		short resourceType = sqlite3_column_int(stmt, 2);
		char * resourceName = (char *) sqlite3_column_text(stmt, 3);
		
		// Add New Routes
		addRoute(route, uri, resourceId, resourceType, resourceName);

		printf("Route created: %s\n", uri);
    }

    sqlite3_finalize(stmt);

	// The DB connection should exist in each thread and should not be shared
    if (closeDatabase(db) == false) {
        fprintf(stderr, "Error closing database.\n");
        return false;
    }

	return true;
}

void inorder(struct Route* root)
{
    if (root != NULL) {
        inorder(root->left);
        printf("%s -> %s -> %d -> %s \n", root->key, root->ri, root->ty, root->value);
        inorder(root->right);
    }
}

struct Route * addRoute(struct Route * root, char* key, char* ri, short ty, char* value) {
	if (root == NULL) {
		return initRoute(key, ri, ty, value);
	}

	if (strcmp(key, root->key) == 0) {
		printf("============ WARNING ============\n");
		printf("A Route For \"%s\" Already Exists\n", key);
	}else if (strcmp(key, root->key) > 0) {
		root->right = addRoute(root->right, key, ri, ty, value);
	}else {
		root->left = addRoute(root->left, key, ri, ty, value);
	}

	return root;
}

struct Route * search(struct Route * root, char* key) {
	if (root == NULL) {
		return NULL;
	} 

	if (strcmp(key, root->key) == 0){
		return root;
	}else if (strcmp(key, root->key) > 0) {
		return search(root->right, key);
	}else if (strcmp(key, root->key) < 0) {
		return search(root->left, key);
	}  

	return root;
}

struct Route * search_byri(struct Route * root, char* ri) {
	if (root == NULL) {
		return NULL;
	} 

	if (strcmp(ri, root->ri) == 0){
		return root;
	}else if (strcmp(ri, root->ri) > 0) {
		return search(root->right, ri);
	}else if (strcmp(ri, root->ri) < 0) {
		return search(root->left, ri);
	}  

	return root;
}

struct Route * search_byrn_ty(struct Route * root, char* rn, short ty) {
	if (root == NULL) {
		return NULL;
	}
	
	if (ty == root->ty && strcmp(rn, root->value) == 0){
		return root;
	} else {
		return search_byrn_ty(root->right, rn, ty);
	}

	return root;
}

