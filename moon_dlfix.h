/* Copyright 2013-2016 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

#ifndef MOON_DLFIX_H_
#define MOON_DLFIX_H_

/* Provides a C function macro that tries to reopen the Lua library
 * in global mode, so that C extension modules do not have to be
 * relinked for Lua VMs in shared libraries.
 */


/* enable/disable debug output */
#if 0
#include <stdio.h>
#define MOON_DLFIX_DBG( _a ) (printf _a)
#else
#define MOON_DLFIX_DBG( _a ) ((void)0)
#endif


/* detect some form of UNIX, so that unistd.h can be included to
 * use other feature macros */
#if defined( unix ) || defined( __unix ) || defined( __unix__ ) || \
    defined( __TOS_AIX__ ) || defined( _SYSTYPE_BSD ) || \
    (defined( __APPLE__ ) && defined( __MACH__ )) || \
    defined( HAVE_UNISTD_H )

#include <unistd.h>
/* check for minimum POSIX version that specifies dlopen etc. */
#if (defined( _POSIX_VERSION ) && _POSIX_VERSION >= 200112L) || \
    defined( HAVE_DLFCN_H )

#include <dlfcn.h>
/* check for the existence of the RTLD_NOLOAD flag
 * - GLIBC has it (since 2.2)
 * - FreeBSD has it
 * - ...
 */
#ifdef RTLD_NOLOAD

/* Lua VM library names to try
 * Most of them require the dev-package to be installed,
 * but guessing the real library names for multiple distros
 * is pretty difficult ...
 * If the library is not in the default search path you can
 * set LD_LIBRARY_PATH, or use MOON_DLFIX_LIBNAME to specify
 * an absolute path to the library.
 */
static char const* const moon_dlfix_lib_names[] = {
/* you can define your own custom name via compiler define: */
#ifdef MOON_DLFIX_LIBNAME
  MOON_DLFIX_LIBNAME,
#endif /* custom Lua library name */
  "liblua5.3.so",       /* Lua 5.3, Debian/Ubuntu naming */
  "liblua5.3.so.0",     /*   same with common SONAME */
  "liblua53.so",
  "liblua5.2.so",       /* Lua 5.2, Debian/Ubuntu naming */
  "liblua5.2.so.0",     /*   same with common SONAME */
  "liblua52.so",
  "liblua.so",          /* default name from Lua's makefile */
  "liblua5.1.so",       /* Lua 5.1, Debian/Ubuntu naming */
  "liblua5.1.so.0",     /*   same with common SONAME */
  "liblua51.so",
  "libluajit-5.1.so",   /* LuaJIT default name */
  "libluajit-5.1.so.2", /*   common SONAME */
};


/* On some OSes we can iterate over all loaded libraries to
 * find the Lua shared object.
 */
#if defined( __linux__ ) && defined( _GNU_SOURCE )
#define MOON_DLFIX_DL_ITERATE_PHDR
#include <link.h>
#endif /* Linux with dl_iterate_phdr() function */
#if defined( __FreeBSD__ ) || defined( __DragonFly__ )
#define MOON_DLFIX_DL_ITERATE_PHDR
#include <link.h>
#endif /* FreeBSD with dl_iterate_phdr() function */


#if !defined( MOON_DLFIX_FIND ) && \
    defined( MOON_DLFIX_DL_ITERATE_PHDR )
#include <string.h>

#ifndef MOON_DLFIX_LIBPREFIX
#define MOON_DLFIX_LIBPREFIX  "liblua"
#endif

static int moon_dlfix_cb( struct dl_phdr_info* info, size_t size,
                          void* data ) {
  int* found = data;
  void* dl = dlopen( info->dlpi_name, RTLD_LAZY );
  (void)size;
  MOON_DLFIX_DBG(("Checking ELF object '%s'.\n", info->dlpi_name));
  if( dl ) {
    if( dlsym( dl, "lua_gettop" ) ) {
      MOON_DLFIX_DBG(("'%s' does have Lua symbols.\n", info->dlpi_name));
      /* the Lua API could be in a dependency, so test the library
       * name for "liblua" */
      char const* libname = strrchr( info->dlpi_name, '/' );
      if( libname )
        ++libname; /* skip slash */
      else
        libname = info->dlpi_name;
      if( 0 == strncmp( libname, MOON_DLFIX_LIBPREFIX,
                        sizeof( MOON_DLFIX_LIBPREFIX )-1 ) ) {
        void* dl2 = dlopen( info->dlpi_name,
                            RTLD_LAZY|RTLD_GLOBAL|RTLD_NOLOAD );
        if( dl2 ) {
          MOON_DLFIX_DBG(("Found and fixed Lua SO!\n"));
          dlclose( dl2 );
          *found = 1;
        }
      }
    }
    dlclose( dl );
  }
  return *found;
}

static int moon_dlfix_find( void ) {
  int found = 0;
  MOON_DLFIX_DBG(("Iterating all loaded ELF objects ...\n"));
  return dl_iterate_phdr( moon_dlfix_cb, &found );
}
#define MOON_DLFIX_FIND()  (moon_dlfix_find())
#endif /* has dl_iterate_phdr() function() */

#if !defined( MOON_DLFIX_FIND )
/* dummy definition (always returns failure) */
#  define MOON_DLFIX_FIND()  (0)
#endif

/* Try to iterate all loaded shared libraries using a platform-
 * specific way to find a loaded Lua shared library.
 * If that fails, try a list of common library names.
 * In all cases reopen the Lua library using RTLD_GLOBAL and
 * RTLD_NOLOAD.
 */
#define MOON_DLFIX() \
  do { \
    if( !MOON_DLFIX_FIND() ) { \
      unsigned i = 0; \
      MOON_DLFIX_DBG(("Trying some common Lua library names ...\n")); \
      for( ; i < sizeof( moon_dlfix_lib_names )/sizeof( *moon_dlfix_lib_names ); ++i ) { \
        void* dl = dlopen( moon_dlfix_lib_names[ i ], RTLD_LAZY|RTLD_GLOBAL|RTLD_NOLOAD ); \
        MOON_DLFIX_DBG(("Trying '%s'.\n", moon_dlfix_lib_names[ i ])); \
        if( dl ) { \
          MOON_DLFIX_DBG(("Fixed Lua SO.\n")); \
          dlclose( dl ); \
          break; \
        } \
      } \
    } \
  } while( 0 )

#endif /* has RTLD_NOLOAD */
#endif /* has dlfcn.h */
#endif /* has unistd.h */

/* define fallback */
#ifndef MOON_DLFIX
#define MOON_DLFIX() \
  (MOON_DLFIX_DBG(("moon_dlfix functionality not available!\n")))
#endif

#endif /* MOON_DLFIX_H_ */

