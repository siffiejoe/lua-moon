/* Copyright 2013-2015 Philipp Janda <siffiejoe@gmx.net>
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

#define MOON_DLFIX() \
  do { \
    unsigned i = 0; \
    for( ; i < sizeof( moon_dlfix_lib_names )/sizeof( *moon_dlfix_lib_names ); ++i ) { \
      void* dl = dlopen( moon_dlfix_lib_names[ i ], RTLD_LAZY|RTLD_GLOBAL|RTLD_NOLOAD ); \
      if( dl ) { \
        dlclose( dl ); \
        break; \
      } \
    } \
  } while( 0 )

#endif /* has RTLD_NOLOAD */
#endif /* has dlfcn.h */
#endif /* has unistd.h */

/* define fallback */
#ifndef MOON_DLFIX
#define MOON_DLFIX()  ((void)0)
#endif

#endif /* MOON_DLFIX_H_ */

