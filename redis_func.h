#ifndef _REDIS_FUNC_H_INCLUDED_
#define _REDIS_FUNC_H_INCLUDED_

int redis_init(const char *hostname, int port);

void redis_free(void);

int redis_list_length(const char *key);

// 从右push
int redis_list_rpush(const char *key, const char *value);

// 从左pop
char *redis_list_lpop(const char *key);

int redis_string_setbit(const char *str, unsigned off, unsigned int value);

int redis_string_getbit(const char *str, unsigned off, unsigned int *value);

int redis_string_bitop(const char *src1, const char *src2, const char *op, const char *dest);

#endif

