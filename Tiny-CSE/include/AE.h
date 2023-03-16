typedef struct {
    char apn[50]; // App Name
    char ct[20]; // creationTime
    short ty; // resourceType
    char **acpi; // Access Control Policy IDs
    char et[20]; // expirationTime
    char **lbl; // labels
    char pi[10]; // parentID
    char **daci; // Dynamic Authorization Consultation IDs
    char **poa; // Point of Access
    char **ch; // Childrens ??????
    char aa[50]; // Announced Atribute 
    char aei[5]; // AE ID
    char rn[50]; // resourceName
    char api[20]; // App ID
    char rr; // Request Reachability
    char csz[50]; // Content Serialization
    char ri[10]; // resourceID
    char nl[20]; // Node Link
    char **at; // Announce To
    char or[50]; // Ontology Ref
    char lt[20]; // lastModifiedTime
} AEStruct;


char init_ae(AEStruct * ae, cJSON *content, char* response);

cJSON *ae_to_json(const AEStruct *ae);