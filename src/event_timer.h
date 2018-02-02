#ifndef _EVENT_TIMER_H_INCLUDED_
#define _EVENT_TIMER_H_INCLUDED_

#include "core.h"
#include "event.h"

#define TIMER_INFINITE      (uint32_t) -1
#define TIMER_LAZY_DELAY    300

int event_timer_init(void);
uint32_t event_fine_timer(void);
void event_expire_timers(void);
void event_cancel_timers(void);

extern rbtree_t event_timer_rbtree;

static inline void internal_event_del_timer(event_t *ev)
{
    LOG_DEBUG("event timer del: %u: %u",
            ((connection_t *) (ev->data))->fd, ev->timer.key);
    
    rbtree_delete(&event_timer_rbtree, &ev->timer);

    ev->timer.left = NULL;
    ev->timer.right = NULL;
    ev->timer.parent = NULL;

    ev->timer_set = 0;
}

static inline void internal_event_add_timer(event_t *ev, uint32_t timer)
{
    uint32_t        key;
    int32_t         diff;
    struct timeval  tv;

    gettimeofday(&tv, NULL);

    key = tv.tv_sec * 1000 + tv.tv_usec / 1000 + timer;

    if (ev->timer_set) {
        diff = (int32_t) (key - ev->timer.key);

        if (abs(diff) < TIMER_LAZY_DELAY) {
            LOG_DEBUG("event timer: %u, old: %u, new: %u", 
                    ((connection_t *) (ev->data))->fd, ev->timer.key, key);
            return;
        }

        event_del_timer(ev);
    }

    ev->timer.key = key;
    LOG_DEBUG("event timer add: %u: %u:%u",
            ((connection_t *) (ev->data))->fd, timer, key);

    rbtree_insert(&event_timer_rbtree, &ev->timer);

    ev->timer_set = 1;
}

#endif

