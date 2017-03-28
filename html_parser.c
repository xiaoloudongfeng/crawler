#include "core.h"
#include "html_parser.h"
#include <gumbo.h>
#include <http_parser.h>

void insert_queue(http_url_t *url);

static void search_for_links(GumboNode *node)
{
    GumboAttribute         *href;
    int                     i;
    int                     rc;
    struct http_parser_url  u;
    http_url_t             *url;

    char                    httpurl[1024];
    char                    schemahttp[1024];
    char                    host[1024];
    char                    path[1024];

    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    memset(&u, 0, sizeof(u));
    memset(httpurl, 0, sizeof(httpurl));
    memset(schemahttp, 0, sizeof(schemahttp));
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));
    href = NULL;

    if (node->v.element.tag == GUMBO_TAG_A && 
        (href = gumbo_get_attribute(&node->v.element.attributes, "href"))) 
    {
        //LOG_DEBUG("get url href[%s]", href->value);

        if (strstr(href->value, "http://bbs.dgtle.com/thread-") || 
            strstr(href->value, "http://bbs.dgtle.com/forum.php?mod=forumdisplay&fid=2&page=")) 
        {
            strcpy(httpurl, href->value);

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
                        strcpy(path, httpurl + u.field_data[UF_PATH].off);

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

static void search_for_author(GumboNode *node, char *title, char *category, char *author, char *timestamp)
{
    GumboNode       *parent;
    GumboAttribute  *class;
    GumboVector     *children;
    int             i;

    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    if (node->v.element.tag == GUMBO_TAG_B) {
        parent = node->parent;

        if (parent && parent->v.element.tag == GUMBO_TAG_FONT) {
            parent = parent->parent;

            if (parent && parent->v.element.tag == GUMBO_TAG_A) {
                parent = parent->parent;

                if (parent && parent->v.element.tag == GUMBO_TAG_SPAN) {
                    class = gumbo_get_attribute(&parent->v.element.attributes, "class");

                    if (class && !strcmp(class->value, "cr_date")) {
                        GumboNode *a_node = node->v.element.children.data[0];

                        if (a_node->type == GUMBO_NODE_TEXT) {
                            // LOG_INFO("%s", a_node->v.text.text);
                            strcpy(category, a_node->v.text.text);
                        }
                    }
                }
            }
        }
    }

    if (node->v.element.tag == GUMBO_TAG_P) {
        parent = node->parent;

        if (parent && parent->v.element.tag == GUMBO_TAG_A) {
            parent = parent->parent;
            
            if (parent && parent->v.element.tag == GUMBO_TAG_H1) {
                class = gumbo_get_attribute(&parent->v.element.attributes, "class");

                if (class && !strcmp(class->value, "cr_h1title")) {
                    GumboNode *a_node = node->v.element.children.data[0];

                    if (a_node->type == GUMBO_NODE_TEXT) {
                        // LOG_INFO("%s", a_node->v.text.text);
                        strcpy(title, a_node->v.text.text);
                    }
                }
            }
        }
    }
    
    if (node->v.element.tag == GUMBO_TAG_A) {
        parent = node->parent;
        
        if (parent && parent->v.element.tag == GUMBO_TAG_P) {
            parent = parent->parent;
            
            if (parent && parent->v.element.tag == GUMBO_TAG_DIV) {
                class = gumbo_get_attribute(&parent->v.element.attributes, "class");
                
                if (class && !strcmp(class->value, "mporf")) {
                    // LOG_INFO("****************found mporf");
                    
                    GumboNode *a_node = node->v.element.children.data[0];
                    
                    if (a_node->type == GUMBO_NODE_TEXT) {
                        // LOG_INFO("%s", a_node->v.text.text);
                        strcpy(author, a_node->v.text.text);
                    }
                }
            }
        }
    }
                
    if (node->v.element.tag == GUMBO_TAG_EM) {
        parent = node->parent;
        
        if (parent && parent->v.element.tag == GUMBO_TAG_SPAN) {
            class = gumbo_get_attribute(&parent->v.element.attributes, "class");
                
            if (class && !strcmp(class->value, "cr_date")) {
                // LOG_INFO("****************found cr_date");
                    
                GumboNode *a_node = node->v.element.children.data[0];
                    
                if (a_node->type == GUMBO_NODE_TEXT) {
                    if (strchr(a_node->v.text.text, ':')) {
                        // LOG_INFO("%s", a_node->v.text.text);
                        strcpy(timestamp, a_node->v.text.text);
                    }
                }
            }
        }
    }

    children = &node->v.element.children;
            
    for (i = 0; i < children->length; i++) {
        search_for_author((GumboNode *)children->data[i], title, category, author, timestamp);
    }
}

int parse_html(http_client_t *client)
{
    char        title[1024] = {};
    char        category[1024] = {};
    char        author[1024] = {};
    char        timestamp[1024] = {};
    GumboOutput *output;

    output = gumbo_parse((const char *)client->html->html->start);

    if (strstr(client->url->path, "forum.php?mod=forumdisplay&fid=2")) {
        search_for_links(output->root);

    } else if (strstr(client->url->path, "thread-")) {
        search_for_author(output->root, title, category, author, timestamp);
        LOG_INFO("title[%s], category[%s], author[%s], timestamp[%s]", title, category, author, timestamp);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    client->status = HTTP_PARSE_DONE;

    return 0;
}
