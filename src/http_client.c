#include "core.h"
#include "http_client.h"

#define BASE_RANGE  4096

#define LINE0   "GET  HTTP/1.1\r\n"
#define LINE1   "Host: \r\n"
#define LINE2   "Connection: keep-alive\r\n"                                                                    \
                "Upgrade-Insecure-Requests: 1\r\n"                                                              \
                "Cache-Control: max-age=0\r\n"                                                                  \
                "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,image/webp,*/*;q=0.8\r\n"        \
                "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) " \
                "Chrome/54.0.2840.71 Safari/537.36\r\n"                                                         \
                "Accept-Encoding: gzip, deflate, sdch\r\n"                                                      \
                "Accept-Language: en-US,en;q=0.8,zh-CN;q=0.6,zh;q=0.4\r\n"                                      \
                "\r\n"
                
                //"Upgrade-Insecure-Requests: 1\r\n"                                                          
                //"Cache-Control: no-cache\r\n"                                                             
                //"Accept-Encoding: deflate, sdch\r\n"                                                      
                //"Accept-Encoding: gzip, deflate, sdch\r\n"                                                

#define TIMEOUT_CONN    5000
#define TIMEOUT_RECV    20000
#define TIMEOUT_SSL     10000
#define TIMEOUT_SEND    5000

static void ssl_handshake_handler(event_t *ev);
static void http_write_event_process(event_t *wev);

// windowbits = 47 ???
static int inflate_gzip(http_buf_t *source, http_buf_t *dest, int windowbits)
{
    LOG_DEBUG("enter inflate_gzip()");
    
    z_stream    strm;
    int         ret;

    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;

    ret = inflateInit2(&strm, windowbits);
    if (ret != Z_OK) {
        LOG_ERROR("inflateInit2() failed");

        return -1;
    }
    
    strm.avail_in = source->last - source->start;
    strm.next_in = source->start;

    do {
        strm.avail_out = dest->end - dest->last;
        strm.next_out = dest->last;

        ret = inflate(&strm, Z_NO_FLUSH);
        //LOG_DEBUG("inflate(): 0x%08X", ret);
        //LOG_DEBUG("strm.avail_out: %d", strm.avail_out);
        switch (ret) {
        case Z_NEED_DICT:
            LOG_ERROR("inflate() failed[Z_NEED_DICT]");
            ret = Z_DATA_ERROR;
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
            LOG_ERROR("inflate() failed[%s]", ret == Z_DATA_ERROR ? "Z_DATA_ERROR" : "Z_MEM_ERROR");
            
            inflateEnd(&strm);
            return -1;
        }

        dest->last = strm.next_out;

        if (strm.avail_out == 0) {
            if (resize_temp_buf(dest) == -1) {
                LOG_ERROR("resize_temp_buf() failed");

                inflateEnd(&strm);
                return -1;
            }
        }

    } while (strm.avail_out == 0);
    
    if (dest->last >= dest->end) {
        resize_temp_buf(dest);
    }
    *dest->last++ = '\0';

    LOG_DEBUG("inflate_gzip() done");

    inflateEnd(&strm);

    return 0;
}

http_buf_t *create_temp_buf(size_t size)
{
    http_buf_t *b;

    b = (http_buf_t *)calloc(1, sizeof(http_buf_t));
    if (b == NULL) {
        LOG_ERROR("calloc() failed");
        return NULL;
    }

    b->start = (u_char *)calloc(size, sizeof(u_char));
    if (b->start == NULL) {
        LOG_ERROR("calloc() failed");
        free(b);
        return NULL;
    }

    b->pos = b->start;
    b->last = b->start;
    b->end = b->last + size;

    return b;
}

void destroy_temp_buf(http_buf_t *b)
{
    if (b) {
        if (b->start) {
            free(b->start);
        }

        free(b);
    }
}

