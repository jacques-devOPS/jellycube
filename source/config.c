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

void config_set_defaults(jellyfin_config_t *c) {
    memset(c, 0, sizeof(*c));
    c->server_port = 8096;
    strcpy(c->device_id, "jellycube_01");
    strcpy(c->client_name, "JellyCube");
    strcpy(c->client_version, "0.2");
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

        char *dest = NULL;
        size_t cap = 0;
        if (!strcasecmp(k, "server"))           { dest = c->server_address; cap = sizeof(c->server_address); }
        else if (!strcasecmp(k, "username"))    { dest = c->username;       cap = sizeof(c->username); }
        else if (!strcasecmp(k, "password"))    { dest = c->password;       cap = sizeof(c->password); }
        else if (!strcasecmp(k, "device_id"))   { dest = c->device_id;      cap = sizeof(c->device_id); }
        else if (!strcasecmp(k, "client_name")) { dest = c->client_name;    cap = sizeof(c->client_name); }
        else if (!strcasecmp(k, "port")) {
            char *end;
            long p = strtol(v, &end, 10);
            if (!*v || *end || p < 1 || p > 65535) { report(n, "invalid port"); goto done; }
            c->server_port = (int)p;
            continue;
        } else {
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
