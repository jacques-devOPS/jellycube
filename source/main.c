#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <gccore.h>
#include <fat.h>
#include <sdcard/gcsd.h>
#include "config.h"
#include "jc_network.h"
#include "dolphin_test.h"
#include "jellyfin.h"
#include "config_locate.h"
#include "token_cache.h"
#include "storage_order.h"
#include <errno.h>

#ifndef JC_VERSION
#define JC_VERSION "unknown"
#endif

static jellyfin_config_t config;
static jellyfin_client_t client;
static char parents[16][128];
static int offsets[16], selections[16];

static const char *volume;          /* Mounted SD volume, e.g. "carda:" */
static char config_path[300];

static void build_playlist_label(const jellyfin_item_t *item, char *label, size_t label_size) {
    int n;
    if (!item || !label || label_size < 2) return;
    label[0] = '\0';
    if (!strcmp(item->type, "Episode") && item->series_name[0]) {
        if (item->parent_index_number > 0 && item->index_number > 0) {
            n = snprintf(label, label_size, "%s S%02dE%02d %s",
                         item->series_name, item->parent_index_number, item->index_number, item->name);
            if (n > 0 && (size_t)n < label_size) return;
        }
        if (item->index_number > 0) {
            n = snprintf(label, label_size, "%s E%02d %s",
                         item->series_name, item->index_number, item->name);
            if (n > 0 && (size_t)n < label_size) return;
        }
        n = snprintf(label, label_size, "%s %s", item->series_name, item->name);
        if (n > 0 && (size_t)n < label_size) return;
    }
    if (!strcmp(item->type, "Movie") && item->production_year > 0) {
        n = snprintf(label, label_size, "%s (%d)", item->name, item->production_year);
        if (n > 0 && (size_t)n < label_size) return;
    }
    snprintf(label, label_size, "%s", item->name);
}

static void sanitize_playlist_name(const char *title, char *out, size_t out_size) {
    size_t w = 0;
    if (!title || !out || out_size < 2) return;
    for (size_t i = 0; title[i] && w + 1 < out_size; ++i) {
        unsigned char c = (unsigned char)title[i];
        char ch;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
            c == '-' || c == '_' || c == '.' || c == '(' || c == ')') {
            ch = (char)c;
        } else if (c == ' ') {
            ch = '_';
        } else {
            ch = '_';
        }
        if (w > 0 && out[w - 1] == '_' && ch == '_') continue;
        out[w++] = ch;
    }
    while (w > 0 && (out[w - 1] == '_' || out[w - 1] == '.')) w--;
    if (w == 0) {
        const char *fallback = "jellycube";
        size_t i = 0;
        while (fallback[i] && i + 1 < out_size) {
            out[i] = fallback[i];
            i++;
        }
        out[i] = '\0';
        return;
    }
    out[w] = '\0';
}

static u32 buttons(void) { VIDEO_WaitVSync(); PAD_ScanPads(); return PAD_ButtonsDown(0); }
static void wait_start(void) { printf("\nSTART: exit\n"); while (!(buttons() & PAD_BUTTON_START)) {} }

static void clamp_label(const char *src, char *dst, size_t dst_size) {
    const size_t max_visible = 38;
    size_t n = 0;
    if (!dst || dst_size == 0) return;
    if (!src) src = "";
    while (src[n] && n + 1 < dst_size && n < max_visible) {
        dst[n] = src[n];
        n++;
    }
    if (src[n] && n >= 3) {
        dst[n - 3] = '.';
        dst[n - 2] = '.';
        dst[n - 1] = '.';
    }
    dst[n] = '\0';
}

static int prompt_choice(const char *title, const char *subtitle,
                         const char **options, int count, int selected) {
    char line[96];
    char line2[96];
    if (!options || count <= 0) return -1;
    if (selected < 0 || selected >= count) selected = 0;
    while (1) {
        printf("\x1b[2J\x1b[H\n\n\n\n\n\n");

        clamp_label(title ? title : "Choose", line, sizeof(line));
        printf("%s\n", line);

        if (subtitle && subtitle[0]) {
            clamp_label(subtitle, line2, sizeof(line2));
            printf("%s\n", line2);
        }

        clamp_label(options[selected], line, sizeof(line));
        printf("< %d/%d > %s\n", selected + 1, count, line);
        printf("\nUP/DOWN or L/R: select\nA: confirm  B/START: cancel\n");

        u32 b = buttons();
        if ((b & PAD_BUTTON_UP) || (b & PAD_TRIGGER_L)) {
            selected = selected > 0 ? selected - 1 : count - 1;
        } else if ((b & PAD_BUTTON_DOWN) || (b & PAD_TRIGGER_R)) {
            selected = selected + 1 < count ? selected + 1 : 0;
        } else if (b & PAD_BUTTON_A) {
            return selected;
        } else if ((b & PAD_BUTTON_B) || (b & PAD_BUTTON_START)) {
            return -1;
        }
    }
}

