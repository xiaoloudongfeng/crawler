#ifndef _HTTP_CLIENT_H_INCLUDED_
#define _HTTP_CLIENT_H_INCLUDED_

#include "core.h"

typedef struct http_buf_s http_buf_t;
typedef struct http_client_s http_client_t;
typedef struct head_cache_s head_cache_t;
typedef struct html_cache_s html_cache_t;
typedef struct key_value_s  key_value_t;
typedef struct http_vsr_s   http_vsr_t;

struct http_buf_s {
    u_char *start;
    u_char *end;
    u_char *last;
    u_char *pos;
};

struct http_vsr_s {
    uint32_t version;
    uint32_t version_len;
    uint32_t status;
    uint32_t status_len;
    uint32_t reason;
    uint32_t reason_len;
};

struct key_value_s {
    uint32_t key;
    uint32_t key_len;
    uint32_t value;
    uint32_t value_len;

    queue_t  queue;
};

struct head_cache_s {
    enum {
        VSR_START,
        VSR_VER, 
        VSR_STAT, 
        VSR_REAS, 
        LINE_DONE, 
        LINE_HALF, 
        KEY_DONE, 
        HEAD_HALF, 
        HEAD_DONE
    }               status;
    http_vsr_t     *vsr;        // http status line: HTTP-Version SP Status-Code SP Reason-Phrase CRLF 
    uint32_t        start;
    
    queue_t         kv_queue;
    
    uint32_t        content_len;
    unsigned        gzip_flag:1;
    unsigned        chunk_flag:1;
};

struct html_cache_s {
    enum {
        LEN_DONE, 
        LEN_HALF
    }               status;

    uint32_t        html_start;

    uint32_t        start;
    
    uint32_t        chunk_sum;
    http_buf_t     *chunks;
    queue_t         chunk_queue;
    http_buf_t     *html;
};

typedef int (*http_client_handler_pt)(http_client_t *client);

struct http_client_s {
    enum {
        HTTP_CONN_DONE, 
        HTTP_REQ_DONE, 
        HTTP_HEAD_DONE, 
        HTTP_HTML_DONE, 
        HTTP_PARSE_DONE
    }                   status;

    http_url_t         *url;
    struct sockaddr    *sockaddr;
    socklen_t           socklen;
    connection_t       *connection;
    
    http_buf_t         *req;
    http_buf_t         *recv;

    head_cache_t       *head;
    html_cache_t       *html;

    unsigned            ssl_flag:1;
    http_client_handler_pt handler;     // 如果handler判断http_client->status != HTTP_PARSE_DONE，则调用回收函数
};

void http_client_create(http_url_t *url, struct in_addr *sin_addr, http_client_handler_pt handler);
void http_client_retrieve(http_client_t *client);

http_buf_t *create_temp_buf(size_t size);
int resize_temp_buf(http_buf_t *b);
void destroy_temp_buf(http_buf_t *b);

#endif

