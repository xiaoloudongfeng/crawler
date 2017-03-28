#include "core.h"

#include <hiredis/hiredis.h>
#include "murmur3.h"

#define SEED_NUM	9
#define BIT_SIZE	4294967295L

// 从大素数表里面随便调了9个
unsigned int	seeds[SEED_NUM] = {	
									1999, 2887, 3989, 4969, 
									5557, 6661, 7883, 8887, 
									9901
								  };

redisContext   *ctx = NULL;

int bloom_init(void)
{
	const char	   *hostname = "127.0.0.1";
	int				port = 6379;
	struct timeval	timeout = {1, 500000};
	redisReply	   *reply = NULL;

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

	reply = redisCommand(ctx, "SETBIT %s %u %s", "bloom_filter", BIT_SIZE, "0");
	if (reply == NULL || ctx->err) {
		LOG_ERROR("redisCommand error: %s\n", ctx->errstr);
		
		return -1;
	}
	freeReplyObject(reply);

	reply = redisCommand(ctx, "BITOP %s %s %s %s", "XOR", "bloom_filter", "bloom_filter", "bloom_filter");
	if (reply == NULL || ctx->err) {
		LOG_ERROR("redisCommand error: %s\n", ctx->errstr);
		return -1;
	}
	freeReplyObject(reply);

	return 0;
}

int bloom_set(const char *str)
{
	unsigned int	i = 0;
	unsigned int	off = 0;
	redisReply	   *reply = NULL;

	for (i = 0; i < SEED_NUM; i++) {
		MurmurHash3_x86_32(str, strlen(str), seeds[i], (void *)&off);
		//printf("off[%d]: %u\n", i, off);
		
		reply = redisCommand(ctx, "SETBIT %s %u %s", "bloom_filter", off, "1");
		if (reply == NULL || ctx->err) {
			LOG_ERROR("redisCommand error: %s\n", ctx->errstr);
			
			return -1;
		}
		freeReplyObject(reply);
	}

	return 0;
}

int bloom_check(const char *str)
{
	unsigned int	i = 0;
	unsigned int	off = 0;
	redisReply	   *reply = NULL;

	for (i = 0; i < SEED_NUM; i++) {
		MurmurHash3_x86_32(str, strlen(str), seeds[i], (void *)&off);
		//printf("off[%d]: %u\n", i, off);
		
		reply = redisCommand(ctx, "GETBIT %s %u", "bloom_filter", off);
		if (reply == NULL || ctx->err) {
			LOG_ERROR("redisCommand error: %s\n", ctx->errstr);

			return -1;
		}

		//printf("reply->integer: %d\n", reply->integer);
		if (!reply->integer) {
			freeReplyObject(reply);
			return 0;
		}

		freeReplyObject(reply);
	}

	return 1;
}

int bloom_free(void)
{
	if (ctx) {
		redisFree(ctx);
	}
	ctx = NULL;

	return 0;
}

/*
int main(void)
{
	int ret = 0;
	ret = bloom_init();
	printf("bloom_init: %d\n", ret);
	
	ret = bloom_check("http://www.baidu.com");
	printf("bloom_check1: %d\n", ret);
	ret = bloom_set("http://www.baidu.com");
	printf("bloom_set: %d\n", ret);
	ret = bloom_check("http://www.baidu.com");
	printf("bloom_check2: %d\n", ret);
	
	ret = bloom_free();
}
*/
