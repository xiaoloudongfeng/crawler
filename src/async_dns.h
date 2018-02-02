#ifndef _ASYNC_DNS_H_INCLUDED_
#define _ASYNC_DNS_H_INCLUDED_

#include "core.h"

typedef struct http_url_s http_url_t;

struct http_url_s {
    char    *buf;
    uint16_t port;
    char    *schema;
    char    *host;
    char    *path;

    queue_t queue;
};

typedef void (*dns_query_handler_pt)(http_url_t *url, struct in_addr *sin_addr);

int dns_init(void);

int dns_query(http_url_t *url, dns_query_handler_pt handler);

http_url_t *create_http_url(const char *url);

#endif