int resize_temp_buf(http_buf_t *b)
{
    size_t      new_range;
    size_t      old_range;
    size_t      pos;
    size_t      last;
    u_char     *start;

    pos = b->pos - b->start;
    last = b->last - b->start;
    old_range = b->end - b->start;

    if (old_range <= 32 * BASE_RANGE) {
        new_range = old_range * 2;

    } else {
        new_range = old_range + BASE_RANGE;
    }

    //LOG_DEBUG("resize_temp_buf new_range: %zu", new_range);

    start = realloc(b->start, new_range);
    if (start == NULL) {
        LOG_ERROR("realloc() failed");
        return -1;
    }

    b->start = start;
    b->pos = start + pos;
    b->last = start + last;
    b->end = start + new_range;

    return 0;
}

static int http_parse_head(http_client_t *client)
{
    http_buf_t     *b;
    http_vsr_t     *vsr;
    head_cache_t   *hc;
    key_value_t    *kv;
    u_char          c;

    queue_t        *q;
    u_char         *k;
    u_char         *v;
    char            kbuf[1024];
    char            vbuf[1024];

    LOG_DEBUG("http_parse_head");

    b = client->recv;
    
    if (client->head == NULL) {
        hc = (head_cache_t *)calloc(1, sizeof(head_cache_t));
        
        if (hc == NULL) {
            LOG_ERROR("calloc() failed");
            return -1;
        }

        client->head = hc;

        hc->status = VSR_START;
        hc->start = 0;

        queue_init(&hc->kv_queue);
    
    } else {
        hc = client->head;
    }

    if (hc->vsr == NULL) {
        vsr = (http_vsr_t *)calloc(1, sizeof(http_vsr_t));
        if (vsr == NULL) {
            LOG_ERROR("calloc() failed");
            return -1;
        }

        hc->vsr = vsr;
    
    } else {
        vsr = hc->vsr;
    }
        
    kv = NULL;

    while (b->pos < b->last) {
        c = *b->pos;
        b->pos++;
        
        switch (c) {
        case ' ':
            switch (hc->status) {
            case VSR_START:
                vsr->version = hc->start;
                vsr->version_len = (b->pos - 1 - b->start) - hc->start;

                hc->status = VSR_VER;
                hc->start = b->pos - b->start;
                
                break;

            case VSR_VER:
                vsr->status = hc->start;
                vsr->status_len = (b->pos - 1 - b->start) - hc->start;
                
                hc->status = VSR_STAT;
                hc->start = b->pos - b->start;
                
                break;
            
            // skip the spaces of the head of value
            case KEY_DONE:
                hc->start = b->pos - b->start;
                
                break;

            default:
                break;
            }
            break;

        case ':':
            if (hc->status == LINE_DONE) {
                hc->status = KEY_DONE;

                kv = (key_value_t *)calloc(1, sizeof(key_value_t));
                if (kv == NULL) {
                    LOG_ERROR("calloc() failed");
                    return -1;
                }

                kv->key = hc->start;
                kv->key_len = (b->pos - 1 - b->start) - hc->start;

                queue_insert_tail(&hc->kv_queue, &kv->queue);

                hc->start = b->pos - b->start;
            }
            break;

        case '\r':
            switch (hc->status) {
            case KEY_DONE:
                hc->status = LINE_HALF;
                
                if (kv == NULL) {
                    if (queue_empty(&hc->kv_queue)) {
                        LOG_ERROR("case \'\\n\', queue empty");
                        return -1;
                    }
                    
                    kv = (key_value_t *)queue_data(queue_last(&hc->kv_queue), key_value_t, queue);
                }
                
                kv->value = hc->start;
                // * * * \r \n *    ->      * * * \r \n *
                //          pos                   pos
                kv->value_len = (b->pos - 1 - b->start) - hc->start;
                kv = NULL;

                break;

            case LINE_DONE:
                hc->status = HEAD_HALF;
                
                break;

            case VSR_STAT:
                vsr->reason = hc->start;
                vsr->reason_len = (b->pos - 1 - b->start) - hc->start;

                hc->status = VSR_REAS;
                hc->start = b->pos - b->start;
                
                break;

            default:
                LOG_ERROR("case \\r, status don't match");
                return -1;
            }
            break;
        
        case '\n':
            switch (hc->status) {
            case LINE_HALF:
                hc->status = LINE_DONE;

                hc->start = b->pos - b->start;
                break;

            case HEAD_HALF:
                hc->status = HEAD_DONE;
                
                client->status = HTTP_HEAD_DONE;

                goto parsed;
                
                break;
            
            case VSR_REAS:
                hc->status = LINE_DONE;

                hc->start = b->pos - b->start;

                break;

            default:
                LOG_ERROR("case \'\\n\', don't match");
                return -1;
            }
        }
    }

    return 0;

parsed:
    memset(kbuf, 0, sizeof(kbuf));
    k = vsr->status + b->start;
    strncpy(kbuf, (char *)k, vsr->status_len);
    if (strcmp("200", kbuf)) {
        LOG_ERROR("status-line status error[%s]", kbuf);

        return -1;
    }
    
    q = hc->kv_queue.next;

    while (q != &hc->kv_queue) {

        kv = (key_value_t *)queue_data(q, key_value_t, queue);
        k = kv->key + b->start;
        v = kv->value + b->start;

        memset(kbuf, 0, sizeof(kbuf));
        memset(vbuf, 0, sizeof(vbuf));
        strncpy(kbuf, (char *)k, kv->key_len);
        strncpy(vbuf, (char *)v, kv->value_len);

        if (!strcmp("Content-Length", kbuf)) {
            hc->content_len = atoi(vbuf);
            LOG_DEBUG(" content_len = %d", hc->content_len);
        }

        if (!strcmp("Transfer-Encoding", kbuf)) {
            if (!strcmp("chunked", vbuf)) {
                hc->chunk_flag = 1;
                LOG_DEBUG(" chunk_flag = 1");
            }
        }

        if (!strcmp("Content-Encoding", kbuf)) {
            if (!strcmp("gzip", vbuf)) {
                hc->gzip_flag = 1;
                LOG_DEBUG(" gzip_flag = 1");
            }
        }

        if (!strcmp("Content-Type", kbuf)) {
            // TODO
        }

        //LOG_INFO("%s:%s", kbuf, vbuf);

        q = q->next;
    }

    return 0;
}

