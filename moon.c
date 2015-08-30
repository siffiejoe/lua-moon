/* Copyright 2013,2014 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

#include <stddef.h>
#include <string.h>
#include <limits.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"

/* don't compile it again if it's already included via moon.h */
#ifndef MOON_C_
#define MOON_C_


#ifndef MOON_PRIVATE_KEY
#  define MOON_PRIVATE_KEY  "__moon"
#endif


#if LUA_VERSION_NUM < 502
MOON_API int moon_absindex( lua_State* L, int i ) {
  if( i < 0 && i > LUA_REGISTRYINDEX )
    i += lua_gettop( L ) + 1;
  return i;
}
#endif


typedef struct {
  size_t size;
  void (*initializer)( void* );
} moon_object_info;


static int moon_object_default_tostring( lua_State* L ) {
  void* ptr = lua_touserdata( L, 1 );
  char const* name = luaL_checkstring( L, lua_upvalueindex( 1 ) );
  lua_pushfstring( L, "[%s]: %p", name, ptr );
  return 1;
}


MOON_API void moon_defobject( lua_State* L,
                              moon_object_type const* t,
                              int nups ) {
  lua_CFunction index = 0;
  moon_object_info* info = NULL;
  luaL_checkstack( L, 2*nups+4, "not enough stack space available" );
  if( 0 == luaL_newmetatable( L, t->metatable_name ) )
    luaL_error( L, "type '%s' is already defined", t->metatable_name );
  lua_pushboolean( L, 0 );
  lua_setfield( L, -2, "__metatable" );
#if LUA_VERSION_NUM < 503
  lua_pushstring( L, t->metatable_name );
  lua_setfield( L, -2, "__name" );
#endif
  lua_pushstring( L, t->metatable_name );
  lua_pushcclosure( L, moon_object_default_tostring, 1 );
  lua_setfield( L, -2, "__tostring" );
  if( t->metamethods != NULL ) {
    luaL_Reg const* l = t->metamethods;
    int i = 0;
    for( ; l->name != NULL; ++l ) {
      if( 0 == strcmp( l->name, MOON_PRIVATE_KEY ) ) {
        luaL_error( L, "'%s' is reserved", MOON_PRIVATE_KEY );
      } else if( 0 != strcmp( l->name, "__index" ) ) {
        for( i = 0; i < nups; ++i )
          lua_pushvalue( L, -nups-1 );
        lua_pushcclosure( L, l->func, nups );
        lua_setfield( L, -2, l->name );
      } else /* handle __index later */
        index = l->func;
    }
  }
  if( t->methods != NULL || index ) {
    int i = 0;
    for( i = 0; i < nups; ++i )
      lua_pushvalue( L, -nups-1 );
    moon_propindex( L, t->methods, index, nups );
    lua_setfield( L, -2, "__index" );
  }
  info = lua_newuserdata( L, sizeof( moon_object_info ) );
  info->size = t->userdata_size;
  info->initializer = t->initializer;
  lua_setfield( L, -2, MOON_PRIVATE_KEY );
  lua_pop( L, nups+1 );
}


MOON_API void* moon_newobject( lua_State* L, char const* name,
                               int env_index ) {
  moon_object_info* info = NULL;
  void* obj = NULL;
  luaL_checkstack( L, 5, "not enough stack space available" );
  if( env_index != 0 )
    env_index = moon_absindex( L, env_index );
  luaL_getmetatable( L, name );
  if( !lua_istable( L, -1 ) )
    luaL_error( L, "no metatable for type '%s' defined", name );
  lua_getfield( L, -1, MOON_PRIVATE_KEY );
  info = lua_touserdata( L, -1 );
  if( info == NULL )
    luaL_error( L, "'%s' is not a moon object type", name );
  obj = lua_newuserdata( L, info->size );
  if( info->initializer )
    info->initializer( obj );
  if( env_index != 0 ) {
    lua_pushvalue( L, env_index );
    moon_setuservalue( L, -2 );
  }
  lua_insert( L, -3 );
  lua_pop( L, 1 );
  lua_setmetatable( L, -2 );
  return obj;
}


