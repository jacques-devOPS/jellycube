#ifndef JC_HTTP_H
#define JC_HTTP_H
#include "platform.h"
#define HTTP_BUFFER_SIZE 2048
typedef struct {
    s32 socket;
    char host[256];int port,status;
    unsigned char buffer[HTTP_BUFFER_SIZE];int pos,used;
} http_connection_t;
s32 http_connect(http_connection_t *c,const char *host,int port);
s32 http_send_request(http_connection_t *c,const char *method,const char *path,const char *body,const char *auth);
s32 http_receive_response(http_connection_t *c,char *out,int cap);
void http_close(http_connection_t *c);
#endif
