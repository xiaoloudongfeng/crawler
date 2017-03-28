#include "core.h"
#include "http_client.h"

static queue_t url_q;

void insert_queue(http_url_t *url)
{
	queue_insert_tail(&url_q, &url->queue);
}

static void create_http_client(http_url_t *url, struct in_addr *sin_addr)
{
	http_client_create(url, sin_addr, parse_html);
}

int main(void)
{
	int			 rc, i;

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
    char *host = "bbs.dgtle.com";
    char *path = "/forum.php?mod=forumdisplay&fid=2";
	
    http_url_t *url = create_http_url(schemahttp, host, path);
	rc = dns_query(url, create_http_client);
	
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
				
				dns_query(url, create_http_client);
			}
		}
	}
	
    return 0;
}
