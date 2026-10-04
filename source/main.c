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

#ifndef JC_VERSION
#define JC_VERSION "unknown"
#endif

static jellyfin_config_t config;
static jellyfin_client_t client;
static char parents[16][128];
static int offsets[16], selections[16];

static const char *volume;          /* Mounted SD volume, e.g. "carda:" */
static char config_path[300];

static u32 buttons(void) { VIDEO_WaitVSync(); PAD_ScanPads(); return PAD_ButtonsDown(0); }
static void wait_start(void) { printf("\nSTART: exit\n"); while (!(buttons() & PAD_BUTTON_START)) {} }
static int load_level(int depth) {
    return depth ? jellyfin_get_items(&client, parents[depth], offsets[depth])
                 : jellyfin_get_libraries(&client);
}

/* Mount the first SD device found. Same order and behaviour as 0.2.1. */
static void mount_volume(void) {
    if (fatMountSimple("carda", &__io_gcsda)) volume = "carda:";
    else if (fatMountSimple("cardb", &__io_gcsdb)) volume = "cardb:";
    else if (fatMountSimple("sd2", &__io_gcsd2)) volume = "sd2:";
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

    mount_volume();
    if (volume) printf("SD: %s mounted\n", volume);
    else printf("SD: no FAT card mounted\n");

    printf("Config search:\n");
    if (volume && config_locate(volume, argc, argv, config_path, sizeof(config_path))) {
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