static int prompt_stream_profile(const jellyfin_item_t *item, jellyfin_stream_profile_t *profile) {
    jellyfin_stream_options_t options;
    char subtitle[320];

    if (!profile) return -1;
    jellyfin_stream_profile_defaults(profile);

    if (config.profile_count > 0) {
        const char *ptrs[JC_MAX_PROFILES];
        int pick;
        for (int i = 0; i < config.profile_count; i++) {
            ptrs[i] = config.profiles[i].name[0] ? config.profiles[i].name : "Profile";
        }
        pick = prompt_choice("Transcode profile", item ? item->name : "", ptrs, config.profile_count, 0);
        if (pick < 0) return -1;
        profile->video_codec = config.profiles[pick].video_codec;
        profile->audio_codec = config.profiles[pick].audio_codec;
        profile->max_width = config.profiles[pick].max_width;
        profile->max_height = config.profiles[pick].max_height;
        profile->max_framerate = config.profiles[pick].max_framerate;
        profile->video_bitrate = config.profiles[pick].video_bitrate;
        profile->audio_bitrate = config.profiles[pick].audio_bitrate;
        profile->audio_channels = config.profiles[pick].audio_channels;
        profile->audio_sample_rate = config.profiles[pick].audio_sample_rate;
    } else {
        profile->video_codec = config.video_codec;
        profile->audio_codec = config.audio_codec;
        profile->max_width = config.max_width;
        profile->max_height = config.max_height;
        profile->max_framerate = config.max_framerate;
        profile->video_bitrate = config.video_bitrate;
        profile->audio_bitrate = config.audio_bitrate;
        profile->audio_channels = config.audio_channels;
        profile->audio_sample_rate = config.audio_sample_rate;
    }

    snprintf(subtitle, sizeof(subtitle), "Item: %.240s", item ? item->name : "");

    if (item && jellyfin_get_stream_options(&client, item->id, &options) == 0) {
        if (options.audio_count > 0) {
            char labels[JC_MAX_STREAMS][128];
            const char *ptrs[JC_MAX_STREAMS];
            int def = 0;
            for (int i = 0; i < options.audio_count; i++) {
                const char *lang = options.audio[i].language[0] ? options.audio[i].language : "und";
                const char *name = options.audio[i].title[0] ? options.audio[i].title : "audio";
                snprintf(labels[i], sizeof(labels[i]), "idx %d | %s | %s%s",
                         options.audio[i].index, lang, name,
                         options.audio[i].is_default ? " [default]" : "");
                ptrs[i] = labels[i];
                if (options.audio[i].is_default) def = i;
            }
            int pick = prompt_choice("Audio stream", subtitle, ptrs, options.audio_count, def);
            if (pick < 0) return -1;
            profile->audio_stream_index = options.audio[pick].index;
        }

        {
            char labels[JC_MAX_STREAMS + 1][128];
            const char *ptrs[JC_MAX_STREAMS + 1];
            int count = 1;
            int def = 0;
            snprintf(labels[0], sizeof(labels[0]), "off");
            ptrs[0] = labels[0];
            for (int i = 0; i < options.subtitle_count; i++) {
                const char *lang = options.subtitle[i].language[0] ? options.subtitle[i].language : "und";
                const char *name = options.subtitle[i].title[0] ? options.subtitle[i].title : "subtitle";
                snprintf(labels[count], sizeof(labels[count]), "idx %d | %s | %s%s",
                         options.subtitle[i].index, lang, name,
                         options.subtitle[i].is_default ? " [default]" : "");
                ptrs[count] = labels[count];
                if (options.subtitle[i].is_default) def = count;
                count++;
                if (count >= JC_MAX_STREAMS + 1) break;
            }
            int pick = prompt_choice("Subtitle stream", subtitle, ptrs, count, def);
            if (pick < 0) return -1;
            profile->subtitle_stream_index = pick == 0 ? -1 : options.subtitle[pick - 1].index;
        }
    }

    return 0;
}

