#include "core.h"
#include "event.h"

static int                  ep = -1;
static struct epoll_event  *event_list;
static uint32_t             nevents = 512;

int event_init(int num)
{
    if (ep == -1) {
        ep = epoll_create(num);

        if (ep == -1) {
            LOG_ERROR("epoll_create() failed");

            return -1;
        }
    }

    if (event_list) {
        free(event_list);
    }

    event_list = (struct epoll_event *)calloc(nevents, sizeof(struct epoll_event));
    if (event_list == NULL) {
        return -1;
    }

    return 0;
}

int event_add(event_t *ev, int32_t event, uint32_t flags)
{
    int                 op;
    uint32_t            events, prev;
    event_t            *e;
    connection_t       *c;
    struct epoll_event  ee;

    c = ev->data;

    events = (uint32_t) event;

    if (event == READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;

    } else {
        e = c->read;
        prev = EPOLLIN|EPOLLRDHUP;
    }

    // 如果另一个事件结构体是激活状态，与上该结构体对应的标志位
    // 同时使用EPOLL_CTL_MOD
    if (e->active) {
        op = EPOLL_CTL_MOD;
        events |= prev;

    } else {
        op = EPOLL_CTL_ADD;
    }

    ee.events = events | (uint32_t) flags;
    ee.data.ptr = (void *) c;

    LOG_DEBUG("epoll add event: fd:%d op:%d ev:%08X", c->fd, op, ee.events);

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        LOG_ERROR("epoll_ctl(%d, %d) failed", op, c->fd);
        return -1;
    }

    ev->active = 1;

    return 0;
}

int event_del(event_t *ev, int32_t event, uint32_t flags)
{
    int                 op;
    uint32_t            prev;
    event_t            *e;
    connection_t       *c;
    struct epoll_event  ee;

    c = ev->data;

    if (flags & CLOSE_EVENT) {
        ev->active = 0;
        return 0;
    }

    if (event == READ_EVENT) {
        e = c->write;
        prev = EPOLLOUT;

    } else {
        e = c->read;
        prev = EPOLLIN|EPOLLRDHUP;
    }

    // 如果另一个事件结构体是激活状态，与上该结构体对应的标志位
    // 同时使用EPOLL_CTL_MOD
    if (e->active) {
        op = EPOLL_CTL_MOD;
        ee.events = prev | (uint32_t) flags;
        ee.data.ptr = (void *) c;

    } else {
        op = EPOLL_CTL_DEL;
        ee.events = 0;
        ee.data.ptr = NULL;
    }

    if (epoll_ctl(ep, op, c->fd, &ee) == -1) {
        LOG_ERROR("epoll_ctl(%d, %d) failed", op, c->fd);

        return -1;
    }

    ev->active = 0;

    return 0;
}

int event_process(int timeout)
{
    int             events;
    event_t        *rev, *wev;
    uint32_t        revents;
    connection_t   *c;
    int             i;

    events = epoll_wait(ep, event_list, nevents, timeout);

    if (events < 0) {
        if (errno == EINTR) {
            return 0;
        }
        
        LOG_ERROR("epoll_wait() failed");
        return -1;
    }

    if (events == 0) {
        if (timeout != (uint32_t) -1) {
            return 0;
        }

        LOG_ERROR("epoll_wait() returned no events without timeout");
        return -1;
    }

    for (i = 0; i < events; i++) {

        c = event_list[i].data.ptr;

        if (c->fd == -1) {
            LOG_INFO("epoll: stale event %p", c);
            continue;
        }

        revents = event_list[i].events;
        LOG_DEBUG("epoll: fd:%d ev:%08X d:%p", c->fd, revents, event_list[i].data.ptr);

        if (revents & (EPOLLERR|EPOLLHUP)) {
            LOG_INFO("epoll_wait() error on fd:%d ev:%08X", c->fd, revents);
        }

        if ((revents & (EPOLLERR|EPOLLHUP)) &&
            (revents & (EPOLLIN|EPOLLOUT)) == 0) 
        {
            /*
             * if the error events were returned without EPOLLIN or EPOLLOUT, 
             * then add these flags to handle the events at least in one
             * active handler
             */
            revents |= EPOLLIN|EPOLLOUT;
        }

        rev = c->read;
        if ((revents & EPOLLIN) && rev->active) {
            if (revents & EPOLLRDHUP) {
                rev->pending_eof = 1;
            }

            rev->ready = 1;
            rev->handler(rev);
        }

        wev = c->write;
        if ((revents & EPOLLOUT) && wev->active) {
            if (c->fd == -1) {
                LOG_INFO("epoll: stale event %p", c);
                continue;
            }

            wev->ready = 1;
            wev->handler(wev);
        }
    }

    return 0;
}

