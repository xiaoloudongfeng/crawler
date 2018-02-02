#include "linux_config.h"

#ifndef _QUEUE_H_INCLUDED_
#define _QUEUE_H_INCLUDED_

typedef struct queue_s queue_t;

struct queue_s {
    queue_t *prev;
    queue_t *next;
};

#define queue_init(q)                                                       \
    (q)->prev = q;                                                          \
    (q)->next = q

#define queue_empty(h)                                                      \
    (h == (h)->prev)

#define queue_insert_head(h, x)                                             \
    (x)->next = (h)->next;                                                  \
    (x)->next->prev = x;                                                    \
    (x)->prev = h;                                                          \
    (h)->next = x

#define queue_insert_tail(h, x)                                             \
    (x)->prev = (h)->prev;                                                  \
    (x)->prev->next = x;                                                    \
    (x)->next = h;                                                          \
    (h)->prev = x

#define queue_head(h)                                                       \
    (h)->next

#define queue_last(h)                                                       \
    (h)->prev

#define queue_next(q)                                                       \
    (q)->next

#define queue_remove(x)                                                     \
    (x)->next->prev = (x)->prev;                                            \
    (x)->prev->next = (x)->next

// h 原队列头
// q 队列中间的某元素
// n 新队列头
// 效果
// 原: h -> element1 -> element2 -> q -> elementn -> h
// 后: 1、h -> element1 -> element2 -> h
//     2、n -> q -> elementn -> n
#define queue_split(h, q, n)                                                \
    (n)->prev = (h)->prev;                                                  \
    (n)->prev->next = n;                                                    \
    (n)->next = q;                                                          \
    (h)->prev = (q)->prev;                                                  \
    (h)->prev->next = h;                                                    \
    (q)->prev = n

#define queue_add(h, n)                                                     \
    (h)->prev->next = (n)->next;                                            \
    (n)->next->prev = (h)->prev;                                            \
    (h)->prev = (n)->prev;                                                  \
    (n)->prev->next = h

#define queue_data(q, type, link)                                           \
    (type *) ((u_char *)q - offsetof(type, link))

#endif