static int http_parse_html(http_client_t *client)
{
    head_cache_t   *hc;
    html_cache_t   *mc;
    key_value_t    *kv;
    http_buf_t     *b, *chunks;
    u_char          c, *p;
    uint32_t        chunk_len;
    http_buf_t     *dest;

    queue_t        *q;

    hc = client->head;
    b = client->recv;

    if (client->html == NULL) {
        mc = (html_cache_t *)calloc(1, sizeof(html_cache_t));
        if (mc == NULL) {
            LOG_ERROR("calloc() failed");

            return -1;
        }
        
        mc->html_start = b->pos - b->start;

        queue_init(&mc->chunk_queue);

        client->html = mc;

    } else {
        mc = client->html;
    }

    /*
     * chunked编码：
     * chunked长度1 0d 0a chunked块1
     * 0d 0a chunked长度2 0d 0a chunked块2
     * ...
     * 0d 0a chunked长度n 0d 0a chunked块n
     * 0d 0a 30 0d 0a 0d 0a
     */
    if (hc->chunk_flag) {
        kv = NULL;

        while (b->pos < b->last) {
            chunk_len = 0;
            
            while (b->pos < b->last) {
                c = *b->pos;
                b->pos++;

                switch(c) {
                case ' ':
                case '\t':
                    break;
                
                case '\r':
                    if (mc->status == LEN_DONE) {
                        mc->status = LEN_HALF;

                    } else {
                        LOG_ERROR("case \'\\r\', chunk len error");
                        return -1;
                    }
                    break;
                
                case '\n':
                    if (mc->status == LEN_HALF) {
                        mc->status = LEN_DONE;
                        
                        goto loop;
                    }
                    break;

                default:
                    //LOG_INFO("%c", c);
                    chunk_len <<= 4;

                    if (c >= '0' && c <= '9') {
                        chunk_len += c - '0';

                    } else if (c >= 'A' && c <= 'F') {
                        chunk_len += c - 'A' + 10;

                    } else if (c >= 'a' && c <= 'f') {
                        chunk_len += c - 'a' + 10;

                    } else {
                        LOG_ERROR("get chunk len error[%c]", c);
                    }
                    break;
                }
            }

            return 0;

        loop:
            LOG_DEBUG("chunk_len: %d", chunk_len);
            if (chunk_len == 0) {
                chunks = create_temp_buf(mc->chunk_sum);
                if (chunks == NULL) {
                    return -1;
                }
                
                mc->chunks = chunks;

                p = chunks->start;

                q = mc->chunk_queue.next;

                while (q != &mc->chunk_queue) {
                    kv = queue_data(q, key_value_t, queue);
                    //LOG_INFO("start[%d] len[%d]", kv->value, kv->value_len);

                    memcpy(p, b->start + kv->value, kv->value_len);
                    p += kv->value_len;

                    q = q->next;
                }

                chunks->last = p + 1;

                break;
            }

            kv = (key_value_t *)calloc(1, sizeof(key_value_t));
            if (kv == NULL) {
                LOG_ERROR("calloc() failed");
                return -1;
            }

            kv->value = b->pos - b->start;
            kv->value_len = chunk_len;
            
            queue_insert_tail(&mc->chunk_queue, &kv->queue);

            kv = NULL;
        
            b->pos += chunk_len + 2;
            
            mc->start = b->pos - b->start;

            mc->chunk_sum += chunk_len;
        }

    } else {
        if (client->head->content_len) {
            LOG_DEBUG("b->last - b->pos: %ld", b->last - b->pos);
            //LOG_DEBUG("%s", b->pos + client->head->content_len);
            LOG_DEBUG("client->head->content_len: %d", client->head->content_len);

            if (b->last - b->pos >= client->head->content_len) {
                chunks = create_temp_buf(client->head->content_len);
                if (chunks == NULL) {
                    return -1;
                }
        
                mc->chunks = chunks;
                mc->chunk_sum = client->head->content_len;

                memcpy(chunks->start, b->pos, client->head->content_len);
                chunks->last = chunks->start + client->head->content_len;
            }
        }
    }

    if (mc->chunks) {
        size_t size;

        size = hc->gzip_flag ? BASE_RANGE : mc->chunk_sum; 

        dest = create_temp_buf(size);
        if (dest == NULL) {
            LOG_ERROR("create_temp_buf() failed");
    
            return -1;
        }
    
        mc->html = dest;
        
        if (hc->gzip_flag) {
            if (inflate_gzip(mc->chunks, dest, 47)) {
                LOG_ERROR("inflate_gzip() failed");
    
                return -1;
            }
    
            client->status = HTTP_HTML_DONE;
    
        } else {
            memcpy(dest->start, mc->chunks->start, mc->chunk_sum);
            dest->last = dest->start + mc->chunk_sum;
            
            client->status = HTTP_HTML_DONE;
        }
    }

    return 0;
}

