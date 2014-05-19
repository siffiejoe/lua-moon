#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


typedef struct {
  int  x;
  int  y;
} embedded;

typedef struct {
  int       z;
  embedded  child;
} parent;


#define ASSIGN_INT( L, _lhs, _idx ) \
  _lhs = luaL_checkint( L, _idx )
#define MOON_SUB_NAME    "EmbeddedType"
#define MOON_SUB_TYPE    embedded
#define MOON_SUB_SUFFIX  emb
#define MOON_SUB_PARENT  parent
#define MOON_SUB_FIELD   child
#define MOON_SUB_ELEMENTS( _ ) \
  _( x, lua_pushinteger, ASSIGN_INT, 1 ) \
  _( y, lua_pushinteger, ASSIGN_INT, 1 )
#include "moon_sub.h"
#undef ASSIGN_INT


static int subex_print_me( lua_State* L ) {
  parent* ud = moon_checkudata( L, 1, "subex" );
  printf( "parent={ z=%d, child={ x=%d, y=%d } }\n",
          ud->z, ud->child.x, ud->child.y );
  return 0;
}


static int subex_index( lua_State* L ) {
  parent* ud = moon_checkudata( L, 1, "subex" );
  size_t len = 0;
  char const* k = lua_tolstring( L, 2, &len );
  if( k == 0 )
    lua_pushnil( L );
  else if( len == 1 && k[ 0 ] == 'z' )
    lua_pushinteger( L, ud->z );
  else if( len == sizeof( "child" )-1 &&
           0 == memcmp( "child", k, len ) ) {
    /* create or retrieve cached userdata for child
     * element. index of parent userdata required. */
    moon_sub_new_emb( L, 1 );
  } else
    lua_pushnil( L );
  return 1;
}


static int subex_newindex( lua_State* L ) {
  parent* ud = moon_checkudata( L, 1, "subex" );
  size_t len = 0;
  char const* k = luaL_checklstring( L, 2, &len );
  if( len == 1 && k[ 0 ] == 'z' )
    ud->z = luaL_checkint( L, 3 );
  /* we usually don't allow setting sub-structs, but we
   * could ... */
  else
    luaL_error( L, "invalid field" );
  return 0;
}


static int subex_new( lua_State* L ) {
  parent* ud = NULL;
  lua_newtable( L );
  ud = moon_newobject( L, "subex", -1 );
  ud->z = 0;
  ud->child.x = 0;
  ud->child.y = 0;
  return 1;
}


int luaopen_subex( lua_State* L ) {
  luaL_Reg const subex_funcs[] = {
    { "new", subex_new },
    { NULL, NULL }
  };
  luaL_Reg const subex_metamethods[] = {
    { "__index", subex_index },
    { "__newindex", subex_newindex },
    { NULL, NULL }
  };
  luaL_Reg const subex_methods[] = {
    { "print_me", subex_print_me },
    { NULL, NULL }
  };
  moon_object_type const subex_type = {
    "subex",
    sizeof( parent ),
    0, /* NULL (function) pointer */
    subex_metamethods,
    subex_methods
  };
  /* register parent userdata type */
  moon_defobject( L, &subex_type, 0 );
  /* register sub-userdata type */
  moon_sub_def_emb( L, 0 );
  /* create module table */
  lua_newtable( L );
  moon_register( L, subex_funcs );
  return 1;
}