MOON_API void* moon_newobject_ref( lua_State* L, char const* name,
                                   int ref_index ) {
  void* ptr = NULL;
  luaL_checkstack( L, 6, "not enough stack space available" );
  if( ref_index != 0 ) {
    ref_index = moon_absindex( L, ref_index );
    /* build env table */
    lua_createtable( L, 0, 1 );
    lua_pushvalue( L, ref_index );
    lua_pushboolean( L, 1 );
    lua_rawset( L, -3 );
  }
  ptr = moon_newobject( L, name, ref_index != 0 ? -1 : 0 );
  if( ref_index != 0 )
    lua_replace( L, -2 );
  return ptr;
}


static int moon_dispatch( lua_State* L ) {
  lua_CFunction pindex;
  /* try method table first */
  lua_pushvalue( L, 2 ); /* duplicate key */
  lua_rawget( L, lua_upvalueindex( 1 ) );
  if( !lua_isnil( L, -1 ) )
    return 1;
  lua_pop( L, 1 );
  pindex = lua_tocfunction( L, lua_upvalueindex( 2 ) );
  return pindex( L );
}


MOON_API void moon_propindex( lua_State* L, luaL_Reg const methods[],
                              lua_CFunction pindex, int nups ) {
  if( methods != NULL ) {
    luaL_checkstack( L, nups+2, "not enough stack space available" );
    lua_newtable( L );
    for( ; methods->func; ++methods ) {
      int i = 0;
      for( i = 0; i < nups; ++i )
        lua_pushvalue( L, -nups-1 );
      lua_pushcclosure( L, methods->func, nups );
      lua_setfield( L, -2, methods->name );
    }
    if( pindex ) {
      lua_pushcfunction( L, pindex );
      if( nups > 0 ) {
        lua_insert( L, -nups-2 );
        lua_insert( L, -nups-2 );
      }
      lua_pushcclosure( L, moon_dispatch, 2+nups );
    } else if( nups > 0 ) {
      lua_replace( L, -nups-1 );
      lua_pop( L, nups-1 );
    }
  } else if( pindex ) {
    lua_pushcclosure( L, pindex, nups );
  } else {
    lua_pop( L, nups );
    lua_pushnil( L );
  }
}


#if LUA_VERSION_NUM < 502
MOON_API void* moon_testudata( lua_State* L, int i, char const* tname ) {
  void* p = lua_touserdata( L, i );
  if( p == NULL || !lua_getmetatable( L, i ) )
    return NULL;
  else {
    int res = 0;
    luaL_getmetatable( L, tname );
    res = lua_rawequal( L, -1, -2 );
    lua_pop( L, 2 );
    if( !res )
      p = NULL;
  }
  return p;
}
#endif


#if LUA_VERSION_NUM < 503
static int moon_type_error( lua_State* L, int i, char const* t1,
                            char const* t2 ) {
  char const* msg = lua_pushfstring( L, "%s expected, got %s", t1, t2 );
  return luaL_argerror( L, i, msg );
}

MOON_API void* moon_checkudata( lua_State* L, int i, char const* tname ) {
  void* p = lua_touserdata( L, i );
  if( p == NULL || !lua_getmetatable( L, i ) ) {
    moon_type_error( L, i, tname, luaL_typename( L, i ) );
  } else {
    luaL_getmetatable( L, tname );
    if( !lua_rawequal( L, -1, -2 ) ) {
      char const* t2 = luaL_typename( L, i );
      lua_getfield( L, -2, "__name" );
      if( lua_isstring( L, -1 ) )
        t2 = lua_tostring( L, -1 );
      moon_type_error( L, i, tname, t2 );
    }
    lua_pop( L, 2 );
  }
  return p;
}
#endif


