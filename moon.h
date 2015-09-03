/* Copyright 2013-2015 Philipp Janda <siffiejoe@gmx.net>
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


#define MOON_VERSION (200)
#define MOON_VERSION_MAJOR (MOON_VERSION/100)
#define MOON_VERSION_MINOR (MOON_VERSION-(MOON_VERSION_MAJOR*100))


/* fake CLANG feature detection on other compilers */
#ifndef __has_attribute
#  define __has_attribute( x ) 0
#endif

/* platform independent attributes for exporting/importing functions
 * from shared libraries */
#if !defined( _WIN32 ) && !defined( __CYGWIN__ )
#  ifndef MOON_LOCAL
#    define MOON_LOCAL
#  endif
#  ifndef MOON_EXPORT
#    define MOON_EXPORT __declspec(dllexport)
#  endif
#  ifndef MOON_IMPORT
#    define MOON_IMPORT __declspec(dllimport)
#  endif
#elif (defined( __GNUC__ ) && __GNUC__ >= 4) || __has_attribute( __visibility__ )
#  ifndef MOON_LOCAL
#    define MOON_LOCAL __attribute__((__visibility__("hidden")))
#  endif
#  ifndef MOON_EXPORT
#    define MOON_EXPORT __attribute__((__visibility__("default")))
#  endif
#  ifndef MOON_IMPORT
#    define MOON_IMPORT __attribute__((__visibility__("default")))
#  endif
#endif

#ifndef MOON_LOCAL
#  define MOON_LOCAL
#endif

#ifndef MOON_EXPORT
#  define MOON_EXPORT
#endif

#ifndef MOON_IMPORT
#  define MOON_IMPORT
#endif


/* handle linking tricks for moon toolkit */
#if defined( MOON_PREFIX )
/* - change the symbol names of functions to avoid linker conflicts
 * - moon.c needs to be compiled (and linked) separately
 */
#  if !defined( MOON_API )
/* the following is fine for static linking moon.c to your C module */
#    define MOON_API MOON_LOCAL
#  endif
#  undef MOON_INCLUDE_SOURCE
#else /* MOON_PREFIX */
/* - make all functions static and include the source (moon.c)
 * - don't change the symbol names of functions
 * - moon.c doesn't need to be compiled (and linked) separately
 */
#  define MOON_PREFIX moon
#  undef MOON_API
#  if defined( __GNUC__ ) || __has_attribute( __unused__ )
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
#define moon_newpointer     MOON_CONCAT( MOON_PREFIX, _newpointer )
#define moon_newfield       MOON_CONCAT( MOON_PREFIX, _newfield )
#define moon_killobject     MOON_CONCAT( MOON_PREFIX, _killobject )
#define moon_checkobject    MOON_CONCAT( MOON_PREFIX, _checkobject )
#define moon_testobject     MOON_CONCAT( MOON_PREFIX, _testobject )
#define moon_checkint       MOON_CONCAT( MOON_PREFIX, _checkint )
#define moon_optint         MOON_CONCAT( MOON_PREFIX, _optint )
#define moon_atexit         MOON_CONCAT( MOON_PREFIX, _atexit )
#define moon_setuvfield     MOON_CONCAT( MOON_PREFIX, _setuvfield )
#define moon_getuvfield     MOON_CONCAT( MOON_PREFIX, _getuvfield )
#define moon_getcache       MOON_CONCAT( MOON_PREFIX, _getcache )
#define moon_stack_         MOON_CONCAT( MOON_PREFIX, _stack_ )
#define moon_stack_assert_  MOON_CONCAT( MOON_PREFIX, _stack_assert_ )


/* all objects defined via moon_defobject share a common header of the
 * following type: */
typedef struct {
  unsigned char flags;
  unsigned char cleanup_offset;
  unsigned char vcheck_offset;
  unsigned char object_offset;
} moon_object_header;


/* flag values in moon_object_header: */
#define MOON_OBJECT_IS_VALID      0x01u
#define MOON_OBJECT_IS_POINTER    0x02u


/* additional Lua API functions in this toolkit */
MOON_API void moon_defobject( lua_State* L, char const* tname,
                              size_t sz, luaL_Reg const* methods,
                              int nup );
MOON_API void* moon_newobject( lua_State* L, char const* tname,
                               void (*destructor)( void* ) );
MOON_API void** moon_newpointer( lua_State* L, char const* tname,
                                 void (*destructor)( void* ) );
MOON_API void** moon_newfield( lua_State* L, char const* tname,
                               int idx, int (*isvalid)( void* p ),
                               void* p );
MOON_API void moon_killobject( lua_State* L, int idx );
MOON_API void* moon_checkobject( lua_State* L, int idx,
                                 char const* tname );
MOON_API void* moon_testobject( lua_State* L, int idx,
                                char const* tname );

MOON_API lua_Integer moon_checkint( lua_State* L, int idx,
                                    lua_Integer low,
                                    lua_Integer high );
MOON_API lua_Integer moon_optint( lua_State* L, int idx,
                                  lua_Integer low, lua_Integer high,
                                  lua_Integer def );
MOON_API int* moon_atexit( lua_State* L, lua_CFunction func );
MOON_API int moon_getuvfield( lua_State* L, int i, char const* key );
MOON_API void moon_setuvfield( lua_State* L, int i, char const* key );
MOON_API void moon_getcache( lua_State* L, int index );
MOON_API void moon_stack_( lua_State* L, char const* file, int line,
                           char const* func );
MOON_API void moon_stack_assert_( lua_State* L, char const* file,
                                  int line, char const* func, ... );


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


/* Lua version compatibility is out of scope for this library, so only
 * compatibility functions needed for the implementation are provided.
 * Consider using the Compat-5.3 project which provides backports of
 * many Lua 5.3 C API functions for Lua 5.1 and 5.2.
 */
#if LUA_VERSION_NUM < 502
MOON_API int moon_absindex( lua_State* L, int idx );
#else
#  define moon_absindex( L, i ) lua_absindex( L, i )
#endif


#if defined( MOON_INCLUDE_SOURCE )
#  include "moon.c"
#endif


#endif /* MOON_H_ */

