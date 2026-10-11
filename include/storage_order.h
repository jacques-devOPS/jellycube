#ifndef JC_STORAGE_ORDER_H
#define JC_STORAGE_ORDER_H
#include <string.h>
#include <strings.h>
/* Swiss/libfat names for supported physical devices. -1 means unknown. */
static inline int storage_launch_index(const char *path) {
    if (!path) return -1;
    const char *colon = strchr(path, ':');
    if (!colon) return -1;
    size_t n = (size_t)(colon - path);
    if ((n == 5 && !strncasecmp(path, "carda", n)) ||
        (n == 3 && !strncasecmp(path, "sda", n))) return 0;
    if ((n == 5 && !strncasecmp(path, "cardb", n)) ||
        (n == 3 && !strncasecmp(path, "sdb", n))) return 1;
    if (n == 3 && !strncasecmp(path, "sd2", n)) return 2;
    return -1;
}
#endif
