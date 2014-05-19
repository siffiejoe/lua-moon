#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


#define MOON_FLAG_NAME "A"
#define MOON_FLAG_TYPE unsigned
#define MOON_FLAG_SUFFIX A
#define MOON_FLAG_USECACHE
#include "moon_flag.h"

#define MOON_FLAG_NAME "B"
#define MOON_FLAG_TYPE unsigned
#define MOON_FLAG_SUFFIX B
#include "moon_flag.h"


int luaopen_flgex( lua_State* L ) {
  /* define the flags and create their metatables */
  moon_flag_def_A( L, 0 );
  moon_flag_def_B( L, 0 );
  /* create some predefined flags */
  lua_newtable( L );
  moon_flag_new_A( L, 0 );
  lua_setfield( L, -2, "NULL" );
  moon_flag_new_A( L, 1 );
  lua_setfield( L, -2, "ONE" );
  moon_flag_new_A( L, 2 );
  lua_setfield( L, -2, "TWO" );
  moon_flag_new_B( L, 4 );
  lua_setfield( L, -2, "THREE" );
  moon_flag_new_B( L, 8 );
  lua_setfield( L, -2, "FOUR" );
  /* we don't use those functions in this example: */
  (void)moon_flag_get_A;
  (void)moon_flag_get_B;
  return 1;
}

