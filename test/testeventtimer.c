#include "core.h"
#include "event.h"

void event_handler(event_t *ev)
{
    printf("test\n");
}

int main(void)
{
    int             i = 0;
    connection_t    conn;

    event_timer_init();

    event_t ev1 = {};
    ev1.handler = event_handler;
    ev1.data = &conn;
    event_add_timer(&ev1, 2000);
    
    event_add_timer(&ev1, 2000);
    
    event_t ev2 = {};
    ev2.handler = event_handler;
    ev2.data = &conn;
    event_add_timer(&ev2, 3000);

    sleep(5);
    event_expire_timers();

    return 0;
}
