/* Copyright 2013,2014 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

/* this include file is a macro file which could be included
 * multiple times with different settings.
 */
#include <string.h>  /* for memcmp */
#include "moon.h"

/* "parameter checking" */
#ifndef MOON_SUB_NAME
#  error MOON_SUB_NAME is not defined
#endif

#ifndef MOON_SUB_TYPE
#  error MOON_SUB_TYPE is not defined
#endif

#ifndef MOON_SUB_SUFFIX
#  error MOON_SUB_SUFFIX is not defined
#endif

#ifndef MOON_SUB_PARENT
#  error MOON_SUB_PARENT is not defined
#endif

#ifndef MOON_SUB_FIELD
#  error MOON_SUB_FIELD is not defined
#endif

#ifndef MOON_SUB_ELEMENTS
#  error MOON_SUB_ELEMENTS is not defined
#endif

/* generate unique names for static functions */
#define MOON_SUB_DEF MOON_CONCAT( moon_sub_def_, MOON_SUB_SUFFIX )
#define MOON_SUB_NEW MOON_CONCAT( moon_sub_new_, MOON_SUB_SUFFIX )
#define MOON_SUB_INDEX MOON_CONCAT( moon_sub_index_, MOON_SUB_SUFFIX )
#define MOON_SUB_NEWINDEX MOON_CONCAT( moon_sub_newindex_, MOON_SUB_SUFFIX )
#define MOON_SUB_PAIRS MOON_CONCAT( moon_sub_pairs_, MOON_SUB_SUFFIX )
#define MOON_SUB_NEXT MOON_CONCAT( moon_sub_next_, MOON_SUB_SUFFIX )
#define MOON_SUB_FIELDNAME MOON_STRINGIFY( MOON_SUB_FIELD )
#define MOON_SUB_TYPENAME MOON_STRINGIFY( MOON_SUB_TYPE )


static void MOON_SUB_NEW( lua_State* L, int index ) {
  luaL_checkstack( L, 3, "not enough stack space available" );
  index = moon_absindex( L, index );
  if( LUA_TNIL == moon_getuvfield( L, index, MOON_SUB_FIELDNAME ) ) {
#ifdef MOON_SUB_POINTER
    MOON_SUB_PARENT** pp = lua_touserdata( L, index );
    MOON_SUB_PARENT* p = pp != NULL ? *pp : NULL;
#else
    MOON_SUB_PARENT* p = lua_touserdata( L, index );
#endif
    MOON_SUB_TYPE** sub = NULL;
    lua_pop( L, 1 );
    if( p == NULL )
      luaL_error( L, "pointer to parent struct is NULL" );
    sub = moon_newobject_ref( L, MOON_SUB_NAME, index );
    *sub = &(p->MOON_SUB_FIELD);
    lua_pushvalue( L, -1 );
    moon_setuvfield( L, index, MOON_SUB_FIELDNAME );
  }
}

static int MOON_SUB_INDEX( lua_State* L ) {
  MOON_SUB_TYPE** se = moon_checkudata( L, 1, MOON_SUB_NAME );
  size_t len = 0;
  char const* k = lua_tolstring( L, 2, &len );
  if( k == NULL )
    lua_pushnil( L );
  else {
#define MOON_SUB_LOOKUP_FIELD( _name, _push, _assign, _enable ) \
    if( len == sizeof( #_name )-1 && \
        0 == memcmp( #_name, k, len ) ) { \
      int top = lua_gettop( L ); \
      _push( L, (*se)->_name ); \
      lua_settop( L, top+1 ); \
    } else
    MOON_SUB_ELEMENTS( MOON_SUB_LOOKUP_FIELD )
#undef MOON_SUB_LOOKUP_FIELD
    lua_pushnil( L );
  }
  return 1;
}

static int MOON_SUB_NEWINDEX( lua_State* L ) {
  MOON_SUB_TYPE** se = moon_checkudata( L, 1, MOON_SUB_NAME );
  size_t len = 0;
  char const* k = luaL_checklstring( L, 2, &len );
#define MOON_SUB_WRITE_FIELD( _name, _push, _assign, _enable ) \
  if( _enable && len == sizeof( #_name )-1 && \
      0 == memcmp( #_name, k, len ) ) { \
    _assign( L, (*se)->_name, 3 ); \
  } else
  MOON_SUB_ELEMENTS( MOON_SUB_WRITE_FIELD )
#undef MOON_SUB_WRITE_FIELD
  luaL_error( L, "invalid field for " MOON_SUB_TYPENAME );
  return 0;
}

#if LUA_VERSION_NUM > 501 && !defined( MOON_SUB_NOPAIRS )
static int MOON_SUB_NEXT( lua_State* L ) {
  MOON_SUB_TYPE** se = moon_checkudata( L, 1, MOON_SUB_NAME );
  size_t len = 0;
  char const* k = lua_tolstring( L, 2, &len );
  luaL_argcheck( L, k != NULL || lua_isnoneornil( L, 2 ), 2,
                 "nil or string key required" );
  if( k == NULL ) {
#define MOON_SUB_LOOKUP_NEXT( _name, _push, _assign, _enable ) \
    int top = lua_gettop( L ); \
    lua_pushliteral( L, #_name ); \
    _push( L, (*se)->_name ); \
    lua_settop( L, top+2 ); \
    return 2; \
  } else if( len == sizeof( #_name )-1 && \
             0 == memcmp( #_name, k, len ) ) {
  MOON_SUB_ELEMENTS( MOON_SUB_LOOKUP_NEXT )
#undef MOON_SUB_LOOKUP_NEXT
    lua_pushnil( L );
    return 1;
  } else
    luaL_error( L, "unknown key" );
  return 0;
}

static int MOON_SUB_PAIRS( lua_State* L ) {
  moon_checkudata( L, 1, MOON_SUB_NAME );
  if( !luaL_getmetafield( L, 1, "__next" ) )
    luaL_error( L, "iterator function not defined" );
  lua_pushvalue( L, 1 );
  lua_pushnil( L );
  return 3;
}
#endif

static void MOON_SUB_DEF( lua_State* L, int nups ) {
  luaL_Reg const metamethods[] = {
    { "__index", MOON_SUB_INDEX },
    { "__newindex", MOON_SUB_NEWINDEX },
#if LUA_VERSION_NUM > 501 && !defined( MOON_SUB_NOPAIRS )
    { "__pairs", MOON_SUB_PAIRS },
    { "__next", MOON_SUB_NEXT },
#endif
    { NULL, NULL }
  };
  moon_object_type const sub_type = {
    MOON_SUB_NAME,
    sizeof( MOON_SUB_TYPE* ),
    0, /* NULL (function) pointer */
    metamethods,
    NULL, /* no methods */
  };
  moon_defobject( L, &sub_type, nups );
}


/* cleanup temporary macros */
#undef MOON_SUB_DEF
#undef MOON_SUB_NEW
#undef MOON_SUB_INDEX
#undef MOON_SUB_NEWINDEX
#undef MOON_SUB_PAIRS
#undef MOON_SUB_NEXT
#undef MOON_SUB_FIELDNAME
#undef MOON_SUB_TYPENAME

#undef MOON_SUB_NAME
#undef MOON_SUB_TYPE
#undef MOON_SUB_SUFFIX
#undef MOON_SUB_PARENT
#undef MOON_SUB_FIELD
#undef MOON_SUB_ELEMENTS
#undef MOON_SUB_POINTER
#undef MOON_SUB_NOPAIRS

