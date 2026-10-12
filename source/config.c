#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include "config.h"

static char *trim(char *s) {
    while (isspace((unsigned char)*s)) s++;
    char *e = s + strlen(s);
    while (e > s && isspace((unsigned char)e[-1])) *--e = 0;
    return s;
}

static void report(int line, const char *why) {
    printf("Config line %d: %s\n", line, why);
}

static int parse_int_key(int line, const char *v, int min, int max, int *out) {
    char *end;
    long x = strtol(v, &end, 10);
    if (!*v || *end || x < min || x > max) {
        report(line, "invalid numeric value");
        return -1;
    }
    *out = (int)x;
    return 0;
}

static int safe_codec(const char *s) {
    if (!s || !*s) return 0;
    for (; *s; ++s) {
        unsigned char c = (unsigned char)*s;
        if (!isalnum(c) && c != '_' && c != '-') return 0;
    }
    return 1;
}

static void profile_set_defaults(jellyfin_profile_config_t *p, int index) {
    memset(p, 0, sizeof(*p));
    snprintf(p->name, sizeof(p->name), "Profile%d", index + 1);
    strcpy(p->video_codec, "mpeg2video");
    strcpy(p->audio_codec, "mp2");
    p->max_width = 320;
    p->max_height = 240;
    p->max_framerate = 25;
    p->video_bitrate = 1000000;
    p->audio_bitrate = 128000;
    p->audio_channels = 2;
    p->audio_sample_rate = 48000;
}

void config_set_defaults(jellyfin_config_t *c) {
    memset(c, 0, sizeof(*c));
    c->server_port = 8096;
    strcpy(c->device_id, "jellycube_01");
    strcpy(c->client_name, "JellyCube");
    strcpy(c->client_version, "0.2");

    strcpy(c->video_codec, "mpeg2video");
    strcpy(c->audio_codec, "mp2");
    c->max_width = 320;
    c->max_height = 240;
    c->max_framerate = 25;
    c->video_bitrate = 1000000;
    c->audio_bitrate = 128000;
    c->audio_channels = 2;
    c->audio_sample_rate = 48000;

    c->profile_count = 0;
    for (int i = 0; i < JC_MAX_PROFILES; i++) {
        profile_set_defaults(&c->profiles[i], i);
    }
}

static void profile_inherit_global(jellyfin_profile_config_t *p, const jellyfin_config_t *c) {
    strcpy(p->video_codec, c->video_codec);
    strcpy(p->audio_codec, c->audio_codec);
    p->max_width = c->max_width;
    p->max_height = c->max_height;
    p->max_framerate = c->max_framerate;
    p->video_bitrate = c->video_bitrate;
    p->audio_bitrate = c->audio_bitrate;
    p->audio_channels = c->audio_channels;
    p->audio_sample_rate = c->audio_sample_rate;
}

static int parse_profile_key(jellyfin_config_t *c, int line, const char *key, const char *value) {
    int idx;
    char field[64];
    if (strncasecmp(key, "profile", 7) != 0) return 0;
    if (sscanf(key, "profile%d_%63s", &idx, field) != 2) return 0;
    if (idx < 1 || idx > JC_MAX_PROFILES) {
        report(line, "profile index out of range");
        return -1;
    }

    jellyfin_profile_config_t *p = &c->profiles[idx - 1];

    if (!strcasecmp(field, "name")) {
        if (strlen(value) >= sizeof(p->name)) { report(line, "value too long"); return -1; }
        strcpy(p->name, value);
        return 1;
    }
    if (!strcasecmp(field, "video_codec")) {
        if (strlen(value) >= sizeof(p->video_codec)) { report(line, "value too long"); return -1; }
        strcpy(p->video_codec, value);
        return 1;
    }
    if (!strcasecmp(field, "audio_codec")) {
        if (strlen(value) >= sizeof(p->audio_codec)) { report(line, "value too long"); return -1; }
        strcpy(p->audio_codec, value);
        return 1;
    }
    if (!strcasecmp(field, "max_width")) return parse_int_key(line, value, 160, 1280, &p->max_width) == 0 ? 1 : -1;
    if (!strcasecmp(field, "max_height")) return parse_int_key(line, value, 120, 960, &p->max_height) == 0 ? 1 : -1;
    if (!strcasecmp(field, "max_framerate")) return parse_int_key(line, value, 10, 60, &p->max_framerate) == 0 ? 1 : -1;
    if (!strcasecmp(field, "video_bitrate")) return parse_int_key(line, value, 100000, 10000000, &p->video_bitrate) == 0 ? 1 : -1;
    if (!strcasecmp(field, "audio_bitrate")) return parse_int_key(line, value, 32000, 512000, &p->audio_bitrate) == 0 ? 1 : -1;
    if (!strcasecmp(field, "audio_channels")) return parse_int_key(line, value, 1, 8, &p->audio_channels) == 0 ? 1 : -1;
    if (!strcasecmp(field, "audio_sample_rate")) return parse_int_key(line, value, 8000, 192000, &p->audio_sample_rate) == 0 ? 1 : -1;

    printf("Config line %d: ignoring unknown key '%s'\n", line, key);
    return 1;
}

/*
 * Accepts: CRLF line endings, UTF-8 BOM, case-insensitive keys,
 * optional quotes around values, '#' or ';' comments.
 * Unknown keys are reported and ignored.
 */
