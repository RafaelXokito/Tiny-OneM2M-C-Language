#include "CSE_Base.h"
#include "AE.h"

#include "Types.h"

#define MIXED   0
#define ACP     1
#define AE      2
#define CNT     3
#define CIN     4
#define CSEBASE 5
#define GRP     9
#define MGMTOBJ 13
#define NOD     14
#define PCH     15
#define CSR     16
#define REQ     17
#define SUB     23
#define SMD     24
#define FCNT    28
#define TS      29
#define TSI     30
#define CRS     48
#define FCI     58
#define TSB     60
#define ACTR    63

char init_protocol(struct Route** head);
char retrieve_csebase(struct Route * destination, char *response);
char create_ae(struct Route** route, struct Route* destination, cJSON *content, char* response);
char retrieve_ae(struct Route * destination, char *response);
char validate_keys(cJSON *object, char *keys[], int num_keys, char *response);
char delete_resource(struct Route * destination, char *response);
char update_ae(struct Route* destination, cJSON *content, char* response);
char insert_multivalue_element(cJSON *element, const char *mtc_id, int parent_id, const char *key, sqlite3 *db);
static int insert_element_into_multivalue_table(sqlite3 *db, const char *mtc_ri, int parent_id, const char *key, const char *value, const char *type);
char insert_multivalue_elements(sqlite3 *db, const char *parent_ri, const char *key, cJSON *atr_array);
char *get_element_value_as_string(cJSON *element);