static int http_process_response(http_client_t *client)
{
    if (client->status == HTTP_REQ_DONE) {
        if (http_parse_head(client) < 0) {
            return -1;
        }
    }

    if (client->status == HTTP_HEAD_DONE) {
        if (http_parse_html(client) < 0) {
            return -1;
        }
    }

    return 0;
}

static void http_read_event_process(event_t *rev)
{
    connection_t   *c;
    http_client_t  *client;
    http_buf_t     *b;
    size_t          size;
    ssize_t         n;
    uint32_t        reset_time;

    LOG_DEBUG("http_read_event_process");

    c = rev->data;
    client = c->data;

    if (rev->timedout) {
        LOG_ERROR("html[%s://%s%s] time out", client->url->schema, client->url->host, client->url->path);
        
        goto failed;
    }

    if (client->recv == NULL) {
        b = create_temp_buf(BASE_RANGE);
        if (b == NULL) {
            LOG_ERROR("create_temp_buf() failed");

            goto failed;
        }
        
        client->recv = b;

    } else {
        b = client->recv;
    }

    reset_time = 0;

    // 1. recv data
    for ( ;; ) {
        if (b->last == b->end) {
            if (resize_temp_buf(b) < 0) {
                LOG_ERROR("resize_temp_buf() failed");

                goto failed;
            }
        }

        size = b->end - b->last;
        n = c->recv(c, b->last, size);
        if (n > 0) {
            b->last += n;

            reset_time = 1;
        }

        if (n == -EAGAIN || n == 0) {
            break;
        }

        if (n == -1) {
            LOG_ERROR("c->recv()[%zu] failed", n);

            goto failed;
        }
    }

    if (reset_time) {
        event_add_timer(rev, TIMEOUT_RECV);
    }

    // 2. parse data
    if (http_process_response(client) < 0) {
        LOG_ERROR("http_process_response() failed");

        goto failed;
    }

    // 3.server closed socket or html receive done
    if (client->status == HTTP_HTML_DONE || rev->pending_eof) {
        /*
        LOG_DEBUG("client->html->html->start: %p, client->html->html->last: %p", 
            client->html->html->start, client->html->html->last);
        LOG_DEBUG("%s", client->html->html->start);
        */
    
        if (rev->pending_eof) {
            LOG_DEBUG("server closed socket");
        }
        
        if (client->handler) {
            LOG_INFO("parsing html[%s://%s%s]", client->url->schema, client->url->host, client->url->path);
            client->handler(client);
        }
    }

    return;

failed:
    if (client->handler) {
        client->handler(client);
    }
}

