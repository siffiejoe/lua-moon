/* Copyright 2013,2014 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

#ifndef MOON_H_
#define MOON_H_

/* file: moon.h
 * Utility functions/macros for binding C code to Lua.
 */

#include <stddef.h>
#include <lua.h>
#include <lauxlib.h>


/* handle linking tricks for moon toolkit */
#if defined( MOON_PREFIX )
/* - change the symbol names of functions to avoid linker conflicts
 * - moon.c needs to be compiled (and linked) separately
 */
#  if !defined( MOON_API )
#    define MOON_API extern
#  endif
#  undef MOON_INCLUDE_SOURCE
#else /* MOON_PREFIX */
/* - make all functions static and include the source (moon.c)
 * - don't change the symbol names of functions
 * - moon.c doesn't need to be compiled (and linked) separately
 */
#  define MOON_PREFIX moon
#  undef MOON_API
#  if defined( __GNUC__ ) || defined( __clang__ )
#    define MOON_API __attribute__((__unused__)) static
#  else
#    define MOON_API static
#  endif
#  define MOON_INCLUDE_SOURCE
#endif /* MOON_PREFIX */


/* some helper macros */
#define MOON_CONCAT_HELPER( a, b ) a##b
#define MOON_CONCAT( a, b )  MOON_CONCAT_HELPER( a, b )
#define MOON_STRINGIFY_HELPER( a ) #a
#define MOON_STRINGIFY( a )  MOON_STRINGIFY_HELPER( a )

/* make sure all functions can be called using the moon_ prefix, even
 * if we change the prefix behind the scenes */
#define moon_defobject      MOON_CONCAT( MOON_PREFIX, _defobject )
#define moon_newobject      MOON_CONCAT( MOON_PREFIX, _newobject )
#define moon_newobject_ref  MOON_CONCAT( MOON_PREFIX, _newobject_ref )
#define moon_propindex      MOON_CONCAT( MOON_PREFIX, _propindex )
#define moon_finalizer      MOON_CONCAT( MOON_PREFIX, _finalizer )
#define moon_preload_c      MOON_CONCAT( MOON_PREFIX, _preload_c )
#define moon_preload_lua    MOON_CONCAT( MOON_PREFIX, _preload_lua )
#define moon_setuvfield     MOON_CONCAT( MOON_PREFIX, _setuvfield )
#define moon_getuvfield     MOON_CONCAT( MOON_PREFIX, _getuvfield )
#define moon_light2full     MOON_CONCAT( MOON_PREFIX, _light2full )
#define moon_lookuptable    MOON_CONCAT( MOON_PREFIX, _lookuptable )
#define moon_pushoption     MOON_CONCAT( MOON_PREFIX, _pushoption )
#define moon_checkoption    MOON_CONCAT( MOON_PREFIX, _checkoption )


/* the type used to collect all necessary information to define an
 * object */
typedef struct {
  char const* metatable_name;
  size_t userdata_size;
  void (*initializer)( void* );
  luaL_Reg const* metamethods;
  luaL_Reg const* methods;
} moon_object_type;


/* necessary information to preload lua modules */
typedef struct {
  char const* require_name;
  char const* error_name;
  char const* code;
  size_t code_length;
} moon_lua_reg;


/* additional Lua API functions in this toolkit */
MOON_API void moon_defobject( lua_State* L,
                              moon_object_type const* t, int nup );
MOON_API void* moon_newobject( lua_State* L, char const* name,
                               int env_index );
MOON_API void* moon_newobject_ref( lua_State* L, char const* name,
                                   int ref_index );
MOON_API void moon_propindex( lua_State* L, luaL_Reg const methods[],
                              lua_CFunction pindex, int nups );
MOON_API int* moon_finalizer( lua_State* L, lua_CFunction func );
MOON_API void moon_preload_c( lua_State* L, luaL_Reg const libs[] );
MOON_API void moon_preload_lua( lua_State* L,
                                moon_lua_reg const libs[] );
MOON_API int moon_getuvfield( lua_State* L, int i, char const* key );
MOON_API void moon_setuvfield( lua_State* L, int i, char const* key );
MOON_API void moon_light2full( lua_State* L, int index );
MOON_API void moon_lookuptable( lua_State* L,
                                char const* const names[],
                                unsigned const values[] );
MOON_API void moon_pushoption( lua_State* L, unsigned val,
                               unsigned const values[],
                               char const* const names[],
                               int lookupindex );
