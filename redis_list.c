#include "core.h"

#include <hiredis/hiredis.h>

redisContext   *ctx;

int redis_init(void)
{
    const char     *hostname = "127.0.0.1";
    int             port = 6379;
    struct timeval  timeout = {1, 500000};

    ctx = redisConnectWithTimeout(hostname, port, timeout);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            LOG_ERROR("Connection error: %s\n", ctx->errstr);
            redisFree(ctx);

        } else {
            LOG_ERROR("Connection error: can't allocate redis context\n");
        }

        return -1;
    }

    return 0;
}

int redis_list_length(const char *key)
{
    int         length;
    redisReply *reply;

    reply = redisCommand(ctx, "llen %s", key);
    if (ctx->err) {
        LOG_ERROR("redisCommand failed: %s", ctx->errstr);

        goto failed;
    }

    if (!reply) {
        LOG_ERROR("redisCommand failed[reply is NULL]");

        goto failed;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        LOG_ERROR("redis_list_length, reply->type != REDIS_REPLY_INTEGER");

        goto failed;
    }

    length = reply->integer;

    LOG_DEBUG("length of %s[%d]", key, length);

    return length;

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return -1;
}

// 从右push
int redis_list_rpush(const char *key, const char *value)
{
    redisReply *reply;

    reply = redisCommand(ctx, "rpush %s %s", key, value);
    if (ctx->err) {
        LOG_ERROR("redisCommand failed: %s", ctx->errstr);

        goto failed;
    }

    if (!reply) {
        LOG_ERROR("redisCommand failed[reply is NULL]");

        goto failed;
    }

    freeReplyObject(reply);

    return 0;

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return -1;
}

// 从左pop
char *redis_list_lpop(const char *key)
{
    char       *result;
    redisReply *reply;

    reply = redisCommand(ctx, "lpop %s", key);
    if (ctx->err) {
        LOG_ERROR("redisCommand failed: %s", ctx->errstr);

        goto failed;
    }

    if (!reply) {
        LOG_ERROR("redisCommand failed[reply is NULL]");

        goto failed;
    }

    LOG_DEBUG("%s lpop: %s", key, reply->str);

    result = calloc(reply->len + 1, sizeof(char));
    strcpy(result, reply->str);

    freeReplyObject(reply);

    return result;

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return NULL;
}

void redis_free(void)
{
    if (ctx) {
        redisFree(ctx);
        ctx = NULL;
    }
}

