#include <stdio.h>
#include <network.h>
#include "jc_network.h"
s32 network_init(void) {
    char ip[16]={0},mask[16]={0},gateway[16]={0};
    printf("Broadband Adapter: DHCP...\n");
    if(if_config(ip,mask,gateway,true,20)<0) return -1;
    printf("IP: %s  Gateway: %s\n",ip,gateway);return 0;
}
/* libbba has no net_deinit entry point. HTTP closes every socket. */
void network_deinit(void) {}
