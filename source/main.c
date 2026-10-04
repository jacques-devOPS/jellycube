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

#ifndef JC_VERSION
#define JC_VERSION "unknown"
#endif

static jellyfin_config_t config;
static jellyfin_client_t client;
static char parents[16][128];
static int offsets[16], selections[16];

static const char *volumes[3];
static int volume_count;
static char config_path[300];
static const char *config_volume;

static u32 buttons(void) { VIDEO_WaitVSync(); PAD_ScanPads(); return PAD_ButtonsDown(0); }
static void wait_start(void) { printf("\nSTART: exit\n"); while (!(buttons() & PAD_BUTTON_START)) {} }
static int load_level(int depth) {
    return depth ? jellyfin_get_items(&client, parents[depth], offsets[depth])
                 : jellyfin_get_libraries(&client);
}

static void mount_volumes(void) {
    if (fatMountSimple("carda", &__io_gcsda)) volumes[volume_count++] = "carda:";
    if (fatMountSimple("cardb", &__io_gcsdb)) volumes[volume_count++] = "cardb:";
    if (fatMountSimple("sd2", &__io_gcsd2))   volumes[volume_count++] = "sd2:";
}

static int try_path(const char *path, const char *volume) {
    if (strlen(path) >= sizeof(config_path)) return 0;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    fclose(f);
    strcpy(config_path, path);
    config_volume = volume;
    return 1;
}

static const char *volume_for(const char *path) {
    for (int i = 0; i < volume_count; i++)
        if (!strncmp(path, volumes[i], strlen(volumes[i]))) return volumes[i];
    return volume_count ? volumes[0] : NULL;
}

/*
 * Locate config.ini in the directory of the launched DOL.
 * Loaders such as Swiss pass the DOL path in argv[0]. The loader's device
 * prefix can differ from JellyCube's libfat mount names, so the directory
 * part is retried on every mounted volume.
 * Fallback: <volume>/apps/jellycube/config.ini.
 */
static int find_config(int argc, char **argv) {
    char dir[200], path[300];
    if (argc > 0 && argv && argv[0] && argv[0][0] && strlen(argv[0]) < sizeof(dir)) {
        printf("DOL: %s\n", argv[0]);
        strcpy(dir, argv[0]);
        char *slash = strrchr(dir, '/');
        if (slash) {
            *slash = 0;
            snprintf(path, sizeof(path), "%s/config.ini", dir);
            if (try_path(path, volume_for(path))) return 1;
            const char *rel = strchr(dir, ':');
            rel = rel ? rel + 1 : dir;
            for (int i = 0; i < volume_count; i++) {
                snprintf(path, sizeof(path), "%s%s%s/config.ini", volumes[i],
                         (rel[0] == '/' || !rel[0]) ? "" : "/", rel);
                if (try_path(path, volumes[i])) return 1;
            }
        }
    } else {
        printf("DOL: path not provided by loader\n");
    }
    for (int i = 0; i < volume_count; i++) {
        snprintf(path, sizeof(path), "%s/apps/jellycube/config.ini", volumes[i]);
        if (try_path(path, volumes[i])) return 1;
    }
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

    mount_volumes();
    if (!volume_count) printf("No FAT SD card mounted.\n");

    if (find_config(argc, argv)) {
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
    const char *volume = config_volume ? config_volume : (volume_count ? volumes[0] : NULL);

    printf("Server: %s:%d (HTTP)\n", config.server_address, config.server_port);
    if (network_init() < 0) {
        printf("DHCP failed. Check Broadband Adapter.\n");
        wait_start();
        return 1;
    }
    client.config = &config;
    if (jellyfin_authenticate(&client) < 0) {
        printf("Authentication failed.\n");
        wait_start();
        network_deinit();
        return 1;
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

    char path[128];
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
                snprintf(path, sizeof(path), "%s/mplayer/jellycube.m3u", volume);
                if (jellyfin_export_playlist(&client, item->id, path) == 0) {
                    printf("\nSaved %s\n", path);
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
