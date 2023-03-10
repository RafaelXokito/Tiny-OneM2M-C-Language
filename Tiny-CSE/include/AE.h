typedef struct {
    short ty; // resourceType
    char ri[50]; // resourceID
    char rn[50]; // resourceName
    char pi[50]; // parentID
    char et[25]; // expirationTime
    char ct[25]; // creationTime
    char lt[25]; // lastModifiedTime
} AEStruct;

char init_ae(AEStruct * ae, cJSON *content, struct sqlite3 * db);

cJSON *ae_to_json(const AEStruct *ae);