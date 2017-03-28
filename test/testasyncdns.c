#include "core.h"

void handler(http_url_t *url, struct in_addr *sin_addr)
{
    LOG_INFO("hostname[%s] handler", url->host);
}

int main(void)
{
    int         rc, i;
    http_url_t  url1, url2, url3, url4, url5, url6, url7, url8;

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
    
    url1.host = "www.gwergweg.com";
    url2.host = "www.hrtjrtjtrjj.com";
    url3.host = "www.erhghfjle.com";
    url4.host = "www.hertjtrd.com";
    url5.host = "www.htrjkykawt.com";
    url6.host = "www.wgehwaogeh.com";
    url7.host = "www.gwephooieoi.com";
    url8.host = "www.hwohgpyyer.com";

    //for (i = 0; i < 8192; i++) {
        rc = dns_query(&url1, handler);
        rc = dns_query(&url2, handler);
        rc = dns_query(&url3, handler);
        rc = dns_query(&url4, handler);
        rc = dns_query(&url5, handler);
        rc = dns_query(&url6, handler);
        rc = dns_query(&url7, handler);
        rc = dns_query(&url8, handler);
    //}

    i = 0;
    while (1) {
        LOG_INFO("-----round[%d]", i);
        event_process(1000);

        event_expire_timers();
        if (i++ > 20) {
            break;
        }
    }
    rc = dns_query(&url1, handler);
    rc = dns_query(&url2, handler);
    rc = dns_query(&url3, handler);
    rc = dns_query(&url4, handler);
    rc = dns_query(&url5, handler);
    rc = dns_query(&url6, handler);
    rc = dns_query(&url7, handler);
    rc = dns_query(&url8, handler);

    return 0;
}
