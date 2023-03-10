#import <sqlite3.h>

typedef struct {
    int ty; // resourceType
    char ri[50]; // resourceID
    char rn[50]; // resourceName
    char pi[50]; // parentID
    char ct[25]; // creationTime
    char lt[25]; // lastModifiedTime
} CSEBase;

char init_cse_base(CSEBase * csebase, struct sqlite3 * db, char isTableCreated);