#ifndef _PROCESS_CYCLE_H_INCLUDED
#define _PROCESS_CYCLE_H_INCLUDED

#include "core.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

extern char **os_argv;

#define HASH_SEED 8887

extern int work_processes;

lua_State *init_lua_state(const char *fname);

void get_work_processes(lua_State *L);

void get_connection_threshold(lua_State *L);

void get_url_seed(lua_State *L);

void free_lua_state(lua_State *L);

int start_work_processes(void);

#endif

