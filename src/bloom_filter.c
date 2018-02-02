#include "core.h"
#include "murmur3.h"

#define SEED_NUM    9
#define BIT_SIZE    4294967295L

// 从大素数表里面随便调了9个
unsigned int    seeds[SEED_NUM] = { 
                                    1999, 2887, 3989, 4969, 
                                    5557, 6661, 7883, 8887, 
                                    9901
                                  };

int bloom_init(void)
{
    if (redis_string_setbit("bloom_filter", BIT_SIZE, 0) < 0) {
        LOG_ERROR("redis_string_setbit() failed");
        return -1;
    }

    if (redis_string_bitop("bloom_filter", "bloom_filter", "XOR", "bloom_filter") < 0) {
        LOG_ERROR("redis_string_setbit() failed");
        return -1;
    }

    return 0;
}

int bloom_set(const char *str)
{
    unsigned int    i = 0;
    unsigned int    off = 0;

    for (i = 0; i < SEED_NUM; i++) {
        MurmurHash3_x86_32(str, strlen(str), seeds[i], (void *)&off);
        //printf("off[%d]: %u\n", i, off);
        
        if (redis_string_setbit("bloom_filter", off, 1) < 0) {
            LOG_ERROR("redis_string_setbit() failed");
            return -1;
        }
    }

    return 0;
}

int bloom_check(const char *str)
{
    unsigned int i = 0;
    unsigned int off = 0;
    unsigned int value;

    for (i = 0; i < SEED_NUM; i++) {
        MurmurHash3_x86_32(str, strlen(str), seeds[i], (void *)&off);
        //printf("off[%d]: %u\n", i, off);
     
        if (redis_string_getbit("bloom_filter", off, &value) < 0) {
            LOG_ERROR("redis_string_getbit() failed");
            return -1;
        }

        // printf("reply->integer: %d\n", reply->integer);
        // 检查到有1位未置1，说明该字符串未被bloom_set()
        if (!value) {
            return 0;
        }
    }

    // 全中，说明该字符串被bloom_set()
    return 1;
}

/*
int bloom_free(void)
{
    if (ctx) {
        redisFree(ctx);
    }
    ctx = NULL;

    return 0;
}
*/

/*
int main(void)
{
    int ret = 0;

    ret = redis_init("127.0.0.1", 6379);
    printf("redis_init: %d\n", ret);

    ret = bloom_init();
    printf("bloom_init: %d\n", ret);
    
    ret = bloom_check("http://www.baidu.com");
    printf("bloom_check1: %d\n", ret);
    ret = bloom_set("http://www.baidu.com");
    printf("bloom_set: %d\n", ret);
    ret = bloom_check("http://www.baidu.com");
    printf("bloom_check2: %d\n", ret);

    return 0;
}
*/

