/* Host test: config_locate + config_load against a mock SD directory.
 * Usage: host_config_test <volume-dir> <argv0 or "">  */
#include <stdio.h>
#include "config.h"
#include "config_locate.h"
int main(int argc, char **argv) {
    if (argc < 3) return 3;
    char path[300];
    jellyfin_config_t c;
    char *av[] = { argv[2], NULL };
    if (!config_locate(argv[1], argv[2][0] ? 1 : 0, av, path, sizeof(path))) { printf("RESULT: NOT FOUND\n"); return 1; }
    if (config_load(path, &c) < 0) { printf("RESULT: PARSE FAIL %s\n", path); return 2; }
    printf("RESULT: OK %s -> %s:%d user=%s\n", path, c.server_address, c.server_port, c.username);
    return 0;
}
