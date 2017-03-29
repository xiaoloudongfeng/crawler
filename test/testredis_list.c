#include "core.h"
#include "redis_list.h"

int main(void)
{
    if (redis_init() < 0) {
        LOG_ERROR("redis_init() failed");

        return -1;
    }

    if (redis_list_rpush("url_list_0", "www.baidu.com") < 0) {
        LOG_ERROR("redis_list_rpush() failed");

        return -1;
    }

    if (redis_list_length("url_list_0") < 0) {
        LOG_ERROR("redis_list_length() failed");

        return -1;
    }

/*
    char *result = redis_list_lpop("url_list_0");
    if (!result) {
        LOG_ERROR("redis_list_lpop() failed");

        return -1;
    }
*/

    redis_free();

    return 0;
}