static int load_level(int depth) {
    return depth ? jellyfin_get_items(&client, parents[depth], offsets[depth])
                 : jellyfin_get_libraries(&client);
}

/* Use the loader's physical device, not whichever card mounts first.
 * Without a recognized launch prefix, try each card for an actual config.
 * Never silently substitute another card when a known launch device fails. */
static int locate_storage_config(int argc, char **argv) {
    const char *names[] = {"carda", "cardb", "sd2"};
    const char *volumes[] = {"carda:", "cardb:", "sd2:"};
    const DISC_INTERFACE *devices[] = {&__io_gcsda, &__io_gcsdb, &__io_gcsd2};
    const char *launch = argc > 0 && argv ? argv[0] : NULL;
    int preferred = storage_launch_index(launch);
    printf("DOL: %s\n", launch ? launch : "not supplied");
    printf("Storage: %s\n", preferred >= 0 ? volumes[preferred] : "launch device unknown; search A/B/SD2");
    for (int attempt = 0; attempt < (preferred >= 0 ? 1 : 3); ++attempt) {
        int i = preferred >= 0 ? preferred : attempt;
        int up = devices[i]->startup();
        int inserted = up ? devices[i]->isInserted() : -1;
        int mounted = -1, mount_errno = 0;
        if (inserted == 1) {
            errno = 0;
            mounted = fatMountSimple(names[i], devices[i]);
            mount_errno = errno;
        }
        printf("%s startup=%d inserted=%d mounted=%d errno=%d\n",
               names[i], up, inserted, mounted, mount_errno);
        if (mounted == 1 && config_locate(volumes[i], argc, argv,
                                         config_path, sizeof(config_path))) {
            volume = volumes[i];
            return 1;
        }
        if (mounted == 1) fatUnmount(volumes[i]);
    }
    config_path[0] = 0;
    if (preferred >= 0) printf("Launch card/config unavailable. No different-card fallback.\n");
    return 0;
}

