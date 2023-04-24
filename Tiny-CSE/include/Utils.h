/*
 * Created on Mon Mar 27 2023
 *
 * Author(s): Rafael Pereira (Rafael_Pereira_2000@hotmail.com)
 *            Carla Mendes (carlasofiamendes@outlook.com) 
 *            Ana Cruz (anacassia.10@hotmail.com) 
 * Copyright (c) 2023 IPLeiria
 */

#define MAX_CONFIG_LINE_LENGTH 64  

char* getCurrentTime();
char* getCurrentTimeLong();
void to_lowercase(char* str);
char* get_datetime_days_later(int days);
void parse_config_line(char* line);
void load_config_file(const char* filename);
void generate_unique_id(char *id_str);
char is_number(const char *str);
int key_in_array(const char *key, const char **key_array, size_t key_array_len);