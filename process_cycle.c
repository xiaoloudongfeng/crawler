#include "process_cycle.h"
#include "murmur3.h"

int work_processes;
pid_t *work_processes_pid;
int connection_threshold;
int crawler_quit;

static void crawler_signal_handler(int signo)
{
    switch (signo) {
    case SIGINT:
        LOG_INFO("get a SIGINT");
        crawler_quit = 1;
        break;
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
        work_processes = 2;
        LOG_WARN("get_work_processes() failed, set work_process = %d", work_processes);
        return;
    }

    work_processes = lua_tointeger(L, -1);

    LOG_DEBUG("get work_processes[%d]", work_processes);
}

void get_connection_threshold(lua_State *L)
{
    lua_getglobal(L, "connection_threshold");

    if (!lua_isnumber(L, -1)) {
        connection_threshold = 1000;
        LOG_WARN("get_connection_threshold() failed, set connection_threshold = %d", connection_threshold);
        return;
    }

    connection_threshold = lua_tointeger(L, -1);

    LOG_DEBUG("get connection_threshold[%d]", connection_threshold);
}

void get_url_seed(lua_State *L)
{
    int         i;
    const char *url;
    char        key[128];
    unsigned    id;

    lua_getglobal(L, "seed");

    if (!lua_istable(L, -1)) {
        luaL_error(L, "'seed' should be a table\n");
    }

    for (i = 1; ; i++) {
        lua_rawgeti(L, -1, i);
        LOG_DEBUG("after lua_rawgeti()");
        
        if (!lua_isstring(L, -1)) {
            break;
        }
        LOG_DEBUG("after lua_isstring()");

        url = lua_tostring(L, -1);

        LOG_DEBUG("get url[%s]", url);

        // 计算url的hash值，并对work_processes取模，将url散列到不同的队列中
        MurmurHash3_x86_32(url, strlen(url), HASH_SEED, (void *)&id);
        id %= work_processes;

        // 根据id得到list的key
        memset(key, 0, sizeof(key));
        sprintf(key, "url_list_%d", id);

        // 插入Redis队列中
        LOG_DEBUG("rpush %s to %s", url, key);
        if (redis_list_rpush(key, url) < 0) {
            LOG_ERROR("redis_list_rpush() failed");

            return;
        }
        lua_pop(L, 1);
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

static void create_http_client(http_url_t *url, struct in_addr *sin_addr)
{
    http_client_create(url, sin_addr, parse_html);
}

static void work_process_cycle(int n)
{
    int              i;
    char            *url;
    char             key[128];
    http_url_t      *http_url;
    struct sigaction sa;

    sa.sa_handler = crawler_signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        LOG_ERROR("sigaction(SIGINT) failed");
        return;
    }

    sprintf(os_argv[0], "crawler: work process[%d]", n);

    // 如果在fork之前调用epoll_create创建文件描述
    // 符，那么父子进程共享同意epoll描述符，可能会
    // 引发一些问题，所以初始化操作均在fork之后调用
    if (event_init(1024) < 0) {
        LOG_ERROR("event_init() failed");
        return;
    }

    if (event_timer_init() < 0) {
        LOG_ERROR("event_timer_init() failed");
        return;
    }

    if (connection_init(1024) < 0) {
        LOG_ERROR("connection_init() failed");
        return;
    }

    // 同理，dns_init()中打开了一个文件描述符
    if (dns_init() < 0) {
        LOG_ERROR("dns_init() failed");
        return;
    }

    // 从hiredis结构体中可以看到，里面有一个文件描述符
    if (bloom_init() < 0) {
        LOG_ERROR("bloom_init() failed");
        return;
    }

    if (redis_init() < 0) {
        LOG_ERROR("redis_init() failed");
        return;
    }

    sprintf(key, "url_list_%d", n);
    process_setaffinity(n);

    for ( ;; ) {
        if (crawler_quit) {
            LOG_INFO("work process[%d] terminating...", n);
            break;
        }

        LOG_INFO("-----process_%d round[%d]-----", n, i++);

        if (get_free_connection_n() > connection_threshold) {
            if (redis_list_length(key) > 0) {
                url = redis_list_lpop(key);
                if (url) {
                    http_url = create_http_url(url);

                    if (http_url) {
                        dns_query(http_url, create_http_client);
                    }

                    free(url);
                }
            }
        }
        
        event_process(1000);

        event_expire_timers();
    }

    redis_free();
    exit(0);
}

int start_work_processes(void)
{
    pid_t            pid;
    int              i;
    struct sigaction sa;

    sa.sa_handler = crawler_signal_handler;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGINT, &sa, NULL) < 0) {
        LOG_ERROR("sigaction(SIGINT) failed");
        return -1;
    }
    
    work_processes_pid = malloc(sizeof(pid_t) * work_processes);
    if (!work_processes_pid) {
        LOG_ERROR("malloc failed");
        return -1;
    }

    for (i = 0; i < work_processes; i++) {
        work_processes_pid[i] = -1;
    }
   
    for (i = 0; i < work_processes; i++) {
        pid = fork();
    
        switch (pid) {
        case -1:
            LOG_ERROR("fork() failed");
            return -1;

        case 0:
            LOG_DEBUG("work process[%d] start", i);
            work_process_cycle(i);
            break;

        default:
            LOG_DEBUG("get work process[%d]", pid);
            work_processes_pid[i] = pid;
            break;
        }
    }

    for ( ;; ) {
        // master进程实现
        if (crawler_quit) {
            for (i = 0; i < work_processes; i++) {
                kill(work_processes_pid[i], SIGINT);
            }

            break;
        }

        sleep(5);
    }

    return 0;
}

