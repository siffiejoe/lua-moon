/* Copyright 2013-2015 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

/* this include file is a macro file which could be included
 * multiple times with different settings.
 */
#include "moon.h"

/* "parameter checking" */
#ifndef MOON_FLAG_NAME
#  error MOON_FLAG_NAME is not defined
#endif

#ifndef MOON_FLAG_TYPE
#  error MOON_FLAG_TYPE is not defined
#endif

#ifndef MOON_FLAG_SUFFIX
#  error MOON_FLAG_SUFFIX is not defined
#endif

#define MOON_FLAG_ADD MOON_CONCAT( moon_flag_add_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_SUB MOON_CONCAT( moon_flag_sub_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_CALL MOON_CONCAT( moon_flag_call_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_AND MOON_CONCAT( moon_flag_and_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_NOT MOON_CONCAT( moon_flag_not_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_EQ MOON_CONCAT( moon_flag_eq_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_DEF MOON_CONCAT( moon_flag_def_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_NEW MOON_CONCAT( moon_flag_new_, MOON_FLAG_SUFFIX )
#define MOON_FLAG_GET MOON_CONCAT( moon_flag_get_, MOON_FLAG_SUFFIX )
#ifndef MOON_FLAG_EQMETHOD
#  define MOON_FLAG_EQMETHOD( a, b ) ((a) == (b))
#endif


static void MOON_FLAG_NEW( lua_State* L, MOON_FLAG_TYPE v ) {
#ifdef MOON_FLAG_USECACHE
  luaL_checkstack( L, 5, MOON_STRINGIFY( MOON_FLAG_NEW ) );
  luaL_getmetatable( L, MOON_FLAG_NAME );
  if( !lua_istable( L, -1 ) )
    luaL_error( L, "no metatable for type '%s' defined", MOON_FLAG_NAME );
  moon_getcache( L, -1 );
  lua_pushnumber( L, (lua_Number)v );
  lua_rawget( L, -2 );
  if( lua_isnil( L, -1 ) ) {
    lua_pop( L, 1 );
#endif
    *(MOON_FLAG_TYPE*)moon_newobject( L, MOON_FLAG_NAME, 0 ) = v;
#ifdef MOON_FLAG_USECACHE
    lua_pushnumber( L, (lua_Number)v );
    lua_pushvalue( L, -2 );
    lua_rawset( L, -4 );
  }
  lua_replace( L, -3 );
  lua_pop( L, 1 );
#endif
}

static MOON_FLAG_TYPE MOON_FLAG_GET( lua_State* L, int index ) {
  return *(MOON_FLAG_TYPE*)moon_checkobject( L, index, MOON_FLAG_NAME );
}

#ifndef MOON_FLAG_NOBITOPS
static int MOON_FLAG_ADD( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_TYPE* b = (MOON_FLAG_TYPE*)moon_checkobject( L, 2, MOON_FLAG_NAME );
  MOON_FLAG_NEW( L, (MOON_FLAG_TYPE)(*a | *b) );
  return 1;
}
static int MOON_FLAG_SUB( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_TYPE* b = (MOON_FLAG_TYPE*)moon_checkobject( L, 2, MOON_FLAG_NAME );
  MOON_FLAG_NEW( L, (MOON_FLAG_TYPE)(*a & ~(*b)) );
  return 1;
}
static int MOON_FLAG_CALL( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_TYPE* b = (MOON_FLAG_TYPE*)moon_checkobject( L, 2, MOON_FLAG_NAME );
  lua_pushboolean( L, !(~(*a) & (*b)) );
  return 1;
}
#if LUA_VERSION_NUM > 502
static int MOON_FLAG_AND( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_TYPE* b = (MOON_FLAG_TYPE*)moon_checkobject( L, 2, MOON_FLAG_NAME );
  MOON_FLAG_NEW( L, (MOON_FLAG_TYPE)(*a & *b) );
  return 1;
}
static int MOON_FLAG_NOT( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_NEW( L, (MOON_FLAG_TYPE)(~*a) );
  return 1;
}
#endif
#endif

#ifndef MOON_FLAG_NORELOPS
static int MOON_FLAG_EQ( lua_State* L ) {
  MOON_FLAG_TYPE* a = (MOON_FLAG_TYPE*)moon_checkobject( L, 1, MOON_FLAG_NAME );
  MOON_FLAG_TYPE* b = (MOON_FLAG_TYPE*)moon_checkobject( L, 2, MOON_FLAG_NAME );
  lua_pushboolean( L, MOON_FLAG_EQMETHOD( *a, *b ) );
  return 1;
}
#endif

static void MOON_FLAG_DEF( lua_State* L ) {
  luaL_Reg const methods[] = {
#ifndef MOON_FLAG_NOBITOPS
    { "__add", MOON_FLAG_ADD },
    { "__sub", MOON_FLAG_SUB },
    { "__call", MOON_FLAG_CALL },
#if LUA_VERSION_NUM > 502
    { "__band", MOON_FLAG_AND },
    { "__bor", MOON_FLAG_ADD },
    { "__bnot", MOON_FLAG_NOT },
#endif
#endif
#ifndef MOON_FLAG_NORELOPS
    { "__eq", MOON_FLAG_EQ },
#endif
    { NULL, NULL }
  };
  moon_defobject( L, MOON_FLAG_NAME, sizeof( MOON_FLAG_TYPE ),
                  methods, 0 );
}


#undef MOON_FLAG_ADD
#undef MOON_FLAG_SUB
#undef MOON_FLAG_CALL
#undef MOON_FLAG_AND
#undef MOON_FLAG_NOT
#undef MOON_FLAG_EQ
#undef MOON_FLAG_NEW
#undef MOON_FLAG_DEF
#undef MOON_FLAG_GET

#undef MOON_FLAG_NAME
#undef MOON_FLAG_TYPE
#undef MOON_FLAG_SUFFIX
#undef MOON_FLAG_NOBITOPS
#undef MOON_FLAG_NORELOPS
#undef MOON_FLAG_USECACHE
#undef MOON_FLAG_EQMETHOD

