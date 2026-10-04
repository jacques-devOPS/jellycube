#ifndef JC_HTTP_H
#define JC_HTTP_H
#include "platform.h"
#define HTTP_BUFFER_SIZE 2048

/* Last stage reached by a connection. Reported on failure. */
enum {
    HTTP_STAGE_NONE = 0,
    HTTP_STAGE_ADDRESS,
    HTTP_STAGE_SOCKET,
    HTTP_STAGE_CONNECT,
    HTTP_STAGE_SEND,
    HTTP_STAGE_RECEIVE,
    HTTP_STAGE_PARSE,
    HTTP_STAGE_STATUS,
    HTTP_STAGE_DONE
};

typedef struct {
    s32 socket;
    char host[256];
    int port, status;
    int stage;      /* HTTP_STAGE_* */
    s32 error;      /* Negative errno from the socket layer, 0 if none. */
    unsigned char buffer[HTTP_BUFFER_SIZE];
    int pos, used;
} http_connection_t;

const char *http_stage_name(int stage);
s32 http_connect(http_connection_t *c, const char *host, int port);
s32 http_send_request(http_connection_t *c, const char *method, const char *path,
                      const char *body, const char *auth);
s32 http_receive_response(http_connection_t *c, char *out, int cap);
void http_close(http_connection_t *c);
#endif
