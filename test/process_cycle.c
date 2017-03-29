#include "core.h"

typedef void (*work_process_cycle_pt)(int num);

int             work_processes;
redisContext   *ctx;

int redis_init(void)
{
    const char     *hostname = "127.0.0.1";
    int             port = 6379;
    struct timeval  timeout = {1, 500000};
    redisReply     *reply = NULL;

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

// 从右面push
int redis_list_rpush(const char *key, const char *value)
{
    redisReply *reply;

    reply = redisCommand(ctx, "rpush %s %s", key, value);
    if (reply == NULL || ctx->err) {
        LOG_ERROR("redisCommand failed: %s", ctx->errstr);

        return -1;
    }

    return 0;
}

// 从左侧pop
int redis_list_lpop(const char *key, call_back_t cb, void *data)
{
    redisReply *reply;

    reply = redisCommand(ctx, "lpop %s", key)

    return 0;
}

void redis_free(void)
{
    if (ctx) {
        redisFree(ctx);
        ctx = NULL;
    }
}

lua_State *init_lua_state(const char *fname)
{
    lua_State *L = lua_open();
    luaL_openlibs(L);

    if (luaL_loadfile(L, fname) || lua_pcall(L, 0, 0, 0)) {
        luaL_error(L, "cannot run config. file: %s", lua_tostring(L, -1));
    }

    return L;
}

void get_work_processes(lua_State *L)
{
    lua_getglobal(L, "work_processes");

    if (!lua_isnumber(L, -1)) {
        luaL_error(L, "'work_processes' should be a number\n");
    }

    work_processes = lua_tointeger(L, -1);

    LOG_DEBUG("get work_processes[%d]", work_processes);
}

void get_url_seed(lua_State *L)
{
    int i;

    lua_getglobal(L, "seed");

    if (!lua_istable(L, -1)) {
        luaL_error(L, "'seed' should be a table\n");
    }

    for (i = 1; ; i++) {
        lua_rawgeti(L, -1, i);
        
        if (!lua_isstring(L, -1)) {
            break;
        }

        LOG_DEBUG("get url[%s]", lua_tostring(L, -1));

        // TODO
        // 插入Redis队列中
    }
}

void free_lua_state(lua_State *L)
{
    lua_close(L);
}

static void process_setaffinity(int n)
{
    pid_t       process_id;
    cpu_set_t   mask;
    
    CPU_ZERO(&mask);
    CPU_SET(n, &mask);

    process_id = getpid();
    if (sched_setaffinity(process_id, sizeof(cpu_set_t), &mask) < 0) {
        LOG_ERROR("sched_setaffinity() failed");
    }
}

static void work_process_cycle(int n)
{
    int i;

    queue_init(&url_q);

    // 如果在fork之前调用epoll_create创建文件描述
    // 符，那么父子进程共享同意epoll描述符，可能会
    // 引发一些问题，所以初始化操作均在fork之后调用
    if (event_init(1024) < 0) {
        return -1;
    }

    if (event_timer_init() < 0) {
        return -1;
    }

    if (connection_init(1024) < 0) {
        return -1;
    }

    // 同理，dns_init()中打开了一个文件描述符
    if (dns_init() < 0) {
        return -1;
    }

    // 从hiredis结构体中可以看到，里面有一个文件描述符
    if (bloom_init() < 0) {
        return -1;
    }

    process_setaffinity(n);

    for ( ;; ) {
        LOG_INFO("-----process_%d round[%d]-----", n, i++);
        if (get_free_connection_n() > 1000) {
            queue_t *p = url_q.next;

            if (p != &rul_q) {
                queue_remove(p);

                http_url_t *url = queue_data(p, http_url_t, queue);

                dns_query(url, create_http_client);
            } else {
                // 从Redis队列中获取对应的url地址
                url_list_pop(&http_url_t, n);
            }
        }
        
        event_process(1000);

        event_expire_timers();
    }
}

int start_work_processes(int n)
{
    pid_t pid;
    int   i;

    for (i = 0; i < n; i++) {
        pid = fork();
    
        switch (pid) {
        case -1:
            LOG_ERROR("fork() failed");
            return -1;

        case 0:
            LOG_DEBUG("process_%d start", n);
            work_process_cycle(i);
            break;

        default:
            break;
        }
    }
}

