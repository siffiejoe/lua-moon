#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


typedef struct {
  int i;
} objex;


static int objex_print_me( lua_State* L ) {
  objex* ud = moon_checkudata( L, 1, "objex" );
  printf( "(print_me) objex={ i=%d } (upvalues: %d, %d)\n",
          ud->i,
          (int)lua_tonumber( L, lua_upvalueindex( 1 ) ),
          (int)lua_tonumber( L, lua_upvalueindex( 2 ) ) );
  return 0;
}


static int objex_index( lua_State* L ) {
  objex* ud = moon_checkudata( L, 1, "objex" );
  char const* s = luaL_checkstring( L, 2 );
  printf( "(__index) objex={ i=%d } (upvalues: %d, %d)\n",
          ud->i,
          /* upvalues are shifted by two positions!
           * the first two upvalues are used internally for
           * dispatching between methods and this function. */
          (int)lua_tonumber( L, lua_upvalueindex( 3 ) ),
          (int)lua_tonumber( L, lua_upvalueindex( 4 ) ) );
  if( 0 == strcmp( s, "i" ) )
    lua_pushinteger( L, ud->i );
  else
    lua_pushnil( L );
  return 1;
}


static int objex_newindex( lua_State* L ) {
  objex* ud = moon_checkudata( L, 1, "objex" );
  char const* s = luaL_checkstring( L, 2 );
  printf( "(__newindex) objex={ i=%d } (upvalues: %d, %d)\n",
          ud->i,
          (int)lua_tonumber( L, lua_upvalueindex( 1 ) ),
          (int)lua_tonumber( L, lua_upvalueindex( 2 ) ) );
  if( 0 == strcmp( s, "i" ) )
    ud->i = (int)moon_checkint( L, 3, INT_MIN, INT_MAX );
  return 0;
}


static int objex_new( lua_State* L ) {
  /* we don't need a uservalue table or even a reference to
   * another userdata for objex */
  objex* ud = moon_newobject( L, "objex", 0 );
  ud->i = 0;
  return 1;
}


int luaopen_objex( lua_State* L ) {
  luaL_Reg const objex_funcs[] = {
    { "new", objex_new },
    { NULL, NULL }
  };
  luaL_Reg const objex_metamethods[] = {
    { "__index", objex_index },
    { "__newindex", objex_newindex },
    { NULL, NULL }
  };
  luaL_Reg const objex_methods[] = {
    { "print_me", objex_print_me },
    { NULL, NULL }
  };
  moon_object_type const objex_type = {
    "objex",
    sizeof( objex ),
    0, /* NULL (function) pointer */
    objex_metamethods,
    objex_methods
  };
  /* push two upvalues for all objex (meta-)functions */
  lua_pushinteger( L, 1 );
  lua_pushinteger( L, 2 );
  /* define object type using the two upvalues */
  moon_defobject( L, &objex_type, 2 );
  lua_newtable( L );
  moon_register( L, objex_funcs );
  return 1;
}