MOON_API int* moon_atexit( lua_State* L, lua_CFunction func ) {
  int* flag = NULL;
  luaL_checkstack( L, 3, "not enough stack space available" );
  flag = lua_newuserdata( L, sizeof( int ) );
  *flag = 0;
  /* setmetatable( flag, { __gc = func } ) */
  lua_newtable( L );
  lua_pushcfunction( L, func );
  lua_setfield( L, -2, "__gc" );
  lua_setmetatable( L, -2 );
  /* registry[ flag ] = flag */
  lua_pushvalue( L, -1 );
  lua_pushvalue( L, -2 );
  lua_settable( L, LUA_REGISTRYINDEX );
  return flag;
}


static int moon_finalizer_gc( lua_State* L ) {
  int* flag = lua_touserdata( L, 1 );
  if( *flag ) {
    lua_getmetatable( L, 1 );
    lua_rawgeti( L, -1, 1 );
    if( luaL_getmetafield( L, -1, "__gc" ) &&
        lua_type( L, -1 ) == LUA_TFUNCTION ) {
      int err = 0;
      lua_pushvalue( L, -2 );
      err = lua_pcall( L, 1, 1, 0 );
      lua_pushnil( L );
      lua_setmetatable( L, -3 );
      if( err )
        lua_error( L );
    }
  }
  return 0;
}


MOON_API void moon_finalizer( lua_State* L, int idx ) {
  int* flag = NULL;
  idx = moon_absindex( L, idx );
  luaL_argcheck( L, lua_type( L, idx ) == LUA_TUSERDATA, idx,
                 "userdata expected" );
  luaL_checkstack( L, 3, "not enough stack space available" );
  flag = lua_newuserdata( L, sizeof( int ) );
  *flag = 0;
  lua_createtable( L, 1, 1 );
  lua_pushvalue( L, idx );
  lua_rawseti( L, -2, 1 );
  lua_pushcfunction( L, moon_finalizer_gc );
  lua_setfield( L, -2, "__gc" );
  lua_setmetatable( L, -2 );
  moon_getuvfield( L, idx, MOON_PRIVATE_KEY );
  lua_pushvalue( L, -2 );
  moon_setuvfield( L, idx, MOON_PRIVATE_KEY );
  if( lua_type( L, -1 ) == LUA_TUSERDATA )
    *(int*)lua_touserdata( L, -1 ) = 0;
  *flag = 1;
  lua_pop( L, 2 );
}


typedef struct {
  void* ptr;
  void (*releasef)( void* );
} moon_resource_info;


static int moon_resource_gc( lua_State* L ) {
  void* ptr = lua_touserdata( L, 1 );
  moon_release( ptr );
  return 0;
}


MOON_API void** moon_resource( lua_State* L,
                               void (*releasef)( void* ) ) {
  moon_resource_info* info = NULL;
  luaL_checkstack( L, 3, "not enough stack space available" );
  info = lua_newuserdata( L, sizeof( *info ) );
  info->ptr = NULL;
  info->releasef = releasef;
  if( luaL_newmetatable( L, MOON_PRIVATE_KEY ) ) {
    lua_pushcfunction( L, moon_resource_gc );
    lua_setfield( L, -2, "__gc" );
  }
  lua_setmetatable( L, -2 );
  return &info->ptr;
}


MOON_API void moon_release( void** ptr ) {
  moon_resource_info* info = (moon_resource_info*)ptr;
  void* p = info->ptr;
  info->ptr = NULL;
  if( p )
    info->releasef( p );
}


MOON_API void moon_preload_c( lua_State* L, luaL_Reg const libs[] ) {
  int top = lua_gettop( L );
  luaL_checkstack( L, 3, "not enough stack space available" );
#if LUA_VERSION_NUM < 502
  lua_getglobal( L, "package" );
  lua_getfield( L, -1, "preload" );
#else
  lua_getfield( L, LUA_REGISTRYINDEX, "_PRELOAD" );
#endif
  for( ; libs->func; libs++ ) {
    lua_pushcfunction( L, libs->func );
    lua_setfield( L, -2, libs->name );
  }
  lua_settop( L, top );
}


