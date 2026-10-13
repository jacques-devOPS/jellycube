#ifndef JC_JELLYFIN_H
#define JC_JELLYFIN_H
#include <stddef.h>
#include "platform.h"
#include "config.h"

#define JC_PAGE_SIZE 12
#define JSON_BUFFER_SIZE 32768
#define JC_MAX_STREAMS 16

typedef struct {
    char id[128],name[256],type[32],series_name[256];
    int folder;
    int parent_index_number;
    int index_number;
    int production_year;
} jellyfin_item_t;

typedef struct {
    int index;
    int is_default;
    char language[16];
    char title[96];
} jellyfin_stream_t;

typedef struct {
    jellyfin_stream_t audio[JC_MAX_STREAMS];
    jellyfin_stream_t subtitle[JC_MAX_STREAMS];
    int audio_count;
    int subtitle_count;
} jellyfin_stream_options_t;

typedef struct {
    int audio_stream_index;      /* -1: default */
    int subtitle_stream_index;   /* -1: none */
    const char *video_codec;     /* e.g. mpeg2video, h264 */
    const char *audio_codec;     /* e.g. mp2, aac */
    int max_width;
    int max_height;
    int max_framerate;
    int video_bitrate;
    int audio_bitrate;
    int audio_channels;
    int audio_sample_rate;
} jellyfin_stream_profile_t;

typedef struct {
    jellyfin_config_t *config;
    char access_token[256],user_id[128];
    jellyfin_item_t items[JC_PAGE_SIZE];int count;
} jellyfin_client_t;

s32 jellyfin_authenticate(jellyfin_client_t *client);
s32 jellyfin_validate_token(jellyfin_client_t *client);
s32 jellyfin_get_libraries(jellyfin_client_t *client);
s32 jellyfin_get_items(jellyfin_client_t *client,const char *parent,int start);
s32 jellyfin_get_stream_options(jellyfin_client_t *client,const char *item,jellyfin_stream_options_t *options);
void jellyfin_stream_profile_defaults(jellyfin_stream_profile_t *profile);
int jellyfin_get_stream_url(jellyfin_client_t *client,const char *item,const jellyfin_stream_profile_t *profile,char *url,size_t size);
int jellyfin_export_playlist(jellyfin_client_t *client,const char *item,const jellyfin_stream_profile_t *profile,const char *path);
#endif
