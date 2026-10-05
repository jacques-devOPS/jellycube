#include <stdio.h>
#include <string.h>
#include "token_cache.h"

/* Accept only characters Jellyfin uses in tokens and user IDs.
 * Anything else means a tampered or corrupt cache file. */
static int safe_line(const char *s) {
    if (!*s) return 0;
    for (; *s; s++) {
        if (!((*s >= '0' && *s <= '9') || (*s >= 'a' && *s <= 'z') ||
              (*s >= 'A' && *s <= 'Z') || *s == '-'))
            return 0;
    }
    return 1;
}

static void strip(char *s) {
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = 0;
}

int token_cache_path(const char *config_path, char *out, size_t size) {
    const char *slash = strrchr(config_path, '/');
    size_t dir = slash ? (size_t)(slash - config_path + 1) : 0;
    if (dir + sizeof("token.cache") > size) return -1;
    memcpy(out, config_path, dir);
    strcpy(out + dir, "token.cache");
    return 0;
}

int token_cache_load(const char *path, char *token, size_t tsize,
                     char *user_id, size_t usize) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    token[0] = user_id[0] = 0;
    if (!fgets(token, tsize, f) || !fgets(user_id, usize, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    strip(token);
    strip(user_id);
    if (!safe_line(token) || !safe_line(user_id)) {
        token[0] = user_id[0] = 0;
        return -1;
    }
    return 0;
}

int token_cache_save(const char *path, const char *token, const char *user_id) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    int r = fprintf(f, "%s\n%s\n", token, user_id) < 0 ? -1 : 0;
    fclose(f);
    return r;
}

void token_cache_clear(const char *path) {
    remove(path);
}