static int ssl_create(connection_t *c)
{
    ssl_connection_t   *ssl;

    //LOG_INFO("ssl_create()");

    ssl = (ssl_connection_t *)calloc(1, sizeof(ssl_connection_t));
    if (ssl == NULL) {
        LOG_ERROR("calloc() failed");
        return -1;
    }

    ssl->ctx = SSL_CTX_new(SSLv23_method());
    if (ssl->ctx == NULL) {
        LOG_ERROR("SSL_CTX_new() failed");
        goto failed1;
    }

    ssl->connection = SSL_new(ssl->ctx);
    if (ssl->connection == NULL) {
        LOG_ERROR("SSL_new() failed");
        goto failed2;
    }

    if (SSL_set_fd(ssl->connection, c->fd) == 0) {
        LOG_ERROR("SSL_set_fd() failed");
        goto failed3;
    }

    c->read->handler = ssl_handshake_handler;
    c->write->handler = ssl_handshake_handler;
    if (!c->write->active) {
        if (event_add(c->write, WRITE_EVENT, CLEAR_EVENT) != 0) {
            LOG_ERROR("event_add() failed");
            goto failed3;
        }
    }

    if (!c->read->active) {
        if (event_add(c->read, READ_EVENT, CLEAR_EVENT) != 0) {
            LOG_ERROR("event_add() failed");
            goto failed3;
        }
    }
    SSL_set_connect_state(ssl->connection);

    c->ssl = ssl;

    return 0;

failed3:
    SSL_free(ssl->connection);
failed2:
    SSL_CTX_free(ssl->ctx);
failed1:
    free(ssl);

    return -1;
}

static int http_ssl_handshake(connection_t *c)
{
    int rc, sslerr, err;

    LOG_DEBUG("entering http_ssl_handshake()");

    if (c->ssl == NULL) {
        if (ssl_create(c) < 0) {
            return -1;
        }
    }

    rc = SSL_connect(c->ssl->connection);

    if (rc == 1) {
        LOG_DEBUG("SSL_connect() success");

        c->ssl->handshaked = 1;
        c->recv = ssl_recv;
        c->send = ssl_send;

        return 0;
    } else {
        LOG_WARN("SSL_do_handshake: %d", rc);
    }

    sslerr = SSL_get_error(c->ssl->connection, rc);

    LOG_DEBUG("SSL_get_error: %d", sslerr);

    if (sslerr == SSL_ERROR_WANT_READ || sslerr == SSL_ERROR_WANT_WRITE) {
        return EAGAIN;
    }

    err = (sslerr == SSL_ERROR_SYSCALL) ? errno : 0;
    if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        LOG_ERROR("peer closed connection in SSL handshake");
        return -1;
    }

    LOG_ERROR("SSL_do_handshake() failed err[%d] sslerr[%d]", err, sslerr);

    return -1;
}

