#include <stdio.h>
#include <string.h>
#include "config.h"
#include "jellyfin.h"

int main(void) {
    jellyfin_config_t config = {
        .server_address = "<JELLYFIN_IP>",
        .server_port    = 8096,
        .username       = "gamecube",
        .password       = "<JELLYFIN_PASSWORD>",
        .device_id      = "hosttest",
        .client_name    = "JellyCube-HostTest",
        .client_version = "0.2-test"
    };
    jellyfin_client_t client = {.config = &config};
    printf("Host auth test against %s:%d\n", config.server_address, config.server_port);
    if (jellyfin_authenticate(&client) < 0) {
        printf("Authentication failed.\n");
        return 1;
    }
    printf("Success. user_id=%s token=%.16s...\n", client.user_id, client.access_token);
    return 0;
}
