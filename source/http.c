/*
 * Blocking HTTP/1.1 client.
 *
 * libogc's GameCube lwIP stack does not support non-blocking sockets:
 * net_fcntl() is a stub that always returns -1, and net_select()
 * dereferences the read set unconditionally. net_connect() blocks on
 * netconn_connect() and returns a negative errno. This client therefore
 * uses plain blocking calls only.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#ifdef JC_HOST_TEST
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
static int net_socket(int d, int t, int p) { int r = socket(d, t, p); return r < 0 ? -errno : r; }
static int net_connect(int s, struct sockaddr *a, socklen_t l) { int r = connect(s, a, l); return r < 0 ? -errno : r; }
static int net_close(int s) { return close(s); }
static int net_send(int s, const void *p, int n, int f) { int r = send(s, p, n, f | MSG_NOSIGNAL); return r < 0 ? -errno : r; }
static int net_recv(int s, void *p, int n, int f) { int r = recv(s, p, n, f); return r < 0 ? -errno : r; }
#else
#include <network.h>
#endif
#include "http.h"

const char *http_stage_name(int stage) {
    switch (stage) {
        case HTTP_STAGE_ADDRESS: return "address";
        case HTTP_STAGE_SOCKET:  return "socket";
        case HTTP_STAGE_CONNECT: return "TCP connect";
        case HTTP_STAGE_SEND:    return "HTTP send";
        case HTTP_STAGE_RECEIVE: return "HTTP receive";
        case HTTP_STAGE_PARSE:   return "HTTP parse";
        case HTTP_STAGE_STATUS:  return "HTTP status";
        case HTTP_STAGE_DONE:    return "done";
        default:                 return "none";
    }
}

s32 http_connect(http_connection_t *c, const char *host, int port) {
    memset(c, 0, sizeof(*c));
    c->socket = -1;
    c->stage = HTTP_STAGE_ADDRESS;
    if (!host || strlen(host) >= sizeof(c->host) || port < 1 || port > 65535) return -1;
    strcpy(c->host, host);
    c->port = port;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = inet_addr(host);
    /* GameCube libbba has no DNS. Require a literal IPv4 address. */
    if (addr.sin_addr.s_addr == 0xffffffffU) return -1;

    c->stage = HTTP_STAGE_SOCKET;
    s32 s = net_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (s < 0) { c->error = s; return -1; }
    c->socket = s;

    c->stage = HTTP_STAGE_CONNECT;
    s32 r = net_connect(c->socket, (struct sockaddr *)&addr, sizeof(addr));
    if (r < 0) { c->error = r; http_close(c); return -1; }

    c->stage = HTTP_STAGE_SEND;
    return 0;
}

static int send_all(http_connection_t *c, const char *p, int n) {
    c->stage = HTTP_STAGE_SEND;
    while (n > 0) {
        int r = net_send(c->socket, p, n, 0);
        if (r == -EINTR || r == -EAGAIN) continue;
        if (r <= 0) { c->error = r ? r : -EPIPE; return -1; }
        p += r;
        n -= r;
    }
    return 0;
}

s32 http_send_request(http_connection_t *c, const char *method, const char *path,
                      const char *body, const char *auth) {
    char header[2048];
    int n = snprintf(header, sizeof(header),
        "%s %s HTTP/1.1\r\nHost: %s:%d\r\n%sAccept: application/json\r\n"
        "Accept-Encoding: identity\r\nContent-Type: application/json\r\n"
        "Content-Length: %lu\r\nConnection: close\r\n\r\n",
        method, path, c->host, c->port, auth ? auth : "",
        (unsigned long)(body ? strlen(body) : 0));
    if (n < 0 || n >= (int)sizeof(header)) { c->stage = HTTP_STAGE_SEND; return -1; }
    if (send_all(c, header, n) < 0) return -1;
    if (body && send_all(c, body, (int)strlen(body)) < 0) return -1;
    c->stage = HTTP_STAGE_RECEIVE;
    return 0;
}

