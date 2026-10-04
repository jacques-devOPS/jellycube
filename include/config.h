#ifndef JC_CONFIG_H
#define JC_CONFIG_H
#define MAX_STRING_LEN 256
typedef struct {
    char server_address[MAX_STRING_LEN];
    int server_port;
    char username[MAX_STRING_LEN],password[MAX_STRING_LEN];
    char device_id[64],client_name[64],client_version[16];
} jellyfin_config_t;
int config_load(const char *path,jellyfin_config_t *config);
void config_set_defaults(jellyfin_config_t *config);
#endif