MOON_API unsigned moon_checkoption( lua_State* L, int idx,
                                    char const* def,
                                    char const* const names[],
                                    unsigned const values[],
                                    int lookupindex );


/* some debugging macros */
#ifndef NDEBUG
#  if (defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L) || \
      defined( __GNUC__ ) || defined( __clang__ )
#    define moon_stack_assert( L, ... ) \
       moon_stack_assert_( L, __FILE__, __LINE__, __func__, __VA_ARGS__, (char const*)NULL )
#    define moon_stack( L ) \
       moon_stack_( L, __FILE__, __LINE__, __func__ )
#  elif defined( _MSC_VER ) && _MSC_VER >= 1100L
#    define moon_stack_assert( L, ... ) \
       moon_stack_assert_( L, __FILE__, __LINE__, __FUNCTION__, __VA_ARGS__, (char const*)NULL )
#    define moon_stack( L ) \
       moon_stack_( L, __FILE__, __LINE__, __FUNCTION__ )
#  else
#    error preprocessor does not support variadic macros
#  endif

#  define moon_stack_          MOON_CONCAT( MOON_PREFIX, _stack_ )
#  define moon_stack_assert_   MOON_CONCAT( MOON_PREFIX, _stack_assert_ )
MOON_API void moon_stack_( lua_State* L, char const* file, int line,
                           char const* func );
MOON_API void moon_stack_assert_( lua_State* L, char const* file,
                                  int line, char const* func, ... );

#else
#  define moon_stack( L )  ((void)(0))
#  if (defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 199901L) || \
      (defined( _MSC_VER ) && _MSC_VER >= 1100L) || \
      defined( __GNUC__ ) || defined( __clang__ )
#    define moon_stack_assert( ... ) ((void)0)
#  else
#    error preprocessor does not support variadic macros
#  endif
#endif


/* handle compatibility with different Lua versions */
#if !defined( LUA_VERSION_NUM ) || LUA_VERSION_NUM < 501
#  error unsupported Lua version
#elif LUA_VERSION_NUM == 501

/* Lua 5.1 support */
#  define moon_checkudata   MOON_CONCAT( MOON_PREFIX, _checkudata )
#  define moon_testudata    MOON_CONCAT( MOON_PREFIX, _testudata )
#  define moon_absindex     MOON_CONCAT( MOON_PREFIX, _absindex )
#  define moon_register( L, libs ) luaL_register( (L), NULL, libs )
#  define moon_setuservalue( L, i ) lua_setfenv( L, i )
#  define moon_getuservalue( L, i ) lua_getfenv( L, i )
#  define moon_rawlen( L, i ) lua_objlen( L, i )
MOON_API int moon_absindex( lua_State* L, int i );
MOON_API void* moon_testudata( lua_State* L, int i, char const* tname );
MOON_API void* moon_checkudata( lua_State* L, int i, char const* tname );

#elif LUA_VERSION_NUM == 502

/* Lua 5.2 support */
#  define moon_checkudata   MOON_CONCAT( MOON_PREFIX, _checkudata )
#  define moon_testudata( L, i, t ) luaL_testudata( L, i, t )
#  define moon_absindex( L, i ) lua_absindex( L, i )
#  define moon_register( L, libs ) luaL_setfuncs( L, libs, 0 )
#  define moon_setuservalue( L, i ) lua_setuservalue( L, i )
#  define moon_getuservalue( L, i ) lua_getuservalue( L, i )
#  define moon_rawlen( L, i ) lua_rawlen( L, i )
MOON_API void* moon_checkudata( lua_State* L, int i, char const* tname );

#elif LUA_VERSION_NUM == 503

/* Lua 5.3 support */
#  define moon_checkudata( L, i, t ) luaL_checkudata( L, i, t )
#  define moon_testudata( L, i, t ) luaL_testudata( L, i, t )
#  define moon_absindex( L, i ) lua_absindex( L, i )
#  define moon_register( L, libs ) luaL_setfuncs( L, libs, 0 )
#  define moon_setuservalue( L, i ) lua_setuservalue( L, i )
#  define moon_getuservalue( L, i ) lua_getuservalue( L, i )
#  define moon_rawlen( L, i ) lua_rawlen( L, i )

#else
#  error unsupported Lua version
#endif


#if defined( MOON_INCLUDE_SOURCE )
#  include "moon.c"
#endif


#endif /* MOON_H_ */

