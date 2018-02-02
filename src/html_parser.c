#include "core.h"
#include <gumbo.h>

#include "murmur3.h"
#include "process_cycle.h"

void insert_queue(http_url_t *url);

static void search_for_links(GumboNode *node)
{
    GumboAttribute *href;
    int             i;
    char            httpurl[1024];
    char            key[128];
    unsigned        id;

    if (node->type != GUMBO_NODE_ELEMENT) {
        return;
    }

    memset(httpurl, 0, sizeof(httpurl));
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

                // 计算url的hash值，并对work_processes取模，将url散列到不同的队列中
                MurmurHash3_x86_32(httpurl, strlen(httpurl), HASH_SEED, (void *)&id);
                id %= work_processes;

                // 根据id得到list的key
                memset(key, 0, sizeof(key));
                sprintf(key, "url_list_%d", id);

                // 插入Redis队列中
                LOG_DEBUG("rpush %s to %s", httpurl, key);
                if (redis_list_rpush(key, httpurl) < 0) {
                    LOG_ERROR("redis_list_rpush() failed");

                    return;
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

                        if (a_node && a_node->type == GUMBO_NODE_TEXT) {
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

                    if (a_node && a_node->type == GUMBO_NODE_TEXT) {
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
                    
                    if (a_node && a_node->type == GUMBO_NODE_TEXT) {
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
                    
                if (a_node && a_node->type == GUMBO_NODE_TEXT) {
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
    char        httpurl[1024] = {};
    GumboOutput *output;
    
    int         rc = -1;

    if (client->status != HTTP_HTML_DONE) {
        LOG_ERROR("html[%s://%s%s] failed", client->url->schema, client->url->host, client->url->path);
        goto failed;
    }

    output = gumbo_parse((const char *)client->html->html->start);
    if (output == NULL) {
        LOG_ERROR("gumbo_parse() failed");
        goto failed;
    }

    if (strstr(client->url->path, "forum.php?mod=forumdisplay&fid=2")) {
        search_for_links(output->root);

    } else if (strstr(client->url->path, "thread-")) {
        search_for_author(output->root, title, category, author, timestamp);
        LOG_INFO("title[%s], category[%s], author[%s], timestamp[%s]", title, category, author, timestamp);
    }

    gumbo_destroy_output(&kGumboDefaultOptions, output);

    rc = 0;
    client->status = HTTP_PARSE_DONE;
    
    // 解析完成，将url推入parse_done队列中
    sprintf(httpurl, "%s://%s%s", client->url->schema, client->url->host, client->url->path);
    if (redis_list_rpush("parse_done", httpurl) < 0) {
        LOG_ERROR("redis_list_rpush() failed");
    }

failed:
    http_client_retrieve(client);
    return rc;
}

