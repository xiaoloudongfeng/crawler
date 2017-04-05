#ifndef _BLOOM_FILTER_H_INCLUDED_
#define _BLOOM_FILTER_H_INCLUDED_

int bloom_init(void);
int bloom_set(const char *str);
int bloom_check(const char *str);

#endif
