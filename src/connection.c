#include "core.h"

static connection_t   *connections;
static event_t        *read_events;
static event_t        *write_events;

static connection_t    *free_connections;
static uint32_t         free_connection_n;

uint32_t get_free_connection_n(void)
{
    return free_connection_n;
}

int connection_init(uint32_t n)
{
    connection_t   *c, *next;
    int             i;

    connections = calloc(n, sizeof(connection_t));
    if (connections == NULL) {
        LOG_ERROR("calloc() failed");
        return -1;
    }

    c = connections;
    write_events = calloc(n, sizeof(event_t));
    if (write_events == NULL) {
        LOG_ERROR("calloc() failed");
        return -1;
    }

    read_events = calloc(n, sizeof(event_t));
    if (read_events == NULL) {
        LOG_ERROR("calloc() failed");
        return -1;
    }

    i = n;
    next = NULL;
    
    do {
        i--;

        c[i].data = next;
        c[i].read = &read_events[i];
        c[i].write = &write_events[i];
        c[i].fd = -1;

        next = &c[i];
    } while (i);
    
    free_connections = next;
    free_connection_n = n;

    return 0;
}

connection_t *get_connection(int s)
{
    connection_t   *c;
    event_t        *rev, *wev;

    c = free_connections;

    if (c == NULL) {
        return NULL;
    }

    free_connections = c->data;
    free_connection_n--;

    rev = c->read;
    wev = c->write;

    memset(c, 0, sizeof(connection_t));
    memset(rev, 0, sizeof(event_t));
    memset(wev, 0, sizeof(event_t));

    c->read = rev;
    c->write = wev;
    c->fd = s;

    rev->data = c;
    wev->data = c;

    return c;
}

void free_connection(connection_t *c)
{
    c->data = free_connections;
    free_connections = c;
    free_connection_n++;
}

void close_connection(connection_t *c)
{
    int fd;

    if (c->fd == -1) {
        LOG_ERROR("connection already closed");
        return;
    }

    if (c->read->active) {
        event_del(c->read, READ_EVENT, CLOSE_EVENT);
    }

    if (c->write->active) {
        event_del(c->write, READ_EVENT, CLOSE_EVENT);
    }

    // 如果读写事件已经被加入定时器, 需要移除
    // 关闭fd, epoll事件循环就自动将其移除, 所以不需要移除读、写事件
    if (c->read->timer_set) {
        event_del_timer(c->read);
    }

    if (c->write->timer_set) {
        event_del_timer(c->write);
    }

    free_connection(c);

    fd = c->fd;
    c->fd = -1;

    close(fd);
}

/*
 * ret:
 *      > 0         recv n bytes
 *      = 0         recv 0 bytes maybe not ready?
 *      -EAGAIN     fd not ready
 *      -1          fd error
 */
ssize_t unix_recv(connection_t *c, u_char *buf, size_t size)
{
    ssize_t     n;
    int         err;
    event_t    *rev;

    rev = c->read;

    do {
        n = recv(c->fd, buf, size, 0);

        LOG_DEBUG("recv: fd:%d %zd of %zu", c->fd, n, size);

        if (n == 0) {
            rev->ready = 0;
            rev->eof = 1;
            return n;

        } else if (n > 0) {
            if ((size_t) n < size) {
                rev->ready = 0;
            }

            return n;
        }
        
        err = errno;
        
        if (err == EAGAIN || err == EINTR) {
            LOG_DEBUG("recv() not ready[%s]", strerror(err));
            n = -EAGAIN;

        } else {
            LOG_DEBUG("recv() failed[%s]", strerror(err));
            n = -1;
            break;
        }

    } while (err == EINTR);

    rev->ready = 0;

    if (n == -1) {
        rev->error = 1;
    }

    return n;
}

/*
 * ret:
 *      > 0         sent n bytes
 *      = 0         sent 0 bytes maybe not ready?
 *      -EAGAIN     fd not ready
 *      -1          fd error
 */
