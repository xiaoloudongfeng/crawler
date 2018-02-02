#include "core.h"

char **os_argv;

int main(int argc, char **argv)
{
    lua_State *L;

    //daemon(1, 1);

    os_argv = argv;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    LOG_INIT("[M]");

    if (redis_init("127.0.0.1", 6379) < 0) {
        LOG_ERROR("redis_init() failed");
        return -1;
    }

    L = init_lua_state("settings.lua");
    
    get_work_processes(L);
    get_connection_threshold(L);
    get_url_seed(L);

    free_lua_state(L);
    redis_free();

    start_work_processes();
    
    return 0;
}
