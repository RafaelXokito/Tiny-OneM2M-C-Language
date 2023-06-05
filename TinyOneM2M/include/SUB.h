/*
 * Created on Thu May 18 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *         Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

typedef struct {
    short ty; // resourceType
    char ri[10]; // resourceID
    char rn[50]; // resourceName
    char pi[10]; // parentID
    char et[20]; // expirationTime
    char lt[20]; // lastModifiedTime
    char ct[20]; // creationTime
    char *json_lbl; // labels
    char *json_acpi; // Access Control Policy IDs
    char *json_daci; // Dynamic Authorization Consultation IDs
    char *url; // url resource
    char enc[50]; // eventNotificationCriteria
    char *json_nu; // notificationURI; List of urls to send the notifications to
    // Can be /CSE0001/AE0001 that will get the POA and send or can be this format as well "https://172.25.30.25:7000/notification/handler".
    char *blob;
} SUBStruct;

SUBStruct *init_sub();
char create_sub(SUBStruct * sub, cJSON *content, char** response);

cJSON *sub_to_json(const SUBStruct *sub);

char update_sub(struct Route* destination, cJSON *content, char** response);

char get_sub(struct Route* destination, char** response);