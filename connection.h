#ifndef _CONNECTION_H_INCLUDED_
#define _CONNECTION_H_INCLUDED_

#include "core.h"
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/rand.h>

#define nonblocking(s)  fcntl(s, F_SETFL, fcntl(s, F_GETFL)|O_NONBLOCK)

typedef ssize_t (*recv_pt)(connection_t *c, u_char *buf, size_t size);
typedef ssize_t (*send_pt)(connection_t *c, u_char *buf, size_t size);

typedef struct {
    SSL_CTX            *ctx;
    SSL                *connection;
    event_handler_pt    saved_read_handler;
    event_handler_pt    saved_write_handler;
    int                 last;
    unsigned            handshaked:1;
} ssl_connection_t;


struct connection_s {
    void               *data;
    event_t            *read;
    event_t            *write;

    int                 fd;

    recv_pt             recv;
    send_pt             send;

    off_t               sent;

    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    char               *addr_text;
    
    ssl_connection_t   *ssl;

    char *              buf;

    queue_t             queue;
};

uint32_t get_free_connection_n(void);
int connection_init(uint32_t n);
connection_t *get_connection(int s);
void free_connection(connection_t *c);
void close_connection(connection_t *c);

ssize_t unix_recv(connection_t *c, u_char *buf, size_t size);
ssize_t unix_send(connection_t *c, u_char *buf, size_t size);
ssize_t unix_udp_recv(connection_t *c, u_char *buf, size_t size);
ssize_t ssl_recv(connection_t *c, u_char *buf, size_t size);
ssize_t ssl_send(connection_t *c, u_char *buf, size_t size);

#endif