static void ssl_handshake_handler(event_t *ev)
{
    connection_t   *c;
    http_client_t  *client;
    int             rc;

    c = ev->data;
    client = c->data;

    LOG_DEBUG("SSL handshake handler");

    if (ev->timedout) {
        LOG_ERROR("html[%s://%s%s] time out", client->url->schema, client->url->host, client->url->path);
        
        goto failed;
    }

    rc = http_ssl_handshake(c);
    if (rc == EAGAIN) {
        event_add_timer(c->write, TIMEOUT_SSL);
        return;
    }

    if (rc < 0) {
        goto failed;
    }
    
    if (c->ssl->handshaked) {
        c->write->handler = http_write_event_process;
        event_add_timer(c->write, TIMEOUT_SEND);
    }
    
    return;

failed:
    LOG_ERROR("html[%s://%s%s] failed", client->url->schema, client->url->host, client->url->path);

    if (client->handler) {
        client->handler(client);
    }
}

static int pack_req_buf(http_client_t *client)
{
    http_url_t *url;
    http_buf_t *b;
    int         req_buf_len;

    url = client->url;
    req_buf_len = strlen(LINE0) + strlen(LINE1) + strlen(LINE2) + 
            strlen(url->host) + strlen(url->path) + 1;

    b = create_temp_buf(req_buf_len);
    if (b == NULL) {
        return -1;
    }

    sprintf((char *) b->start, "GET %s HTTP/1.1\r\n"
                    "Host: %s\r\n" LINE2, url->path, url->host);

    b->last += req_buf_len;

    client->req = b;

    return 0;
}

static void http_write_event_process(event_t *wev)
{
    connection_t   *c;
    http_client_t  *client;
    http_buf_t     *b;
    size_t          size;
    ssize_t         n;
    event_t        *rev;

    c = wev->data;
    client = c->data;

    LOG_DEBUG("http_write_event_process");

    if (wev->timedout) {
        LOG_ERROR("html[%s://%s%s] time out", client->url->schema, client->url->host, client->url->path);
        
        goto failed;
    }

    if (client->req == NULL) {
        if (pack_req_buf(client) < 0 ) {
            LOG_ERROR("pack_req_buf() failed");
            goto failed;
        }
    }

    b = client->req;
    size = b->last - b->pos;

    if (size && wev->ready) {
        n = c->send(c, b->pos, size);
        if (n == -1) {
            LOG_ERROR("send() failed");
            goto failed;
        }

        if (n > 0) {
            b->pos += n;
        }

        if (n == -EAGAIN) {
            event_add_timer(wev, TIMEOUT_SEND);
        }
    }

    /* trans end */
    if (b->pos == b->last) {
        if (wev->timer_set) {
            event_del_timer(wev);
        }
        
        if (wev->active) {
            if (event_del(wev, WRITE_EVENT, 0) < 0) {
                LOG_ERROR("event_del() failed");
                goto failed;
            }
        }

        rev = c->read;
        rev->handler = http_read_event_process;

        if (!rev->active) {
            if (event_add(rev, READ_EVENT, CLEAR_EVENT) < 0) {
                LOG_ERROR("event_add() failed");
                goto failed;
            }
        }

        event_add_timer(rev, TIMEOUT_RECV);

        client->status = HTTP_REQ_DONE;

        return;
    }

failed:
    if (client->handler) {
        client->handler(client);
    }
}

