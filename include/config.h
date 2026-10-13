#ifndef JC_CONFIG_H
#define JC_CONFIG_H

#define MAX_STRING_LEN 256
#define JC_MAX_PROFILES 6

typedef struct {
    char name[32];
    char video_codec[32];
    char audio_codec[32];
    int max_width;
    int max_height;
    int max_framerate;
    int video_bitrate;
    int audio_bitrate;
    int audio_channels;
    int audio_sample_rate;
} jellyfin_profile_config_t;

typedef struct {
    char server_address[MAX_STRING_LEN];
    int server_port;
    char username[MAX_STRING_LEN],password[MAX_STRING_LEN];
    char device_id[64],client_name[64],client_version[16];

    /* Global transcode defaults for playlist export. */
    char video_codec[32];
    char audio_codec[32];
    int max_width;
    int max_height;
    int max_framerate;
    int video_bitrate;
    int audio_bitrate;
    int audio_channels;
    int audio_sample_rate;

    /* Optional named profiles selectable per export. */
    int profile_count;
    jellyfin_profile_config_t profiles[JC_MAX_PROFILES];
} jellyfin_config_t;

int config_load(const char *path,jellyfin_config_t *config);
void config_set_defaults(jellyfin_config_t *config);

#endif
