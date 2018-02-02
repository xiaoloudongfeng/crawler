#include "core.h"
#include "event.h"

rbtree_t                event_timer_rbtree;
static rbtree_node_t    event_timer_sentinel;

int event_timer_init(void)
{
    rbtree_init(&event_timer_rbtree, &event_timer_sentinel, 
                rbtree_insert_timer_value);

    return 0;
}

uint32_t event_find_timer(void)
{
    int32_t         timer;
    rbtree_node_t  *node, *root, *sentinel;
    struct timeval  tv;
    uint32_t        current_msec;

    if (event_timer_rbtree.root == &event_timer_sentinel) {
        return TIMER_INFINITE;
    }

    root = event_timer_rbtree.root;
    sentinel = event_timer_rbtree.sentinel;

    node = rbtree_min(root, sentinel);

    gettimeofday(&tv, NULL);
    current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
    timer = (int32_t) (node->key - current_msec);

    return (uint32_t) (timer > 0 ? timer : 0);
}

void event_expire_timers(void)
{
    event_t        *ev;
    rbtree_node_t  *node, *root, *sentinel;
    struct timeval  tv;
    uint32_t        current_msec;

    sentinel = event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = rbtree_min(root, sentinel);

        gettimeofday(&tv, NULL);
        current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;
        if ((int32_t) (node->key - current_msec) > 0) {
            return;
        }

        ev = (event_t *) ((char *) node - offsetof(event_t, timer));
        
        LOG_DEBUG("event timer del: %u: %u", ((connection_t *)(ev->data))->fd, 
                ev->timer.key);

        rbtree_delete(&event_timer_rbtree, &ev->timer);

        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;

        ev->timer_set = 0;

        ev->timedout = 1;

        ev->handler(ev);
    }
}

void event_cancel_timers(void)
{
    event_t        *ev;
    rbtree_node_t  *node, *root, *sentinel;

    sentinel = event_timer_rbtree.sentinel;

    for ( ;; ) {
        root = event_timer_rbtree.root;

        if (root == sentinel) {
            return;
        }

        node = rbtree_min(root, sentinel);
        ev = (event_t *) ((char *)node - offsetof(event_t, timer));
        
        LOG_DEBUG("event timer cancel: %u: %u", ((connection_t *)(ev->data))->fd, 
                ev->timer.key);

        rbtree_delete(&event_timer_rbtree, &ev->timer);

        ev->timer.left = NULL;
        ev->timer.right = NULL;
        ev->timer.parent = NULL;

        ev->timer_set = 0;

        ev->handler(ev);
    }
}

