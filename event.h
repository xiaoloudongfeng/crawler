#ifndef _EVENT_H_INCLUDED_
#define _EVENT_H_INCLUDED_

#include "core.h"

#define READ_EVENT      (EPOLLIN|EPOLLRDHUP)
#define WRITE_EVENT     EPOLLOUT
#define CLEAR_EVENT     EPOLLET
#define CLOSE_EVENT     1

struct event_s {
    void               *data;
    unsigned            wirte:1;
    unsigned            active:1;       // 已经加入事件循环

    unsigned            ready:1;

    rbtree_node_t       timer;
    unsigned            timedout:1;     // 已经超时
    unsigned            timer_set:1;    // 已经加入定时器

    /* the pending eof reported by epoll */
    unsigned            pending_eof:1;

    unsigned            eof:1;

    unsigned            error:1;
    
    event_handler_pt    handler;        // 事件处理函数
}; 

int event_init(int num);
int event_add(event_t *ev, int32_t event, uint32_t flags);
int event_del(event_t *ev, int32_t event, uint32_t flags);
int event_process(int timeout);

#define event_del_timer internal_event_del_timer
#define event_add_timer internal_event_add_timer

#include "event_timer.h"

#endif

