#include "core.h"
#include "http_client.h"

static queue_t url_q;

void insert_queue(http_url_t *url)
{
    queue_insert_tail(&url_q, &url->queue);
}

http_url_t *create_http_url(char *schema, char *host, char *path)
{
    http_url_t *url;
    char       *p;
    
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

int main(void)
{
    int          rc, i;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    queue_init(&url_q);

    rc = event_init(1024);
    if (rc < 0) {
        return -1;
    }

    rc = event_timer_init();
    if (rc < 0) {
        return -1;
    }

    rc = connection_init(1024);
    if (rc < 0) {
        return -1;
    }

    rc = dns_init();
    if (rc < 0) {
        return -1;
    }

    rc = bloom_init();
    if (rc < 0) {
        return -1;
    }
    
    char *schemahttp = "http";
    /*
    char *schemahttps = "https";
    char *host1 = "kerinewton.deviantart.com";
    char *host2 = "bbs.dgtle.com";
    char *host3 = "www.zhihu.com";
    char *host4 = "www.wowotech.net";
    char *host5 = "www.baidu.com";
    char *host6 = "www.bilibili.com";
    */
    char *host7 = "bbs.dgtle.com";

    /*
    char *path = "/dgtle_module.php?mod=trade";
    char *path = "/";
    char *path1 = "/index.php";
    char *path2 = "/forum.php?mod=forumdisplay&fid=2";
    char *path1 = "/thread0806.php?fid=16";
    */
    char *path1 = "/forum.php?mod=forumdisplay&fid=2";
   
    /*
    http_url_t *url1 = create_http_url(schemahttp, host1, path);
    rc = dns_query(url1, http_client_create);
    
    http_url_t *url2 = create_http_url(schemahttp, host2, path2);
    rc = dns_query(url2, http_client_create);
    
    http_url_t *url3 = create_http_url(schemahttps, host3, path);
    rc = dns_query(url3, http_client_create);
    
    http_url_t *url4 = create_http_url(schemahttp, host4, path);
    rc = dns_query(url4, http_client_create);

    http_url_t *url5 = create_http_url(schemahttps, host5, path);
    rc = dns_query(url5, http_client_create);
    
    http_url_t *url6 = create_http_url(schemahttp, host6, path);
    rc = dns_query(url6, http_client_create);
    */
    
    http_url_t *url7 = create_http_url(schemahttp, host7, path1);
    rc = dns_query(url7, http_client_create);
    
    i = 0;
    while (1) {
        LOG_INFO("-----round[%d]-----", i++);
        event_process(1000);

        event_expire_timers();

        if (get_free_connection_n() > 1000) {
            queue_t *p = url_q.next;
            
            if (p != &url_q) {
                queue_remove(p);

                http_url_t *url = queue_data(p, http_url_t, queue);
                
                dns_query(url, http_client_create);
            }
        }
        /*
        if (i++ > 120) {
            break;
        }
        */
    }
    
    return 0;
}
