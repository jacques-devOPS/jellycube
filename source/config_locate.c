#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include "config_locate.h"

/* Append one component to dir, using the on-card spelling of a case-insensitive match. */
static int step_ci(char *dir, size_t size, const char *name) {
    DIR *d = opendir(dir);
    if (!d) return 0;
    struct dirent *e;
    int found = 0;
    while ((e = readdir(d)) != NULL) {
        if (strcasecmp(e->d_name, name)) continue;
        size_t len = strlen(dir);
        const char *sep = (len && dir[len - 1] == '/') ? "" : "/";
        int n = snprintf(dir + len, size - len, "%s%s", sep, e->d_name);
        found = n > 0 && (size_t)n < size - len;
        break;
    }
    closedir(d);
    return found;
}

static int resolve_ci(const char *volume, const char *rel, char *out, size_t size) {
    char comp[128];
    int n = snprintf(out, size, "%s/", volume);
    if (n < 0 || (size_t)n >= size) return 0;
    while (*rel) {
        while (*rel == '/') rel++;
        if (!*rel) break;
        size_t len = strcspn(rel, "/");
        if (len >= sizeof(comp)) return 0;
        memcpy(comp, rel, len);
        comp[len] = 0;
        rel += len;
        if (!step_ci(out, size, comp)) return 0;
    }
    return 1;
}

static int try_rel(const char *volume, const char *rel, char *out, size_t size) {
    int ok = resolve_ci(volume, rel, out, size);
    printf("  %s/%s: %s\n", volume, rel, ok ? "found" : "not found");
    return ok;
}

int config_locate(const char *volume, int argc, char **argv, char *out, size_t size) {
    char rel[256];
    if (!volume) return 0;
    if (argc > 0 && argv && argv[0] && argv[0][0]) {
        printf("  DOL: %s\n", argv[0]);
        const char *p = strchr(argv[0], ':');
        p = p ? p + 1 : argv[0];
        while (*p == '/') p++;
        const char *slash = strrchr(p, '/');
        size_t n = slash ? (size_t)(slash - p) : 0;
        if (n + sizeof("/config.ini") < sizeof(rel)) {
            memcpy(rel, p, n);
            rel[n] = 0;
            strcat(rel, n ? "/config.ini" : "config.ini");
            if (try_rel(volume, rel, out, size)) return 1;
        }
    } else {
        printf("  DOL: path not provided by loader\n");
    }
    return try_rel(volume, "apps/jellycube/config.ini", out, size);
}
