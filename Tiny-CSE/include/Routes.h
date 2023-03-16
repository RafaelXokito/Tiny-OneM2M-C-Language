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

struct Route * search(struct Route * root, char * key);
struct Route * search_byri(struct Route * root, char* ri);
struct Route * search_byrn_ty(struct Route * root, char* rn, short ty);
int count_same_types(struct Route* head, int type);

void inorder(struct Route * root );

char init_routes(struct Route** head);