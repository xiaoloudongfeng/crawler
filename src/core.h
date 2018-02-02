#ifndef _CORE_H_INCLUDED_
#define _CORE_H_INCLUDED_

#include "linux_config.h"

typedef struct event_s      event_t;
typedef struct connection_s connection_t;

typedef void (*event_handler_pt)(event_t *ev);
typedef void (*connection_handler_pt)(connection_t *c);

#include "simple_log.h"
#include "rbtree.h"
#include "queue.h"
#include "connection.h"
#include "event.h"
#include "async_dns.h"
#include "http_client.h"
#include "html_parser.h"
#include "bloom_filter.h"
#include "redis_func.h"
#include "process_cycle.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#endif
