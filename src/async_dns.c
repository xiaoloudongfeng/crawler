#include "core.h"

#include "http_parser.h"

#define DNS_UDP_SIZE    4096
#define DNS_TRY_LIMIT   1

typedef struct dns_cache_s  dns_cache_t;
typedef struct dns_header_s dns_header_t;
typedef struct dns_query_s  dns_query_t;

struct dns_cache_s {
    char            host[DNS_UDP_SIZE];
    struct in_addr  sin_addr;
    queue_t         queue;
};

struct dns_header_s {
    uint16_t    id;         // Transaction ID
    uint16_t    flags;      // Flags
    uint16_t    qdcount;    // Questions
    uint16_t    ancount;    // Answers
    uint16_t    nscount;    // Authority PRs
    uint16_t    arcount;    // Other PRs
    u_char      data[1];    // data
};

struct dns_query_s {
    http_url_t *url;
    queue_t     queue;
    // event ident must be after 3 pointers as in connection_t
    // 这里为了兼容connection_t的格式, 将id放在三个指针后面
    uint32_t    id;
    u_char      pkt[DNS_UDP_SIZE];
    int         len;
    event_t     ev;
    int         trytimes;
    dns_query_handler_pt handler;
};

uint16_t        id;
queue_t         cache_queue;
uint32_t        cache_count;

queue_t         query_queue;
connection_t   *dns_conn;

static dns_cache_t *find_cache(const char *host)
{
    queue_t     *p;
    dns_cache_t *cache;

    p = cache_queue.next;

    while (p != &cache_queue) {
        cache = queue_data(p, dns_cache_t, queue);
        if (!strcmp(cache->host, host)) {
            return cache;
        }

        p = p->next;
    }

    return NULL;
}

static void set_cache(const char *host, const struct in_addr *sin_addr)
{
    dns_cache_t *cache; 
    
    cache = (dns_cache_t *)calloc(1, sizeof(dns_cache_t));
    if (cache) {
        strcpy(cache->host, host);
        cache->sin_addr = *sin_addr;
    
    } else {
        return;
    }
    
    queue_insert_head(&cache_queue, &cache->queue);
    cache_count++;

    if (cache_count > 50) {
        queue_remove(queue_last(&cache_queue));
        cache_count--;
    }
}

static dns_query_t *find_query(uint16_t id)
{
    queue_t     *p;
    dns_query_t *query;

    p = query_queue.next;

    while (p != &query_queue) {
        query = queue_data(p, dns_query_t, queue);
        if (query->id == id) {
            return query;
        }

        p = p->next;
    }

    return NULL;
}

static int pack_udp(u_char *pkt, char *host)
{
    dns_header_t   *header;
    u_char         *chunk_len;
    u_char         *p;
    int             len, i;

    header = (dns_header_t *)pkt;
    header->id      = id;
    header->flags   = htons(0x0100);
    header->qdcount = htons(1);
    header->ancount = 0;
    header->nscount = 0;
    header->arcount = 0;

    chunk_len = (u_char *)&header->data;
    p = chunk_len + 1;
    len = strlen(host) + 1;

    for (i = 0; i < len; ++i, ++p) {
        if (host[i] == '.') {
            *chunk_len = p - chunk_len - 1;
            chunk_len = p;
            continue;
        }

        *p = tolower(host[i]);

        if (host[i] == '\0') {
            *chunk_len = p - chunk_len - 1;
        }
    }

    *p++ = 0;
    *p++ = (uint8_t) (0x01);

    *p++ = 0;
    *p++ = 1;
    
    return p - pkt;
}

