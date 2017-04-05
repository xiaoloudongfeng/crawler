#include "core.h"

#include <hiredis/hiredis.h>

redisContext   *ctx;

int redis_init(const char *hostname, int port)
{
    struct timeval  timeout = {1, 500000};

    ctx = redisConnectWithTimeout(hostname, port, timeout);
    if (ctx == NULL || ctx->err) {
        if (ctx) {
            LOG_ERROR("redisConnectWithTimeout() failed: %s\n", ctx->errstr);
            redisFree(ctx);

        } else {
            LOG_ERROR("redisConnectWithTimeout: can't allocate redis context\n");
        }

        return -1;
    }

    return 0;
}

void redis_free(void)
{
    if (ctx) {
        redisFree(ctx);
        ctx = NULL;
    }
}

int redis_list_length(const char *key)
{
    int         length;
    redisReply *reply;

    reply = redisCommand(ctx, "llen %s", key);
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        length = -1;
        goto failed;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        LOG_ERROR("redisCommand() failed: reply->type != REDIS_REPLY_INTEGER");

        length = -1;
        goto failed;
    }

    length = reply->integer;

    LOG_DEBUG("length of %s[%d]", key, length);

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return length;
}

// 从右push
int redis_list_rpush(const char *key, const char *value)
{
    redisReply *reply;
    int         rc;

    rc = 0;

    reply = redisCommand(ctx, "rpush %s %s", key, value);
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        rc = -1;
    }

    if (reply) {
        freeReplyObject(reply);
    }

    return rc;
}

// 从左pop
char *redis_list_lpop(const char *key)
{
    char       *result;
    redisReply *reply;

    result = NULL;

    reply = redisCommand(ctx, "lpop %s", key);
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        goto failed;
    }

    if (reply->type != REDIS_REPLY_STRING) {
        LOG_ERROR("redisCommand() failed: reply->type != REDIS_REPLY_STRING");

        goto failed;
    }

    LOG_DEBUG("%s lpop: %s", key, reply->str);

    result = calloc(reply->len + 1, sizeof(char));
    if (result == NULL) {
        LOG_ERROR("redis_list_lpop() ->calloc() failed");
        
        goto failed;
    }

    strcpy(result, reply->str);

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return result;
}

int redis_string_setbit(const char *str, unsigned int off, unsigned int value)
{
    redisReply *reply;
    int         rc;

    rc = 0;

    LOG_DEBUG("SETBIT %s %u %u", str, off, (unsigned int)(value != 0));
    reply = redisCommand(ctx, "SETBIT %s %u %u", str, off, (unsigned int)(value != 0));
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        rc = -1;
    }

    if (reply) {
        freeReplyObject(reply);
    }

    return rc;
}

int redis_string_getbit(const char *str, unsigned int off, unsigned int *value)
{
    redisReply *reply;
    int         rc;

    rc = 0;

    LOG_DEBUG("GETBIT %s %u", str, off);
    reply = redisCommand(ctx, "GETBIT %s %u", str, off);
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        rc = -1;
        goto failed;
    }

    if (reply->type != REDIS_REPLY_INTEGER) {
        LOG_ERROR("redisCommand() failed: reply->type != REDIS_REPLY_INTEGER");

        rc = -1;
        goto failed;
    }

    *value = reply->integer;

failed:
    if (reply) {
        freeReplyObject(reply);
    }

    return rc;
}

int redis_string_bitop(const char *src1, const char *src2, const char *op, const char *dest)
{
    redisReply *reply;
    int         rc;

    rc = 0;

    LOG_DEBUG("BITOP %s %s %s %s", op, src1, src2, dest);
    reply = redisCommand(ctx, "BITOP %s %s %s %s", op, src1, src2, dest);
    if (ctx->err) {
        LOG_ERROR("redisCommand() failed: %s", ctx->errstr);

        rc = -1;
    }

    if (reply) {
        freeReplyObject(reply);
    }

    return rc;
}