int main(int argc, char **argv) {
    VIDEO_Init();
    PAD_Init();
    GXRModeObj *mode = VIDEO_GetPreferredMode(NULL);
    void *xfb = MEM_K0_TO_K1(SYS_AllocateFramebuffer(mode));
    console_init(xfb, 20, 20, mode->fbWidth, mode->xfbHeight, mode->fbWidth * VI_DISPLAY_PIX_SZ);
    VIDEO_Configure(mode);
    VIDEO_SetNextFramebuffer(xfb);
    VIDEO_SetBlack(FALSE);
    VIDEO_Flush();
    VIDEO_WaitVSync();
    if (mode->viTVMode & VI_NON_INTERLACE) VIDEO_WaitVSync();

    printf("\x1b[2J\x1b[HJellyCube %s\n", JC_VERSION);

    printf("Storage diagnostics (-1 = stage skipped):\n");
    int found_config = locate_storage_config(argc, argv);
    printf("Press A to continue.\n");
    while (!(buttons() & PAD_BUTTON_A)) {}
    if (found_config) {
        if (config_load(config_path, &config) < 0) {
            printf("Invalid config: %s\n", config_path);
            wait_start();
            return 1;
        }
        printf("Config: %s\n", config_path);
    } else {
#ifdef JC_DOLPHIN_TEST
        printf("No config.ini found. Dolphin test mode: embedded config.\n");
        config = dolphin_test_config;
#else
        printf("No config.ini found.\nPlace config.ini next to the DOL file.\n");
        wait_start();
        return 1;
#endif
    }

    printf("Server: %s:%d (HTTP)\n", config.server_address, config.server_port);
    if (network_init() < 0) {
        printf("DHCP failed. Check Broadband Adapter.\n");
        wait_start();
        return 1;
    }
    client.config = &config;
    {
        char token_path[300];
        int cached = 0, fresh = 0;
        token_path[0] = 0;
        if (config_path[0] && token_cache_path(config_path, token_path, sizeof(token_path)) == 0)
            cached = token_cache_load(token_path, client.access_token, sizeof(client.access_token),
                                      client.user_id, sizeof(client.user_id)) == 0;
        if (cached && jellyfin_validate_token(&client) == 0) {
            printf("Session: cached token valid.\n");
        } else {
            if (cached) printf("Session: cached token rejected; re-authenticating.\n");
            if (jellyfin_authenticate(&client) < 0) {
                printf("Authentication failed.\n");
                wait_start();
                network_deinit();
                return 1;
            }
            fresh = 1;
        }
        if (fresh && token_path[0]) {
            if (token_cache_save(token_path, client.access_token, client.user_id) == 0)
                printf("Session: token cached. Exported playlists stay valid.\n");
            else
                printf("Session: token cache write failed (playlists valid this session only).\n");
        }
    }
    /* Password no longer required after authentication. */
    memset(config.password, 0, sizeof(config.password));

    int depth = 0, selected = 0;
    if (load_level(depth) < 0) {
        printf("Library request failed.\n");
        wait_start();
        network_deinit();
        return 1;
    }

    char path[300];
    char playlist_label[320];
    char playlist_name[96];
    jellyfin_stream_profile_t profile;
    int redraw = 1;
    while (1) {
        if (redraw) {
            printf("\x1b[2J\x1b[HJellyCube | %s | page %d\n", depth ? "Items" : "Libraries",
                   offsets[depth] / JC_PAGE_SIZE + 1);
            for (int i = 0; i < client.count; i++)
                printf("%c %2d %-42.42s %s\n", i == selected ? '>' : ' ', i + 1,
                       client.items[i].name, client.items[i].folder ? "[DIR]" : "");
            printf("\nUP/DOWN: select  A: open/export  B: parent\nL/R: page  START: exit\n");
            redraw = 0;
        }
        u32 b = buttons();
        if (b & PAD_BUTTON_START) break;
        if ((b & PAD_BUTTON_UP) && selected > 0) { selected--; redraw = 1; }
        if ((b & PAD_BUTTON_DOWN) && selected + 1 < client.count) { selected++; redraw = 1; }

        int old_depth = depth, old_offset = offsets[depth], old_selected = selected, reload = 0;
        if ((b & PAD_BUTTON_B) && depth > 0) {
            depth--; selected = selections[depth]; reload = 1;
        } else if ((b & PAD_TRIGGER_R) && depth > 0 && client.count == JC_PAGE_SIZE) {
            offsets[depth] += JC_PAGE_SIZE; selected = 0; reload = 1;
        } else if ((b & PAD_TRIGGER_L) && depth > 0 && offsets[depth] >= JC_PAGE_SIZE) {
            offsets[depth] -= JC_PAGE_SIZE; selected = 0; reload = 1;
        } else if ((b & PAD_BUTTON_A) && client.count) {
            jellyfin_item_t *item = &client.items[selected];
            if (item->folder && depth < 15) {
                selections[depth] = selected; depth++;
                strcpy(parents[depth], item->id);
                offsets[depth] = 0; selected = 0; reload = 1;
            } else if (!item->folder && (!strcmp(item->type, "Movie") ||
                       !strcmp(item->type, "Episode") || !strcmp(item->type, "Video"))) {
                if (!volume) { printf("\nExport disabled: no SD card.\n"); continue; }
                if (prompt_stream_profile(item, &profile) < 0) {
                    printf("\nExport cancelled.\n");
                    redraw = 1;
                    continue;
                }
                build_playlist_label(item, playlist_label, sizeof(playlist_label));
                sanitize_playlist_name(playlist_label, playlist_name, sizeof(playlist_name));
                snprintf(path, sizeof(path), "%s/mplayer/%s.m3u", volume, playlist_name);
                if (jellyfin_export_playlist(&client, item->id, &profile, path) == 0) {
                    printf("\nSaved %s\n", path);
                    printf("Profile: v=%s a=%s %dx%d %dfps vb=%d ab=%d\n",
                           profile.video_codec, profile.audio_codec,
                           profile.max_width, profile.max_height, profile.max_framerate,
                           profile.video_bitrate, profile.audio_bitrate);
                    printf("Playlist contains a session token. Delete after testing.\n");
                } else {
                    printf("\nExport failed. Create /mplayer on the SD card.\n");
                }
            } else {
                printf("\nUnsupported item or directory depth.\n");
            }
        }
        if (reload) {
            if (load_level(depth) < 0 || !client.count) {
                printf("\nRequest failed or page empty. Restoring previous page.\n");
                depth = old_depth; offsets[depth] = old_offset; selected = old_selected;
                if (load_level(depth) < 0) { printf("Network failed.\n"); wait_start(); break; }
            }
            if (selected >= client.count) selected = 0;
            redraw = 1;
        }
    }
    memset(client.access_token, 0, sizeof(client.access_token));
    network_deinit();
    return 0;
}
