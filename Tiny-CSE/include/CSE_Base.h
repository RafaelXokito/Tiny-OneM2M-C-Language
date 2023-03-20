typedef struct {
    short ty; // resourceType
    char ri[50]; // resourceID
    char rn[50]; // resourceName
    char pi[50]; // parentID
    short cst;
    char *json_srt;
    char *json_lbl;
    char csi[50];
    char nl[50];
    char *json_poa;
    char *json_acpi;
    char ct[25]; // creationTime
    char lt[25]; // lastModifiedTime
} CSEBaseStruct;

CSEBaseStruct *init_cse_base();

char create_cse_base(CSEBaseStruct * csebase, char isTableCreated);

char getLastCSEBaseStruct(CSEBaseStruct * csebase, sqlite3 *db);

cJSON *csebase_to_json(const CSEBaseStruct *csebase);