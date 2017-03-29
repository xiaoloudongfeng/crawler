#ifndef _REDIS_LIST_H_INCLUDED
#define _REDIS_LIST_H_INCLUDED

int redis_init(void);

int redis_list_length(const char *key);

// 从右push
int redis_list_rpush(const char *key, const char *value);

// 从左pop
char *redis_list_lpop(const char *key);

void redis_free(void);

#endif

