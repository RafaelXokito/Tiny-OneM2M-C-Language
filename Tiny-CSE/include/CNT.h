/*
 * Created on Mon Apr 24 2023
 *
 * Author(s): Rafcntl Pereira (Rafcntl_Pereira_2000@hotmail.com)
 *         Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

typedef struct {
    char *url; // url resource
    char apn[50]; // App Name
    char ct[20]; // creationTime
    short ty; // resourceType
    char *json_acpi; // Access Control Policy IDs
    char et[20]; // expirationTime
    char *json_lbl;
    char pi[10]; // parentID
    char *json_daci; // Dynamic Authorization Consultation IDs
    char *json_poa; // Point of Access
    char *json_ch; // Childrens ??????
    char aa[50]; // Announced Atribute 
    char cnti[10]; // CNT ID
    char rn[50]; // resourceName
    char api[20]; // App ID
    char rr[5]; // Request Reachability
    char csz[50]; // Content Serialization
    char ri[10]; // resourceID
    char nl[20]; // Node Link
    char *json_at; // Announce To
    char or[50]; // Ontology Ref
    char lt[20]; // lastModifiedTime
} CNTStruct;


CNTStruct *init_cnt();
char create_cnt(CNTStruct * cnt, cJSON *content, char** response);

cJSON *cnt_to_json(const CNTStruct *cnt);

char update_cnt(struct Route* destination, cJSON *content, char** response);

char get_cnt(struct Route* destination, char** response);