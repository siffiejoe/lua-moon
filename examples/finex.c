#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


typedef int objA;
typedef int objB;


static int objA_gc( lua_State* L ) {
  objA* a = moon_checkudata( L, 1, "objA" );
  printf( "objA finalizer called for %p\n", (void*)a );
  return 0;
}


static int objA_before( lua_State* L ) {
  moon_checkudata( L, 1, "objA" );
  moon_checkudata( L, 2, "objB" );
  moon_getuservalue( L, 1 );
  lua_pushvalue( L, 2 );
  lua_pushboolean( L, 1 );
  lua_rawset( L, -3 );
  moon_finalizer( L, 1 );
  return 0;
}


static int objA_new( lua_State* L ) {
  objA* a = NULL;
  lua_newtable( L );
  a = moon_newobject( L, "objA", -1 );
  printf( "objA %p created\n", (void*)a );
  return 1;
}


static int objB_gc( lua_State* L ) {
  objB* b = moon_checkudata( L, 1, "objB" );
  printf( "objB finalizer called for %p\n", (void*)b );
  return 0;
}


static int objB_new( lua_State* L ) {
  objB* b = moon_newobject( L, "objB", 0 );
  printf( "objB %p created\n", (void*)b );
  return 1;
}


int luaopen_finex( lua_State* L ) {
  luaL_Reg const finex_funcs[] = {
    { "newA", objA_new },
    { "newB", objB_new },
    { NULL, NULL }
  };
  luaL_Reg const objA_metamethods[] = {
    { "__gc", objA_gc },
    { NULL, NULL }
  };
  luaL_Reg const objA_methods[] = {
    { "before", objA_before },
    { NULL, NULL }
  };
  luaL_Reg const objB_metamethods[] = {
    { "__gc", objB_gc },
    { NULL, NULL }
  };
  moon_object_type const objA_type = {
    "objA",
    sizeof( objA ),
    0, /* NULL (function) pointer */
    objA_metamethods,
    objA_methods
  };
  moon_object_type const objB_type = {
    "objB",
    sizeof( objB ),
    0, /* NULL (function) pointer */
    objB_metamethods,
    NULL
  };
  moon_defobject( L, &objA_type, 0 );
  moon_defobject( L, &objB_type, 0 );
  lua_newtable( L );
  moon_register( L, finex_funcs );
  return 1;
}