int config_load(const char *path, jellyfin_config_t *c) {
    char line[1024];
    int n = 0, result = -1;
    config_set_defaults(c);
    FILE *f = fopen(path, "r");
    if (!f) { printf("Config: cannot open %s\n", path); return -1; }

    while (fgets(line, sizeof(line), f)) {
        n++;
        if (!strchr(line, '\n') && !feof(f)) { report(n, "line too long"); goto done; }
        char *k = line;
        if (n == 1 && (unsigned char)k[0] == 0xEF && (unsigned char)k[1] == 0xBB &&
            (unsigned char)k[2] == 0xBF) k += 3;
        k = trim(k);
        if (!*k || *k == '#' || *k == ';') continue;
        char *v = strchr(k, '=');
        if (!v) { report(n, "missing '='"); goto done; }
        *v++ = 0;
        k = trim(k);
        v = trim(v);
        size_t vl = strlen(v);
        if (vl >= 2 && ((v[0] == '"' && v[vl - 1] == '"') || (v[0] == '\'' && v[vl - 1] == '\''))) {
            v[vl - 1] = 0;
            v++;
        }

        {
            int p = parse_profile_key(c, n, k, v);
            if (p < 0) goto done;
            if (p > 0) continue;
        }

        char *dest = NULL;
        size_t cap = 0;
        if (!strcasecmp(k, "server"))                { dest = c->server_address; cap = sizeof(c->server_address); }
        else if (!strcasecmp(k, "username"))         { dest = c->username;       cap = sizeof(c->username); }
        else if (!strcasecmp(k, "password"))         { dest = c->password;       cap = sizeof(c->password); }
        else if (!strcasecmp(k, "device_id"))        { dest = c->device_id;      cap = sizeof(c->device_id); }
        else if (!strcasecmp(k, "client_name"))      { dest = c->client_name;    cap = sizeof(c->client_name); }
        else if (!strcasecmp(k, "video_codec"))      { dest = c->video_codec;    cap = sizeof(c->video_codec); }
        else if (!strcasecmp(k, "audio_codec"))      { dest = c->audio_codec;    cap = sizeof(c->audio_codec); }
        else if (!strcasecmp(k, "port")) {
            if (parse_int_key(n, v, 1, 65535, &c->server_port) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "profile_count")) {
            if (parse_int_key(n, v, 0, JC_MAX_PROFILES, &c->profile_count) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "max_width")) {
            if (parse_int_key(n, v, 160, 1280, &c->max_width) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "max_height")) {
            if (parse_int_key(n, v, 120, 960, &c->max_height) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "max_framerate")) {
            if (parse_int_key(n, v, 10, 60, &c->max_framerate) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "video_bitrate")) {
            if (parse_int_key(n, v, 100000, 10000000, &c->video_bitrate) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "audio_bitrate")) {
            if (parse_int_key(n, v, 32000, 512000, &c->audio_bitrate) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "audio_channels")) {
            if (parse_int_key(n, v, 1, 8, &c->audio_channels) < 0) goto done;
            continue;
        }
        else if (!strcasecmp(k, "audio_sample_rate")) {
            if (parse_int_key(n, v, 8000, 192000, &c->audio_sample_rate) < 0) goto done;
            continue;
        }
        else {
            printf("Config line %d: ignoring unknown key '%s'\n", n, k);
            continue;
        }

        if (strlen(v) >= cap) { report(n, "value too long"); goto done; }
        memcpy(dest, v, strlen(v) + 1);
    }

    if (ferror(f)) { report(n, "read error"); goto done; }
    if (!*c->server_address) { printf("Config: 'server' missing\n"); goto done; }
    if (!*c->username) { printf("Config: 'username' missing\n"); goto done; }
    if (strpbrk(c->device_id, "\"\\\r\n") || strpbrk(c->client_name, "\"\\\r\n")) {
        printf("Config: device_id/client_name contain quote or backslash\n");
        goto done;
    }
    if (!safe_codec(c->video_codec) || !safe_codec(c->audio_codec)) {
        printf("Config: codec values must be alnum, '-' or '_'\n");
        goto done;
    }

    for (int i = 0; i < JC_MAX_PROFILES; i++) {
        if (c->profiles[i].name[0] == 0) snprintf(c->profiles[i].name, sizeof(c->profiles[i].name), "Profile%d", i + 1);
        profile_inherit_global(&c->profiles[i], c);
    }

    /* Re-parse profile fields from loaded values to preserve explicit overrides.
       profile_inherit_global updated all profiles as base defaults. */
    fseek(f, 0, SEEK_SET);
    n = 0;
    while (fgets(line, sizeof(line), f)) {
        n++;
        char *k = line;
        if (n == 1 && (unsigned char)k[0] == 0xEF && (unsigned char)k[1] == 0xBB &&
            (unsigned char)k[2] == 0xBF) k += 3;
        k = trim(k);
        if (!*k || *k == '#' || *k == ';') continue;
        char *v = strchr(k, '=');
        if (!v) continue;
        *v++ = 0;
        k = trim(k);
        v = trim(v);
        size_t vl = strlen(v);
        if (vl >= 2 && ((v[0] == '"' && v[vl - 1] == '"') || (v[0] == '\'' && v[vl - 1] == '\''))) {
            v[vl - 1] = 0;
            v++;
        }
        if (parse_profile_key(c, n, k, v) < 0) goto done;
    }

    for (int i = 0; i < c->profile_count; i++) {
        if (!safe_codec(c->profiles[i].video_codec) || !safe_codec(c->profiles[i].audio_codec)) {
            printf("Config: profile%d codec values must be alnum, '-' or '_'\n", i + 1);
            goto done;
        }
    }

    unsigned a, b, d, e;
    char extra;
    if (strpbrk(c->server_address, "/ \t") ||
        sscanf(c->server_address, "%u.%u.%u.%u%c", &a, &b, &d, &e, &extra) != 4 ||
        a > 255 || b > 255 || d > 255 || e > 255 || a == 0 || a >= 224) {
        printf("Config: server must be a plain IPv4 address, got '%s'\n", c->server_address);
        goto done;
    }
    result = 0;
done:
    fclose(f);
    return result;
}