MOON_API void moon_preload_lua( lua_State* L, moon_lua_reg const libs[] ) {
  int top = lua_gettop( L );
  luaL_checkstack( L, 3, "not enough stack space available" );
#if LUA_VERSION_NUM < 502
  lua_getglobal( L, "package" );
  lua_getfield( L, -1, "preload" );
#else
  lua_getfield( L, LUA_REGISTRYINDEX, "_PRELOAD" );
#endif
  for( ; libs->code; libs++ ) {
    if( 0 != luaL_loadbuffer( L, libs->code, libs->code_length,
                              libs->error_name ) )
      lua_error( L );
    lua_setfield( L, -2, libs->require_name );
  }
  lua_settop( L, top );
}


MOON_API void moon_setuvfield( lua_State* L, int i, char const* key ) {
  luaL_checkstack( L, 2, "not enough stack space available" );
  moon_getuservalue( L, i );
  if( lua_type( L, -1 ) != LUA_TTABLE )
    luaL_error( L, "attempt to add to non-table uservalue" );
  lua_pushvalue( L, -2 );
  lua_setfield( L, -2, key );
  lua_pop( L, 2 );
}


MOON_API int moon_getuvfield( lua_State* L, int i, char const* key ) {
  luaL_checkstack( L, 2, "not enough stack space available" );
  moon_getuservalue( L, i );
  if( lua_type( L, -1 ) == LUA_TTABLE )
    lua_getfield( L, -1, key );
  else
    lua_pushnil( L );
  lua_replace( L, -2 );
  return lua_type( L, -1 );
}


MOON_API void moon_light2full( lua_State* L, int index ) {
  static char xyz = 0; /* used as a unique key */
  luaL_checkstack( L, 3, "not enough stack space available" );
  index = moon_absindex( L, index );
  lua_pushlightuserdata( L, &xyz );
  lua_rawget( L, index );
  if( !lua_istable( L, -1 ) ) {
    lua_pop( L, 1 );
    lua_createtable( L, 0, 1 );
    lua_pushliteral( L, "__mode" );
    lua_pushliteral( L, "v" );
    lua_rawset( L, -3 );
    lua_pushvalue( L, -1 );
    lua_setmetatable( L, -2 );
    lua_pushlightuserdata( L, &xyz );
    lua_pushvalue( L, -2 );
    lua_rawset( L, index );
  }
}


MOON_API void moon_lookuptable( lua_State* L,
                                char const* const names[],
                                lua_Integer const values[] ) {
  size_t idx = 0;
  luaL_checkstack( L, 5, "not enough stack space available" );
  lua_newtable( L );
  for( idx = 0; names[ idx ] != NULL; ++idx ) {
    lua_pushstring( L, names[ idx ] );
    lua_pushinteger( L, values[ idx ] );
    lua_pushvalue( L, -1 );
    lua_pushvalue( L, -3 );
    lua_rawset( L, -5 );
    lua_rawset( L, -3 );
  }
}


MOON_API void moon_pushoption( lua_State* L, lua_Integer val,
                               lua_Integer const values[],
                               char const* const names[],
                               int lookupindex ) {
  if( lookupindex == 0 ) {
    size_t idx = 0;
    while( names[ idx ] != NULL ) {
      if( values[ idx ] == val ) {
        lua_pushstring( L, names[ idx ] );
        return;
      }
      ++idx;
    }
  } else {
    lookupindex = moon_absindex( L, lookupindex );
    lua_pushinteger( L, val );
    lua_rawget( L, lookupindex );
    if( !lua_isnil( L, -1 ) )
      return;
    lua_pop( L, 1 );
  }
  lua_pushinteger( L, val );
}