static int http_conn(http_client_t *client)
{
    int                 fd;
    connection_t       *c;
    event_t            *wev;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        LOG_ERROR("socket() failed");
        return -1;
    }

    c = get_connection(fd);
    if (c == NULL) {
        LOG_ERROR("get_connection() failed");
        close(fd);
        return -1;
    }

    if (nonblocking(fd) < 0) {
        LOG_ERROR("nonblocking(%d) failed", fd);
        goto failed;
    }

    wev = c->write;

    c->recv = unix_recv;
    c->send = unix_send;
    
    if (client->ssl_flag) {
        wev->handler = ssl_handshake_handler;

    } else {
        wev->handler = http_write_event_process;
    }
    c->data = client;

    if (event_add(wev, WRITE_EVENT, CLEAR_EVENT) < 0) {
        LOG_ERROR("event_add_conn() failed");
        goto failed;
    }

    if (connect(fd, client->sockaddr, client->socklen) < 0) {
        if (errno != EINPROGRESS) {
            LOG_ERROR("connect() failed");
           
            // 加不加都一样，close_connection() 中会调用 close(fd)
            // event_del(wev, WRITE_EVENT, 0);
            goto failed;
        }
    
        event_add_timer(wev, TIMEOUT_CONN);
    }

    client->connection = c;
    client->status = HTTP_CONN_DONE;

    return 0;

failed:
    close_connection(c);
    return -1;
}

void http_client_retrieve(http_client_t *client)
{
    connection_t *c;

    if (client->url) {
        LOG_DEBUG("retrieving resource[%s://%s%s]...", client->url->schema, client->url->host, client->url->path);
        free(client->url->buf);
        free(client->url);
    }

    if (client->sockaddr) {
        free(client->sockaddr);
    }

    if (client->connection) {
        c = client->connection;
        if (c->ssl) {
            SSL_free(c->ssl->connection);
            SSL_CTX_free(c->ssl->ctx);
            free(c->ssl);
            c->ssl = NULL;
        }
        close_connection(c);
        client->connection = NULL;
    }
    
    destroy_temp_buf(client->req);
    destroy_temp_buf(client->recv);

    if (client->head) {
        if (client->head->vsr) {
            free(client->head->vsr);
        }

        queue_t *q = &client->head->kv_queue;
        while (q != q->next) {
            key_value_t *tmp = queue_data(q->next, key_value_t, queue);
            queue_remove(q->next);
            free(tmp);
        }

        free(client->head);
    }

    if (client->html) {
        if (client->html->chunks) {
            destroy_temp_buf(client->html->chunks);
        }

        queue_t *q = &client->html->chunk_queue;
        while (q != q->next) {
            key_value_t *tmp = queue_data(q->next, key_value_t, queue);
            queue_remove(q->next);
            free(tmp);
        }

        if (client->html->html) {
            destroy_temp_buf(client->html->html);
        }

        free(client->html);
    }
   
    free(client);
    LOG_DEBUG("retrieving resource done");
}

void http_client_create(http_url_t *url, struct in_addr *sin_addr, http_client_handler_pt handler)
{
    struct sockaddr_in *sa;
    http_client_t      *client;

    client = (http_client_t *)calloc(1, sizeof(http_client_t));
    if (client == NULL) {
        LOG_ERROR("calloc() failed");

        goto failed;
    }

    sa = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    if (sa == NULL) {
        LOG_ERROR("calloc() failed");
        
        free(client);
        goto failed;
    }

    sa->sin_family = AF_INET;
    sa->sin_port = htons(url->port);
    sa->sin_addr = *sin_addr;
    
    client->handler = handler;
    client->sockaddr = (struct sockaddr *)sa;
    client->socklen = sizeof(*sa);
    client->url = url;
    if (!strcmp("https", url->schema)) {
        client->ssl_flag = 1;
    }

    if (http_conn(client) < 0) {
        LOG_ERROR("http_client_conn() failed");

        if (client->handler) {
            client->handler(client);
        }
    }

    return;

failed:
    free(url->buf);
    free(url);
}

