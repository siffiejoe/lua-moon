#include <lua.h>
#include <lauxlib.h>
#include <stddef.h>
#include "moon.h"

static int somefunc( lua_State* L ) {
  moon_stack_assert( L, "s", "ds", "t" );
  lua_pushinteger( L, lua_gettop( L ) );
  return 1;
}

int luaopen_stkex( lua_State* L ) {
  luaL_Reg const stkex_funcs[] = {
    { "somefunc", somefunc },
    { NULL, NULL }
  };
#if LUA_VERSION_NUM < 502
  luaL_register( L, "stkex", stkex_funcs );
#else
  luaL_newlib( L, stkex_funcs );
#endif
  return 1;
}