static void parse_udp(const unsigned char *pkt, int len)
{
    dns_header_t   *header;
    dns_query_t    *query;
    int             found;
    int             dlen;
    unsigned char  *p;
    uint16_t        type;
    int             rc;
    struct in_addr  sin_addr;

    header = (dns_header_t *)pkt;
    if (ntohs(header->qdcount) != 1) {
        LOG_WARN("qdcount error");
        goto failed;
    }

    query = find_query(header->id);
    if (query == NULL) {
        LOG_WARN("dns response id[%d] match failed", header->id);
        goto failed;
    }

    if (header->ancount == 0) {
        LOG_WARN("ancount is 0");
        goto failed;
    }

    // Skip QNAME
    for (p = &header->data[0]; (p - pkt < len) && (*p != '\0'); p++);

#define NTOHS(p)    (((p)[0] << 8) | (p)[1])
    if ((&p[5] > pkt + len) || NTOHS(p + 1) != 0x01) {
        LOG_WARN("QCLASS match failed");
        goto failed;
    }

    // Skip QTYPE and QCLASS
    p += 5;

    // p -> first section
    for (found = 0; &p[12] < pkt + len; ) {
        if (*p != 0xC0) {
            while (*p && &p[12] < pkt + len) {
                ++p;
            }
            --p;
        }

        type = htons(((uint16_t *)p)[1]);
        if (type == 5) {
            dlen = htons(((uint16_t *)p)[5]);
            p += 12 + dlen;

        } else if (type == 0x01){
            found = 1;
            break;

        } else {
            break;
        }
    }

    if (found && &p[12] < pkt + len) {
        dlen = htons(((uint16_t *)p)[5]);
        p += 12;

        if (dlen != 4) {
            LOG_WARN("maybe not IPV4");
            goto failed;
        }
        LOG_DEBUG("hostname: %s[%d.%d.%d.%d]", query->url->host, p[0], p[1], p[2], p[3]);

        sin_addr.s_addr = htonl(p[0]<<24 | p[1]<<16 | p[2]<<8 | p[3]);
        set_cache(query->url->host, &sin_addr);
        LOG_DEBUG("set_cache done");
        
        query->handler(query->url, &sin_addr);
    }

    queue_remove(&query->queue);
    event_del_timer(&query->ev);
    free(query);

    return;

failed:
    if (query) {
        if (query->trytimes++ > DNS_TRY_LIMIT) {
            LOG_ERROR("get hostname[%s] failed", query->url->host);
            queue_remove(&query->queue);
            event_del_timer(&query->ev);
            // free(query->url->buf);
            // free(query->url);
            free(query);

        } else {
            rc = unix_send(dns_conn, query->pkt, query->len);
            if (rc < 0) {
                LOG_ERROR("unix_send failed");
            } else {
                event_add_timer(&query->ev, 5000);
            }
        }
    }
    return;
}

static void dns_timeout_handler(event_t *ev)
{
    int          rc;
    dns_query_t *query;

    if (ev->timedout) {
        query = ev->data;
        LOG_WARN("get hostname[%s] timeout", query->url->host);

        if (query->trytimes++ > DNS_TRY_LIMIT) {
            LOG_ERROR("get hostname[%s] failed", query->url->host);
            queue_remove(&query->queue);
            // free(query->url->buf);
            // free(query->url);
            free(query);

        } else {
            rc = unix_send(dns_conn, query->pkt, query->len);
            if (rc < 0) {
                LOG_ERROR("unix_send failed");
            
            } else {
                event_add_timer(&query->ev, 5000);
            }
        }
    }
}

static void dns_process_event(event_t *rev)
{
    ssize_t         n;
    connection_t   *c;
    unsigned char   pkt[DNS_UDP_SIZE];

    c = rev->data;

    while (1) {
        n = unix_udp_recv(c, pkt, DNS_UDP_SIZE);
        if (n < 0) {
            if (n == -EAGAIN) {
                LOG_WARN("recive udp EAGAIN");
                return;
            }
            
            LOG_ERROR("recive udp failed %zd", n);
            return;

        } else if (n < sizeof(dns_header_t)) {
            LOG_WARN("recive udp incomplete %zd", n);
            continue;

        } else {
            /*
            {
                int i, j;
                char print[128];

                for (i = 0, j = 0; i < n; ++i) {
                    if (j == 8) {
                        LOG_INFO("%s", print);
                        memset(print, 0, sizeof(print));
                        j = 0;
                    }

                    sprintf(print + j * 3, "%02X ", pkt[i]);
                    j++;
                }
            }
            */

            parse_udp(pkt, n);
        }
    }
}

