#include "core.h"
#include "html_parser.h"
#include <gumbo.h>
#include <http_parser.h>

http_url_t *create_http_url(char *schema, char *host, char *path);
void insert_queue(http_url_t *url);

static void search_for_links(GumboNode *node)
{
	GumboAttribute		   *href;
	int						i;
	int						rc;
	struct http_parser_url	u;
	http_url_t			   *url;

	char					httpurl[1024];
	char					schemahttp[1024];
	char					host[1024];
	char					path[1024];

	if (node->type != GUMBO_NODE_ELEMENT) {
		return;
	}

	memset(&u, 0, sizeof(u));
	memset(httpurl, 0, sizeof(httpurl));
	memset(schemahttp, 0, sizeof(schemahttp));
	memset(host, 0, sizeof(host));
	memset(path, 0, sizeof(path));
	href = NULL;

	if ((node->v.element.tag == GUMBO_TAG_A && (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) ||
		(node->v.element.tag == GUMBO_TAG_INPUT && (href = gumbo_get_attribute(&node->v.element.attributes, "src"))))
	{
		//LOG_DEBUG("get url href[%s]", href->value);

		if (strstr(href->value, ".jpg") || strstr(href->value, ".jpeg") ||
			strstr(href->value, "thread0806") || strstr(href->value, "htm_data"))
		{
			if (strstr(href->value, "http")) {
				strcpy(httpurl, href->value);

			} else {
				strcpy(httpurl, "http://cl.gtta.pw/");
				if (strncmp("../../../", href->value, 9)) {
					strcat(httpurl, href->value);
				
				} else {
					strcat(httpurl, href->value + 9);
				}
			}

			if (!bloom_check(httpurl)) {
				LOG_INFO("httpurl[%s]", httpurl);

				bloom_set(httpurl);
				rc = http_parser_parse_url(httpurl, strlen(httpurl), 0, &u);
				if (rc != 0) {
					LOG_WARN("http_parser_parse_url() failed");

				} else {
					if ((u.field_set & (1 << UF_HOST))) {
						strncpy(schemahttp, httpurl + u.field_data[UF_SCHEMA].off, u.field_data[UF_SCHEMA].len);
						strncpy(host, httpurl + u.field_data[UF_HOST].off, u.field_data[UF_HOST].len);
						strncpy(path, httpurl + u.field_data[UF_PATH].off, u.field_data[UF_PATH].len);

						url = create_http_url(schemahttp, host, path);
						if (url == NULL) {
							LOG_ERROR("create_http_url() failed");
						}

						insert_queue(url);

					} else {
						LOG_WARN("host_name don't exist");
					}
				}
			}
		}
	}

	GumboVector *children = &node->v.element.children;

	for (i = 0; i < children->length; i++) {
		search_for_links((GumboNode *)children->data[i]);
	}
}

int parse_html(http_client_t *client)
{
    GumboOutput *output;

	output = gumbo_parse((const char *)client->html->html->start);
	
	search_for_links(output->root);

	gumbo_destroy_output(&kGumboDefaultOptions, output);

	client->status = HTTP_PARSE_DONE;

	return 0;
}
