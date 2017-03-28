#ifndef _SIMPLE_LOG_H_INCLUDED_
#define _SIMPLE_LOG_H_INCLUDED_

#include <string.h>

#define ESC_START       "\033["
#define ESC_END         "\033[0m"
#define LN              "\n"

#define COLOR_FATAL     "31;40;1m"
#define COLOR_ALERT     "31;40;1m"
#define COLOR_CRIT      "31;40;1m"
#define COLOR_ERROR     "31;40;1m"
#define COLOR_WARN      "33;40;1m"
#define COLOR_NOTICE    "34;40;1m"
#define COLOR_INFO      "32;40;1m"
#define COLOR_DEBUG     "36;40;1m"
#define COLOR_TRACE     "37;40;1m"

char time_buf[64];

static inline void time_stamp(void)
{
    time_t      timep;
    struct tm  *p;

    time(&timep);
    p = localtime(&timep);
    memset(time_buf, 0, sizeof(time_stamp));

    sprintf(time_buf, "[%04d/%02d/%02d %02d:%02d:%02d]", 
        p->tm_year + 1900, p->tm_mon, p->tm_mday, p->tm_hour, p->tm_min, p->tm_sec);
}

#define LOG_FATAL(fmt, args...)                                                             \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_FATAL "%s " fmt ESC_END LN, time_buf, ##args);      \
    } while (0)

#define LOG_ALERT(fmt, args...)                                                             \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_ALERT "%s " fmt ESC_END LN, time_buf, ##args);      \
    } while (0)

#define LOG_CRIT(fmt, args...)                                                              \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_CRIT "%s " fmt ESC_END LN, time_buf, ##args);       \
    } while (0)

#define LOG_ERROR(fmt, args...)                                                             \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_ERROR "%s " fmt ESC_END LN, time_buf, ##args);      \
    } while (0)

#define LOG_WARN(fmt, args...)                                                              \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_WARN "%s " fmt ESC_END LN, time_buf, ##args);       \
    } while (0)

#define LOG_NOTICE(fmt, args...)                                                            \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_NOTICE "%s " fmt ESC_END LN, time_buf, ##args);     \
    } while (0)

#define LOG_INFO(fmt, args...)                                                              \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_INFO "%s " fmt ESC_END LN, time_buf, ##args);       \
    } while (0)

#define LOG_DEBUG(fmt, args...)                                                             \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_DEBUG "%s " fmt ESC_END LN, time_buf, ##args);      \
    } while (0)

#define LOG_TRACE(fmt, args...)                                                             \
    do {                                                                                    \
        time_stamp();                                                                       \
        fprintf(stderr, ESC_START COLOR_TRACE "%s " fmt ESC_END LN, time_buf, ##args);      \
    } while (0)

/*
int log_init(void)
{
    return 0;
}
*/

#endif

