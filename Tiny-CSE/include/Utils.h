#define MAX_CONFIG_LINE_LENGTH 64  

char* getCurrentTime();
char* getCurrentTimeLong();
void to_lowercase(char* str);
char* get_datetime_days_later(int days);
void parse_config_line(char* line);
void load_config_file(const char* filename);
void generate_unique_id(char *id_str);