int dns_init(void)
{
    FILE               *fp;
    char                line[512];
    int                 a, b, c, d;
    int                 fd, rc;
    event_t            *rev;
    struct sockaddr_in  sa;

    queue_init(&query_queue);
    queue_init(&cache_queue);

    fp = fopen("/etc/resolv.conf", "r");
    if (fp == NULL) {
        LOG_ERROR("fopen() resolv.conf failed");
        return -1;
    }

    while (fgets(line, sizeof(line), fp)) {
        if (sscanf(line, "nameserver %d.%d.%d.%d", 
                            &a, &b, &c, &d) == 4)
        {
            LOG_DEBUG("get nameserver: %s", line);
            sa.sin_family      = AF_INET;
            sa.sin_port        = htons(53);
            sa.sin_addr.s_addr = htonl(a << 24 | b << 16 | c << 8 | d);
            
            break;
        }

        memset(line, 0, sizeof(line));
    }

    fclose(fp);

    if (strlen(line) == 0) {
        LOG_ERROR("get nameserver failed");
        return -1;
    }

    fd = socket(AF_INET, SOCK_DGRAM | SOCK_NONBLOCK, 0);
    if (fd == -1) {
        LOG_ERROR("socket() failed");
        return -1;
    }
    
    rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc == -1) {
        LOG_ERROR("connection() failed");
        return -1;
    }

    dns_conn = get_connection(fd);
    if (dns_conn == NULL) {
        if (close(fd) == -1) {
            LOG_ERROR("close() failed");
        }
        return -1;
    }

    rev = dns_conn->read;
    rev->handler = dns_process_event;

    rc = event_add(rev, READ_EVENT, CLEAR_EVENT);
    if (rc == -1) {
        LOG_ERROR("event_add() failed");
        close_connection(dns_conn);
        return -1;
    }

    return 0;
}

int dns_query(http_url_t *url, dns_query_handler_pt handler)
{
    int             rc, len;
    char           *host;
    dns_cache_t    *cache;
    dns_query_t    *query;
    u_char         *pkt;
    event_t        *ev;

    host = url->host;

    if (strlen(host) >= DNS_UDP_SIZE - sizeof(dns_header_t)) {
        LOG_ERROR("host out of range");
        
        return -1;
    }

    cache = find_cache(host);
    if (cache) {
        LOG_DEBUG("find hostname[%s] cache", host);
        
        handler(url, &cache->sin_addr);
        return 0;
    }

    query = (dns_query_t *)calloc(1, sizeof(dns_query_t));
    if (query == NULL) {
        LOG_ERROR("calloc() failed");
        return -1;
    }

    ++id;
    pkt = query->pkt;
    len = pack_udp(query->pkt, host);
    query->id = id;
    query->url = url;
    query->len = len;
    query->handler = handler;

    rc = unix_send(dns_conn, pkt, len);
    if (rc < 0) {
        LOG_ERROR("unix_send() failed");
        free(query);
        return -1;
    }

/*
    {
        int i, j;
        char print[128];

        for (i = 0, j = 0; i < len; ++i) {
            if (j == 8) {
                LOG_INFO("%s", print);
                memset(print, 0, sizeof(print));
                j = 0;
            }

            sprintf(print + j * 3, "%02X ", pkt[i]);
            j++;
        }
    }
*/
    ev = &query->ev;
    ev->data = query;
    ev->handler = dns_timeout_handler;
    event_add_timer(ev, 5000);

    queue_insert_tail(&query_queue, &query->queue);

    LOG_DEBUG("send hostname[%s] req", url->host);

    return 0;
}

http_url_t *create_http_url(const char *str_url)
{
    char        schema[1024];
    char        host[1024];
    char        path[1024];
    struct http_parser_url u;
    
    http_url_t *url;
    char       *p;

    memset(schema, 0, sizeof(schema));
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));

    if (http_parser_parse_url(str_url, strlen(str_url), 0, &u) != 0) {
        LOG_ERROR("http_parser_parse_url() failed");

        return NULL;

    } else {
        if ((u.field_set & (1 << UF_HOST))) {
            strncpy(host, str_url + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
        
        } else {
            LOG_ERROR("host_name don't exist");

            return NULL;
        }

        if ((u.field_set & (1 << UF_SCHEMA))) {
            strncpy(schema, str_url + u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len);
        
        } else {
            LOG_WARN("schema don't exist");

            strcpy(schema, "http");
        }

        if ((u.field_set & (1 << UF_PATH))) {
            strcpy(path, str_url + u.field_data[UF_PATH].off);

        } else {
            LOG_WARN("path don't exist");

            strcpy(path, "/");
        }
    }

    url = (http_url_t *)malloc(sizeof(http_url_t));
    if (url == NULL) {
        LOG_ERROR("malloc() failed");

        return NULL;
    }

    url->buf = (char *)calloc(1024, sizeof(char));
    p = url->buf;

    strcpy(p, schema);
    url->schema = p;
    p += strlen(schema) + 1;

    strcpy(p, host);
    url->host = p;
    p += strlen(host) + 1;

    strcpy(p, path);
    url->path = p;  

    if (strcmp(schema, "https")) {
        url->port = 80;
    
    } else {
        url->port = 443; 
    }

    return url;
}

