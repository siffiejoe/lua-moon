#include <stdlib.h>
#include <stdio.h>
#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>
#include "plugin.h"
#include "moon_dlfix.h"

int plugin_main( void ) {
  lua_State* L = luaL_newstate();
  luaL_openlibs( L );
#if 1
  MOON_DLFIX();
#endif
  if( luaL_dofile( L, "test.lua" ) ) {
    fprintf( stderr, "%s\n", lua_tostring( L, 1 ) );
    lua_close( L );
    return EXIT_FAILURE;
  }
  lua_close( L );
  return EXIT_SUCCESS;
}