MOON_API lua_Integer moon_checkoption( lua_State* L, int idx,
                                       char const* def,
                                       char const* const names[],
                                       lua_Integer const values[],
                                       int lookupindex ) {
  if( lookupindex == 0 ) {
    int i = luaL_checkoption( L, idx, def, names );
    return values[ i ];
  } else {
    lua_Integer val = 0;
    char const* name = def;
    lookupindex = moon_absindex( L, lookupindex );
    idx = moon_absindex( L, idx );
    if( lua_isnoneornil( L, idx ) && def != NULL )
      lua_pushstring( L, def );
    else {
      name = luaL_checkstring( L, idx );
      lua_pushvalue( L, idx );
    }
    lua_rawget( L, lookupindex );
    if( lua_type( L, -1 ) == LUA_TNUMBER ) {
      val = lua_tointeger( L, -1 );
      lua_pop( L, 1 );
    } else
      luaL_argerror( L, idx, lua_pushfstring( L,
                     "invalid option '%s'", name ) );
    return val;
  }
}


MOON_API lua_Integer moon_checkint( lua_State* L, int idx,
                                    lua_Integer low,
                                    lua_Integer high ) {
  lua_Integer v = luaL_checkinteger( L, idx );
  int valid = (high < low) ? (low <= v || v <= high)
                           : (low <= v && v <= high);
  if( !valid ) {
    char const* msg = NULL;
#if LUA_VERSION_NUM >= 503
    msg = lua_pushfstring( L, "value out of range [%I,%I]", low, high );
#else
    if( low >= INT_MIN && low <= INT_MAX &&
        high >= INT_MIN && high <= INT_MAX )
      msg = lua_pushfstring( L, "value out of range [%d,%d]",
                             (int)low, (int)high );
    else
      msg = lua_pushfstring( L, "value out of range [%f,%f]",
                             (lua_Number)low, (lua_Number)high );
#endif
    luaL_argerror( L, idx, msg );
  }
  return v;
}


MOON_API lua_Integer moon_optint( lua_State* L, int idx,
                                  lua_Integer low, lua_Integer high,
                                  lua_Integer def ) {
  if( !lua_isnoneornil( L, idx ) )
    def = moon_checkint( L, idx, low, high );
  return def;
}


MOON_API void* moon_checkarray( lua_State* L, int idx,
                                void* buffer, size_t* nelems, size_t esize,
                                int (*assignfn)(lua_State*, int i, void*),
                                int extra ) {
  char const* msg = "invalid array element";
  size_t len = 0;
  size_t i = 1;
  luaL_checkstack( L, 3, "not enough stack space available" );
  idx = moon_absindex( L, idx );
  if( lua_istable( L, idx ) ) {
    int top;
    len = moon_rawlen( L, idx );
    if( !buffer || len > *nelems )
      buffer = lua_newuserdata( L, len*esize );
    top = lua_gettop( L );
    for( i = 1; i <= len; ++i ) {
      lua_rawgeti( L, idx, i );
      if( !assignfn( L, top+1, (char*)buffer+(i-1)*esize ) ) {
        msg = lua_pushfstring( L, "array element no %d invalid", (int)i );
        luaL_argerror( L, idx, msg );
      }
      lua_settop( L, top );
    }
  } else {
    int top = lua_gettop( L );
    if( top >= idx + extra )
      len = top - idx - extra + 1;
    if( !buffer || len > *nelems )
      buffer = lua_newuserdata( L, len*esize );
    for( i = 0; i < len; ++i )
      if( !assignfn( L, idx+i, (char*)buffer+i*esize ) )
        luaL_argerror( L, idx+i, msg );
  }
  *nelems = len;
  return buffer;
}


#ifndef NDEBUG
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

#define SEP \
  "######################################################################"


