#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


static int assign_double( lua_State* L, int idx, void* ptr ) {
  if( lua_type( L, idx ) != LUA_TNUMBER )
    return 0;
  *(lua_Number*)ptr = lua_tonumber( L, idx );
  return 1;
}


static int get_number_array( lua_State* L ) {
  lua_Number buffer[ 5 ];
  size_t n = sizeof( buffer )/sizeof( *buffer );
  lua_Number* ptr = moon_checkarray( L, 1, buffer, &n,
                                     sizeof( lua_Number ), assign_double );
  size_t i = 0;
  printf( "static buffer: %p, array: %p, nelems: %zu\n",
          (void*)buffer, (void*)ptr, n );
  for( i = 0; i < n; ++i ) {
    printf( "\t%02zu: %g\n", i, ptr[ i ] );
  }
  return 0;
}


int luaopen_arrex( lua_State* L ) {
  luaL_Reg const funcs[] = {
    { "array", get_number_array },
    { NULL, NULL }
  };
  lua_newtable( L );
  moon_register( L, funcs );
  return 1;
}

