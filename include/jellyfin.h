#ifndef JC_JELLYFIN_H
#define JC_JELLYFIN_H
#include <stddef.h>
#include "platform.h"
#include "config.h"
#define JC_PAGE_SIZE 12
#define JSON_BUFFER_SIZE 32768
typedef struct { char id[128],name[256],type[32];int folder; } jellyfin_item_t;
typedef struct {
    jellyfin_config_t *config;
    char access_token[256],user_id[128];
    jellyfin_item_t items[JC_PAGE_SIZE];int count;
} jellyfin_client_t;
s32 jellyfin_authenticate(jellyfin_client_t *client);
s32 jellyfin_validate_token(jellyfin_client_t *client);
s32 jellyfin_get_libraries(jellyfin_client_t *client);
s32 jellyfin_get_items(jellyfin_client_t *client,const char *parent,int start);
int jellyfin_get_stream_url(jellyfin_client_t *client,const char *item,char *url,size_t size);
int jellyfin_export_playlist(jellyfin_client_t *client,const char *item,const char *path);
#endif
