#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


#define MOON_FLAG_NAME "X"
#define MOON_FLAG_TYPE unsigned
#define MOON_FLAG_SUFFIX X
#define MOON_FLAG_USECACHE
#include "moon_flag.h"

#define MOON_FLAG_NAME "Y"
#define MOON_FLAG_TYPE unsigned
#define MOON_FLAG_SUFFIX Y
#include "moon_flag.h"


static int flgex_getXmethods( lua_State* L ) {
  if( moon_getmethods( L, "X" ) == LUA_TNIL )
    lua_pushnil( L );
  return 1;
}


int luaopen_flgex( lua_State* L ) {
  luaL_Reg const flgex_funcs[] = {
    { "getXmethods", flgex_getXmethods },
    { NULL, NULL }
  };
  /* define the flags and create their metatables */
  moon_flag_def_X( L );
  moon_flag_def_Y( L );
#if LUA_VERSION_NUM < 502
  luaL_register( L, "flgex", flgex_funcs );
#else
  luaL_newlib( L, flgex_funcs );
#endif
  /* create some predefined flags */
  moon_flag_new_X( L, 0 );
  lua_setfield( L, -2, "NULL" );
  moon_flag_new_X( L, 1 );
  lua_setfield( L, -2, "ONE" );
  moon_flag_new_X( L, 2 );
  lua_setfield( L, -2, "TWO" );
  moon_flag_new_Y( L, 4 );
  lua_setfield( L, -2, "THREE" );
  moon_flag_new_Y( L, 8 );
  lua_setfield( L, -2, "FOUR" );
  /* we don't use those functions in this example: */
  (void)moon_flag_get_X;
  (void)moon_flag_get_Y;
  return 1;
}

