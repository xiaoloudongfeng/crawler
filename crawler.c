#include "core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "process_cycle.h"

int main(void)
{
    lua_State *L;

    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    if (redis_init() < 0) {
        LOG_ERROR("redis_init() failed");
        return -1;
    }

    L = init_lua_state("settings.lua");
    
    get_work_processes(L);
    get_connection_threshold(L);
    get_url_seed(L);

    free_lua_state(L);
    LOG_DEBUG("free_lua_state()");
    redis_free();
    LOG_DEBUG("redis_free()");

    start_work_processes();
    
    return 0;
}