ssize_t unix_send(connection_t *c, u_char *buf, size_t size)
{
    ssize_t     n;
    int         err;
    event_t    *wev;

    wev = c->write;

    for ( ;; ) {
        n = send(c->fd, buf, size, 0);

        LOG_DEBUG("send: fd:%d %zd of %zu", c->fd, n, size);

        if (n > 0) {
            if (n < (ssize_t) size) {
                wev->ready = 0;
            }

            c->sent += n;

            return n;
        }

        err = errno;

        if (n == 0) {
            LOG_ERROR("send() returned zero");
            wev->ready = 0;
            return n;
        }

        if (err == EAGAIN || err == EINTR) {
            wev->ready = 0;

            LOG_DEBUG("send() not ready: [%s]", strerror(err));

            if (err == EAGAIN) {
                return -EAGAIN;
            }

        } else {
            wev->error = 1;
            LOG_ERROR("send() failed: [%s]", strerror(err));
            return -1;
        }
    }
}

/*
 * ret:
 *      > 0         sent n bytes
 *      = 0         sent 0 bytes maybe not ready?
 *      -EAGAIN     fd not ready
 *      -1          fd error
 */
ssize_t unix_udp_recv(connection_t *c, u_char *buf, size_t size)
{
    ssize_t     n;
    event_t    *rev;
    int         err;

    rev = c->read;

    do {
        n = recv(c->fd, buf, size, 0);

        LOG_DEBUG("recv: fd:%d %zd of %zu", c->fd, n, size);

        if (n >= 0) {
            return n;
        }

        err = errno;

        if (err == EAGAIN || err == EINTR) {
            LOG_DEBUG("recv() not ready");
            n = -EAGAIN;
        
        } else {
            LOG_ERROR("recv() failed: [%s]", strerror(err));
            n = -1;
            break;
        }

    } while (err == EINTR);

    rev->ready = 0;

    if (n == -1) {
        rev->error = 1;
    }

    return n;
}

static void ssl_write_handler(event_t *wev)
{
    connection_t *c;

    c = wev->data;

    c->read->handler(c->read);
}

static int ssl_handle_recv(connection_t *c, int n)
{
    int sslerr, err;
    
    if (n > 0) {
        if (c->ssl->saved_write_handler) {
            c->write->handler = c->ssl->saved_write_handler;
            c->ssl->saved_write_handler = NULL;
            c->write->ready = 1;
        }

        return 0;
    }

    sslerr = SSL_get_error(c->ssl->connection, n);

    err = (sslerr == SSL_ERROR_SYSCALL) ? errno : 0;

    LOG_DEBUG("SSL_get_error: %d", sslerr);

    if (sslerr == SSL_ERROR_WANT_READ) {
        c->read->ready = 0;
        return -EAGAIN;
    }

    if (sslerr == SSL_ERROR_WANT_WRITE) {
        LOG_ERROR("peer started SSL renegotiation");

        c->write->ready = 0;

        if (!c->write->active) {
            if (event_add(c->write, WRITE_EVENT, CLEAR_EVENT) < 0) {
                return -1;
            }
        }

        if (c->ssl->saved_write_handler == NULL) {
            c->ssl->saved_write_handler = c->write->handler;
            c->write->handler = ssl_write_handler;
        }

        return -EAGAIN;
    }

    if (sslerr == SSL_ERROR_ZERO_RETURN || ERR_peek_error() == 0) {
        LOG_DEBUG("peer shutdown SSL cleanly");

        return -4;
    }

    LOG_ERROR("SSL_read() failed: err[%d], sslerr[%d]", err, sslerr);

    return -1;
}

