#ifndef JC_CONFIG_LOCATE_H
#define JC_CONFIG_LOCATE_H
#include <stddef.h>

/*
 * Find config.ini on a mounted volume (e.g. "carda:").
 * 1. Directory of the launched DOL (argv[0], device prefix ignored).
 * 2. Fallback: apps/jellycube/config.ini.
 * Every path component is matched case-insensitively against the card.
 * Prints each attempt. Returns 1 and writes the real path to out on success.
 */
int config_locate(const char *volume, int argc, char **argv, char *out, size_t size);

#endif
