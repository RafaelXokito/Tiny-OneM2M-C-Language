typedef struct {
    short ty; // resourceType
    char ri[50]; // resourceID
    char rn[50]; // resourceName
    char pi[50]; // parentID
    char ct[25]; // creationTime
    char lt[25]; // lastModifiedTime
} CSEBaseStruct;

char init_cse_base(CSEBaseStruct * csebase, struct sqlite3 * db, char isTableCreated);

char getLastCSEBaseStruct(CSEBaseStruct * csebase, sqlite3 *db);

cJSON *csebase_to_json(const CSEBaseStruct *csebase);