void ssl_error(void)
{
    int         flags;
    u_long      n;
    char       *p, *last;
    const char *data;
    char        errstr[1024];

    last = errstr + 1024;

    strcpy(errstr, "ignoring stale global SSL error");

    p = errstr + strlen(errstr);

    for ( ;; ) {
        n = ERR_peek_error_line_data(NULL, NULL, &data, &flags);

        if (n == 0) {
            break;
        }

        if (p >= last) {
            goto next;
        }

        *p++ = ' ';

        ERR_error_string_n(n, p, last - p);

        while (p < last && *p) {
            p++;
        }

        if (p < last && *data && (flags & ERR_TXT_STRING)) {
            *p++ = ':';
            strncat(errstr, data, last - p);
            p = errstr + strlen(errstr);
        }

    next:
        (void) ERR_get_error();
    }

    LOG_ERROR("%s", errstr);
}

ssize_t ssl_recv(connection_t *c, u_char *buf, size_t size)
{
    int n, bytes;

    if (c->ssl->last == -1) {
        c->read->error = 1;
        return -1;
    }

    if (c->ssl->last == -4) {
        c->read->ready = 0;
        c->read->eof = 1;
        return 0;
    }

    bytes = 0;
    while (ERR_peek_error()) {
        // LOG_ERROR("ignoring stale global SSL error");
        ssl_error();
    }

    ERR_clear_error();

    for ( ;; ) {

        n = SSL_read(c->ssl->connection, buf, size);
        
        LOG_DEBUG("SSL_read: %d of %zu", n, size);

        if (n > 0) {
            bytes += n;
        }

        c->ssl->last = ssl_handle_recv(c, n);

        if (c->ssl->last == 0) {
            size -= n;
            
            if (size == 0) {
                c->read->ready = 1;
                return bytes;
            }

            buf += n;

            continue;
        }

        if (bytes) {
            if (c->ssl->last != -EAGAIN) {
                c->read->ready = 1;
            }
            
            return bytes;
        }

        switch (c->ssl->last) {
        
        case -4:
            c->read->ready = 0;
            c->read->eof = 1;
            return 0;
        
        case -1:
            c->read->error = 1;

        case -EAGAIN:
            return c->ssl->last;
        }
    }
}

static void ssl_read_handler(event_t *rev)
{
    connection_t *c;

    c = rev->data;

    c->write->handler(c->write);
}

ssize_t ssl_send(connection_t *c, u_char *buf, size_t size)
{
    int n, sslerr, err;

    while (ERR_peek_error()) {
        // LOG_ERROR("ignoring stale global SSL error");
        ssl_error();
    }

    ERR_clear_error();

    LOG_DEBUG("SSL to write: %zd", size);

    n = SSL_write(c->ssl->connection, buf, size);

    LOG_DEBUG("SSL_write: %d", n);

    if (n > 0) {
        if (c->ssl->saved_read_handler) {
            c->read->handler = c->ssl->saved_read_handler;
            c->ssl->saved_read_handler = NULL;
            c->read->ready = 1;
        }

        c->sent += n;

        return n;
    }

    sslerr = SSL_get_error(c->ssl->connection, n);
    
    err = (sslerr == SSL_ERROR_SYSCALL) ? errno : 0;

    LOG_DEBUG("SSL_get_error: %d", sslerr);

    if (sslerr == SSL_ERROR_WANT_WRITE) {
        c->write->ready = 0;
        return -EAGAIN;
    }

    if (sslerr == SSL_ERROR_WANT_READ) {
        LOG_ERROR("peer started SSL renegotiation");

        c->read->ready = 0;

        if (!c->read->active) {
            if (event_add(c->read, READ_EVENT, CLEAR_EVENT) < 0) {
                return -1;
            }
        }

        if (c->ssl->saved_read_handler == NULL) {
            c->ssl->saved_read_handler = c->read->handler;
            c->read->handler = ssl_read_handler;
        }

        return -EAGAIN;
    }

    c->write->error = 1;
    
    LOG_ERROR("SSL_write() failed: err[%d], sslerr[%d]", err, sslerr);

    return -1;
}

