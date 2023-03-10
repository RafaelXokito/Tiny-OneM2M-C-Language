#include "Routes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct Route * initRoute(char* key, short ty, char* value) {
	struct Route * temp = (struct Route *) malloc(sizeof(struct Route));

	temp->key = (char*) malloc(strlen(key) + 1);
	strcpy(temp->key, key);

	temp->ty = ty;
	
	temp->value = (char*) malloc(strlen(value) + 1);
	strcpy(temp->value, value);

	temp->left = temp->right = NULL;
	return temp;
}

void inorder(struct Route* root)
{
    if (root != NULL) {
        inorder(root->left);
        printf("%s -> %d -> %s \n", root->key, root->ty, root->value);
        inorder(root->right);
    }
}

struct Route * addRoute(struct Route * root, char* key, short ty, char* value) {
	if (root == NULL) {
		return initRoute(key, ty, value);
	}

	if (strcmp(key, root->key) == 0) {
		printf("============ WARNING ============\n");
		printf("A Route For \"%s\" Already Exists\n", key);
	}else if (strcmp(key, root->key) > 0) {
		root->right = addRoute(root->right, key, ty, value);
	}else {
		root->left = addRoute(root->left, key, ty, value);
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

