/*
 * Created on Thu May 11 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *         Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

typedef struct {
    char *url; // url resource
    char ct[20]; // creationTime
    short ty; // resourceType
    char et[20]; // expirationTime
    char *json_lbl;
    char pi[10]; // parentID
    char aa[50]; // Announced Atribute 
    char rn[50]; // resourceName
    char ri[10]; // resourceID
    char *json_at; // Announce To
    char or[50]; // Ontology Ref
    char lt[20]; // lastModifiedTime
    char *blob;
    short st;  // stateTag
    char cnf[20]; // contentInfo
    short cs; // contentSize
    char cr[50]; // contentRef
    char *con; // content
    short dc; // deletionCnt
    char dr[20]; // dataGenerationTime
} CINStruct;

CINStruct *init_cin();
char create_cin(CINStruct * cin, cJSON *content, char** response);

cJSON *cin_to_json(const CINStruct *cin);

char update_cin(struct Route* destination, cJSON *content, char** response);

char get_cin(struct Route* destination, char** response);