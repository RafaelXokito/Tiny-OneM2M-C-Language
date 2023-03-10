#include "CSE_Base.h"
#include "AE.h"

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

char init_protocol(struct sqlite3 * db, struct Route* route);
char create_ae(struct Route* route);