#ifndef __BLOOM_FILTER_H__
#define __BLOOM_FILTER_H__

int bloom_init(void);
int bloom_set(const char *str);
int bloom_check(const char *str);
int bloom_free();

#endif