MOON_API void moon_stack_assert_( lua_State* L, char const* file,
                                  int line, char const* func, ... ) {
  int top = lua_gettop( L );
  int nargs = 0;
  int skip = 0;
  int have_error = 0;
  int i = 0;
  char const* arg;
  va_list ap;
  va_start( ap, func );
  while( (arg = va_arg( ap, char const* )) != NULL )
    ++nargs;
  va_end( ap );
  if( nargs > top ) {
    fputs( SEP "\n", stderr );
    fprintf( stderr, "Lua stack assertion failed:"
             " less stack slots (%d) than expected (%d)!\n",
             top, nargs );
    have_error = 1;
    skip = nargs - top;
  }
  va_start( ap, func );
  for( i = 0; i < skip; ++i )
    arg = va_arg( ap, char const* );
  for( i = top+skip-nargs+1; i <= top; ++i ) {
    char const* tn = NULL;
    arg = va_arg( ap, char const* );
    if( !(arg[ 0 ] == '*' && arg[ 1 ] == '\0') &&
        arg != (tn=luaL_typename( L, i )) &&
        0 != strcmp( arg, tn ) ) {
      if( !have_error )
        fputs( SEP "\n", stderr );
      fprintf( stderr, "Lua stack assertion failed:"
               " '%s' at slot %d (expected '%s')!\n",
               tn, i, arg );
      have_error = 1;
    }
  }
  va_end( ap );
  if( have_error ) {
    moon_stack_( L, file, line, func );
    luaL_error( L, "Lua stack assertion failed!" );
  }
}


MOON_API void moon_stack_( lua_State* L, char const* file, int line,
                           char const* func ) {
  int n = lua_gettop( L );
  int i = 0;
  fputs( SEP "\n", stderr );
  fprintf( stderr, "Lua stack in function '%s' at '%s:%d':\n",
           func != NULL ? func : "(unknown)", file, line );
  for( i = 1; i <= n; ++i ) {
    fprintf( stderr, "%02d: %s ", i, luaL_typename( L,  i ) );
    switch( lua_type( L, i ) ) {
      case LUA_TNONE:
        fputs( "(none)\n", stderr );
        break;
      case LUA_TNIL:
        fputs( "(nil)\n", stderr );
        break;
      case LUA_TBOOLEAN:
        fprintf( stderr, "(%s)\n", lua_toboolean( L, i ) ? "true"
                                                         : "false" );
        break;
      case LUA_TNUMBER:
#if LUA_VERSION_NUM >= 503
        if( lua_isinteger( L, i ) )
          fprintf( stderr, "(" LUA_INTEGER_FMT ")\n",
                   lua_tointeger( L, i ) );
        else
#endif
          fprintf( stderr, "(" LUA_NUMBER_FMT ")\n",
                   lua_tonumber( L, i ) );
        break;
      case LUA_TSTRING: {
          char const* str = lua_tostring( L, i );
          char const* ellipsis = "";
          unsigned len = (unsigned)moon_rawlen( L, i );
          unsigned i = 0;
          fprintf( stderr, "(len: %u, str: \"", len );
          if( len > 15 ) {
            len = 15;
            ellipsis = "...";
          }
          for( i = 0; i < len; ++i ) {
            if( isprint( (unsigned char)str[ i ] ) )
              putc( (unsigned char)str[ i ], stderr );
            else
              fprintf( stderr, "\\x%02X", (unsigned)str[ i ] );
          }
          fputs( ellipsis, stderr );
          fputs( "\")\n", stderr );
          break;
        }
      case LUA_TLIGHTUSERDATA:
        fprintf( stderr, "(%p)\n", lua_touserdata( L, i ) );
        break;
      case LUA_TUSERDATA:
        if( luaL_getmetafield( L, i, "__name" ) ) {
          char const* name = lua_tostring( L, -1 );
          fprintf( stderr, "(type: \"%s\", size: %u, ptr: %p)\n",
                   name != NULL ? name : "(NULL)",
                   (unsigned)moon_rawlen( L, i ),
                   lua_touserdata( L, i ) );
          lua_pop( L, 1 );
        } else {
          fprintf( stderr, "(size: %u, ptr: %p)\n",
                   (unsigned)moon_rawlen( L, i ),
                   lua_touserdata( L, i ) );
        }
        break;
      default: /* thread, table, function */
        fprintf( stderr, "(%p)\n", lua_topointer( L, i ) );
        break;
    }
  }
  fputs( SEP "\n", stderr );
}

#undef SEP

#endif


#endif /* MOON_C_ */