/* Returns 1 on byte, 0 on orderly close, -1 on socket error. */
static int byte(http_connection_t *c, unsigned char *v) {
    if (c->pos == c->used) {
        int r;
        do {
            r = net_recv(c->socket, c->buffer, sizeof(c->buffer), 0);
        } while (r == -EINTR || r == -EAGAIN);
        if (r < 0) { c->error = r; return -1; }
        if (r == 0) return 0;
        c->used = r;
        c->pos = 0;
    }
    *v = c->buffer[c->pos++];
    return 1;
}

static int line(http_connection_t *c, char *out, size_t cap) {
    size_t n = 0;
    unsigned char v;
    while (1) {
        if (byte(c, &v) != 1 || n + 1 >= cap) return -1;
        if (v == '\n') {
            if (!n || out[n - 1] != '\r') return -1;
            out[n - 1] = 0;
            return 0;
        }
        out[n++] = v;
    }
}

static int read_n(http_connection_t *c, char *out, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (byte(c, (unsigned char *)out + i) != 1) return -1;
    return 0;
}

s32 http_receive_response(http_connection_t *c, char *out, int cap) {
    char h[2048];
    size_t used = 0, header_bytes = 0;
    long length = -1;
    int chunked = 0;
    if (cap < 1) return -1;
    out[0] = 0;

    c->stage = HTTP_STAGE_RECEIVE;
    if (line(c, h, sizeof(h)) < 0) return -1;
    c->stage = HTTP_STAGE_PARSE;
    if (sscanf(h, "HTTP/%*d.%*d %d", &c->status) != 1) return -1;

    while (1) {
        if (line(c, h, sizeof(h)) < 0) return -1;
        header_bytes += strlen(h) + 2;
        if (header_bytes > 16384) return -1;
        if (!*h) break;
        if (!strncasecmp(h, "Content-Length:", 15)) {
            char *e;
            const char *v = h + 15;
            length = strtol(v, &e, 10);
            while (*e == ' ' || *e == '\t') e++;
            if (e == v || *e || length < 0 || length >= cap) return -1;
        } else if (!strncasecmp(h, "Transfer-Encoding:", 18)) {
            const char *v = h + 18;
            while (*v == ' ' || *v == '\t') v++;
            if (strcasecmp(v, "chunked")) return -1;
            chunked = 1;
        } else if (!strncasecmp(h, "Content-Encoding:", 17)) {
            const char *v = h + 17;
            while (*v == ' ' || *v == '\t') v++;
            if (strcasecmp(v, "identity")) return -1;
        }
    }

    if (chunked) {
        while (1) {
            if (line(c, h, sizeof(h)) < 0 || !*h || *h == '-' || *h == '+') return -1;
            char *e;
            unsigned long n = strtoul(h, &e, 16);
            if (e == h || (*e && *e != ';') || n > (size_t)cap - 1 - used) return -1;
            if (!n) {
                do {
                    if (line(c, h, sizeof(h)) < 0) return -1;
                    header_bytes += strlen(h) + 2;
                    if (header_bytes > 16384) return -1;
                } while (*h);
                break;
            }
            if (read_n(c, out + used, n) < 0 || line(c, h, sizeof(h)) < 0 || *h) return -1;
            used += n;
        }
    } else if (length >= 0) {
        if (read_n(c, out, length) < 0) return -1;
        used = length;
    } else {
        unsigned char v;
        int r;
        while ((r = byte(c, &v)) == 1) {
            if (used >= (size_t)cap - 1) return -1;
            out[used++] = v;
        }
        if (r < 0) return -1;
    }
    out[used] = 0;

    if (c->status < 200 || c->status >= 300) {
        c->stage = HTTP_STAGE_STATUS;
        return -1;
    }
    c->stage = HTTP_STAGE_DONE;
    return (s32)used;
}

void http_close(http_connection_t *c) {
    if (c->socket >= 0) net_close(c->socket);
    c->socket = -1;
}
