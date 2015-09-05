/* Copyright 2013-2015 Philipp Janda <siffiejoe@gmx.net>
 *
 * You may do anything with this work that copyright law would normally
 * restrict, so long as you retain the above notice(s) and this license
 * in all redistributed copies and derived works.  There is no warranty.
 */

#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <limits.h>
#include <ctype.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"

/* don't compile it again if it's already included via moon.h */
#ifndef MOON_C_
#define MOON_C_


#if LUA_VERSION_NUM < 502
MOON_API int moon_absindex( lua_State* L, int i ) {
  if( i < 0 && i > LUA_REGISTRYINDEX )
    i += lua_gettop( L ) + 1;
  return i;
}
#endif


/* struct that is part of some moon objects (those created via
 * `moon_newfield`) and contains a function pointer and a data pointer
 * that can be used to check whether the object is still valid. The
 * check is done automatically in `moon_{check,test}object`, and is
 * primarily needed to safely support tagged unions. */
typedef struct moon_object_vcheck_ {
  int (*check)( void* );
  void* tagp;
  struct moon_object_vcheck_* next; /* linked list */
} moon_object_vcheck_;


typedef void (*moon_object_cleanup_)( void* );


/* For keeping memory consumption as low as possible multiple C
 * values might be stored side-by-side in the same memory block, and
 * we have to figure out the alignment to use for thos values. */
typedef union {
  lua_Number n;
  double d;
  lua_Integer i;
  long l;
  size_t s;
  void* p;
  void (*fp)( void );
} moon_object_alignment_u_;


/* Newer versions of C and C++ have a way to figure out alignment
 * built into the language, and we use those if available. There's
 * also a fallback implementation for older versions. */
#ifndef MOON_ALIGNOF_
#  if defined( __cplusplus ) && __cplusplus >= 201103L
/* alignof is an operator in newer C++ versions */
#    define MOON_ALIGNOF_( _t ) alignof( _t )
#  elif defined( __STDC_VERSION__ ) && __STDC_VERSION__ >= 201112L
/* newer C versions have it in stdalign.h */
#    include <stdalign.h>
#    define MOON_ALIGNOF_( _t ) alignof( _t )
#  else
/* Calculate the alignment ourselves. This may give smaller values
 * than using `sizeof( _t )` which is also a portable solution. */
#    define MOON_ALIGNOF_( _t ) \
  ((offsetof( struct { char _1; _t _2; }, _2 )) > sizeof( _t ) \
   ? sizeof( _t ) \
   : (offsetof( struct { char _1; _t _2; }, _2 )))
#  endif
#endif


#define MOON_OBJ_ALIGNMENT_ MOON_ALIGNOF_( moon_object_alignment_u_ )
#define MOON_PTR_ALIGNMENT_ MOON_ALIGNOF_( void* )
#define MOON_GCF_ALIGNMENT_ MOON_ALIGNOF_( moon_object_cleanup_ )
#define MOON_VCK_ALIGNMENT_ MOON_ALIGNOF_( moon_object_vcheck_ )
#define MOON_ROUNDTO_( _s, _a ) ((((_s)+(_a)-1)/(_a))*(_a))
#define MOON_PTR_( _p, _o ) ((void*)(((char*)(_p))+(_o)))


/* Raise properly formatted argument error messages. */
static int moon_type_error_( lua_State* L, int i, char const* t1,
                             char const* t2 ) {
  char const* msg = lua_pushfstring( L, "%s expected, got %s", t1, t2 );
  return luaL_argerror( L, i, msg );
}

static int moon_type_error_invalid_( lua_State* L, int i,
                                     char const* tname ) {
  char const* msg = lua_pushfstring( L, "invalid '%s' object", tname );
  return luaL_argerror( L, i, msg );
}

static int moon_type_error_version_( lua_State* L, int i ) {
  char const* msg = lua_pushfstring( L, "not a moon %d.%d object",
                                     MOON_VERSION_MAJOR,
                                     MOON_VERSION_MINOR );
  return luaL_argerror( L, i, msg );
}


/* Default implementation of a __tostring metamethod for moon objects
 * which displays the type name and memory address. */
static int moon_object_default_tostring_( lua_State* L ) {
  void* ptr = lua_touserdata( L, 1 );
  char const* name = lua_tostring( L, lua_upvalueindex( 1 ) );
  char const* basename = strrchr( name, '.' );
  if( basename != NULL )
    name = basename+1;
  lua_pushfstring( L, "[%s]: %p", name, ptr );
  return 1;
}


/* Run the destructor and mark the object as invalid/destroyed. */
static void moon_object_run_destructor_( moon_object_header* h ) {
  if( h->cleanup_offset > 0 && (h->flags & MOON_OBJECT_IS_VALID) ) {
    void* p = MOON_PTR_( h, h->object_offset );
    moon_object_cleanup_* gc = NULL;
    gc = (moon_object_cleanup_*)MOON_PTR_( h, h->cleanup_offset );
    if( h->flags & MOON_OBJECT_IS_POINTER )
      p = *((void**)p);
    if( *gc != 0 && p != NULL )
      (*gc)( p );
  }
  h->flags &= ~MOON_OBJECT_IS_VALID;
}


/* Common __gc metamethod for all moon objects. The actual finalizer
 * function is stored in the userdata to support different lifetimes.
 */
static int moon_object_default_gc_( lua_State* L ) {
  moon_object_header* h = (moon_object_header*)lua_touserdata( L, 1 );
  moon_object_run_destructor_( h );
  return 0;
}


/* Check the methods table for a given key and then (if unsuccessful)
 * call the registered C function for looking up properties. */
static int moon_index_dispatch_( lua_State* L ) {
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


/* Depending on the availability of a methods table and/or a C
 * function for looking up properties this function creates an __index
 * metamethod (function or table). */
static void moon_makeindex_( lua_State* L, luaL_Reg const methods[],
                             lua_CFunction pindex, int nups ) {
  if( methods != NULL ) {
    lua_newtable( L );
    for( ; methods->func; ++methods ) {
      int i = 0;
      if( methods->name[ 0 ] != '_' || methods->name[ 1 ] != '_' ) {
        for( i = 0; i < nups; ++i )
          lua_pushvalue( L, -nups-1 );
        lua_pushcclosure( L, methods->func, nups );
        lua_setfield( L, -2, methods->name );
      }
    }
    if( pindex ) {
      lua_pushcfunction( L, pindex );
      if( nups > 0 ) {
        lua_insert( L, -nups-2 );
        lua_insert( L, -nups-2 );
      }
      lua_pushcclosure( L, moon_index_dispatch_, 2+nups );
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


/* Type names used by the moon toolkit must not start with a double
 * underscore, because they might be used as keys in the metatable. */
static void moon_check_tname_( lua_State* L, char const* tname ) {
  if( tname == NULL || (tname[ 0 ] == '_' && tname[ 1 ] == '_') )
    luaL_error( L, "invalid type name: '%s'", tname ? tname : "NULL" );
}


MOON_API void moon_defobject( lua_State* L, char const* tname,
                              size_t sz, luaL_Reg const* methods,
                              int nups ) {
  int has_methods = 0;
  lua_CFunction index = 0;
  moon_check_tname_( L, tname );
  luaL_checkstack( L, 2*nups+4, "not enough stack space available" );
  /* we don't use luaL_newmetatable to make sure that we never have a
   * half-constructed metatable in the registry! */
  luaL_getmetatable( L, tname );
  if( !lua_isnil( L, -1 ) )
    luaL_error( L, "type '%s' is already defined", tname );
  lua_pop( L, 1 );
  lua_newtable( L );
  lua_pushstring( L, tname );
  lua_pushcclosure( L, moon_object_default_tostring_, 1 );
  lua_setfield( L, -2, "__tostring" );
  if( methods != NULL ) {
    luaL_Reg const* l = methods;
    int i = 0;
    for( ; l->func != NULL; ++l ) {
      if( l->name[ 0 ] == '_' && l->name[ 1 ] == '_' ) {
        if( 0 != strcmp( l->name+2, "index" ) ) {
          for( i = 0; i < nups; ++i )
            lua_pushvalue( L, -nups-1 );
          lua_pushcclosure( L, l->func, nups );
          lua_setfield( L, -2, l->name );
        } else /* handle __index later */
          index = l->func;
      } else
        has_methods = 1;
    }
  }
  if( has_methods || index ) {
    int i = 0;
    for( i = 0; i < nups; ++i )
      lua_pushvalue( L, -nups-1 );
    moon_makeindex_( L, has_methods ? methods : NULL, index, nups );
    lua_setfield( L, -2, "__index" );
  }
  lua_pushboolean( L, 0 );
  lua_setfield( L, -2, "__metatable" );
  lua_pushstring( L, tname );
  lua_setfield( L, -2, "__name" );
  lua_pushcfunction( L, moon_object_default_gc_ );
  lua_setfield( L, -2, "__gc" );
  lua_pushinteger( L, MOON_VERSION );
  lua_setfield( L, -2, "__moon_version" );
  lua_pushinteger( L, (lua_Integer)sz );
  lua_setfield( L, -2, "__moon_size" );
  lua_setfield( L, LUA_REGISTRYINDEX, tname );
  lua_pop( L, nups );
}


/* Pushes the metatable for the given type onto the Lua stack, and
 * makes sure that the given type is a moon object type. */
static void moon_check_metatable_( lua_State* L, char const* tname ) {
  moon_check_tname_( L, tname );
  luaL_getmetatable( L, tname );
  if( !lua_istable( L, -1 ) )
    luaL_error( L, "no metatable for type '%s' defined", tname );
  lua_getfield( L, -1, "__moon_version" );
  if( lua_tointeger( L, -1 ) != MOON_VERSION )
    luaL_error( L, "'%s' is not a moon %d.%d object type", tname,
                MOON_VERSION_MAJOR, MOON_VERSION_MINOR );
  lua_pop( L, 1 );
}


MOON_API void* moon_newobject( lua_State* L, char const* tname,
                               void (*gc)( void* ) ) {
  moon_object_header* obj = NULL;
  size_t off1 = 0;
  size_t off2 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                               MOON_OBJ_ALIGNMENT_ );
  size_t sz = 0;
  luaL_checkstack( L, 2, "not enough stack space available" );
  moon_check_metatable_( L, tname );
  lua_getfield( L, -1, "__moon_size" );
  sz = lua_tointeger( L, -1 );
  lua_pop( L, 1 );
  if( sz == 0 )
    luaL_error( L, "type '%s' is incomplete (size is 0)", tname );
  if( gc != 0 ) {
    off1 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                          MOON_GCF_ALIGNMENT_ );
    off2 = MOON_ROUNDTO_( off1 + sizeof( moon_object_cleanup_ ),
                          MOON_OBJ_ALIGNMENT_ );
  }
  obj = (moon_object_header*)lua_newuserdata( L, sz+off2 );
  if( off1 > 0 ) {
    moon_object_cleanup_* cl = NULL;
    cl = (moon_object_cleanup_*)MOON_PTR_( obj, off1 );
    *cl = gc;
  }
  obj->cleanup_offset = off1;
  obj->object_offset = off2;
  obj->vcheck_offset = 0;
  obj->flags = MOON_OBJECT_IS_VALID;
  lua_insert( L, -2 );
  lua_setmetatable( L, -2 );
  return MOON_PTR_( obj, off2 );
}


MOON_API void** moon_newpointer( lua_State* L, char const* tname,
                                 void (*gc)( void* ) ) {
  moon_object_header* obj = NULL;
  void** p = NULL;
  size_t off1 = 0;
  size_t off2 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                               MOON_PTR_ALIGNMENT_ );
  luaL_checkstack( L, 2, "not enough stack space available" );
  moon_check_metatable_( L, tname );
  if( gc != 0 ) {
    off1 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                          MOON_GCF_ALIGNMENT_ );
    off2 = MOON_ROUNDTO_( off1 + sizeof( moon_object_cleanup_ ),
                          MOON_PTR_ALIGNMENT_ );
  }
  obj = (moon_object_header*)lua_newuserdata( L, sizeof( void* )+off2 );
  p = (void**)MOON_PTR_( obj, off2 );
  *p = NULL;
  if( off1 > 0 ) {
    moon_object_cleanup_* cl = NULL;
    cl = (moon_object_cleanup_*)MOON_PTR_( obj, off1 );
    *cl = gc;
  }
  obj->cleanup_offset = off1;
  obj->object_offset = off2;
  obj->vcheck_offset = 0;
  obj->flags = MOON_OBJECT_IS_VALID | MOON_OBJECT_IS_POINTER;
  lua_insert( L, -2 );
  lua_setmetatable( L, -2 );
  return p;
}


MOON_API void** moon_newfield( lua_State* L, char const* tname,
                               int idx, int (*isvalid)( void* ),
                               void* tagp ) {
  moon_object_header* obj = NULL;
  moon_object_vcheck_* nextcheck = NULL;
  void** p = NULL;
  size_t off1 = 0;
  size_t off2 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                               MOON_PTR_ALIGNMENT_ );
  luaL_checkstack( L, 3, "not enough stack space available" );
  if( idx != 0 ) {
    moon_object_header* h = (moon_object_header*)lua_touserdata( L, idx );
    idx = moon_absindex( L, idx );
    if( h != NULL && lua_getmetatable( L, idx ) ) {
      lua_getfield( L, -1, "__moon_version" );
      if( lua_tointeger( L, -1 ) == MOON_VERSION ) {
        if( h->vcheck_offset > 0 ) {
          moon_object_vcheck_* vc = NULL;
          vc = (moon_object_vcheck_*)MOON_PTR_( h, h->vcheck_offset );
          if( isvalid == 0 ) { /* inherit vcheck from idx object */
            isvalid = vc->check;
            tagp = vc->tagp;
            nextcheck = vc->next;
          } else /* add it to the chain */
            nextcheck = vc;
        }
      }
      lua_pop( L, 2 );
    }
  }
  moon_check_metatable_( L, tname );
  if( isvalid != 0 ) {
    off1 = MOON_ROUNDTO_( sizeof( moon_object_header ),
                          MOON_VCK_ALIGNMENT_ );
    off2 = MOON_ROUNDTO_( off1 + sizeof( moon_object_vcheck_ ),
                          MOON_PTR_ALIGNMENT_ );
  }
  obj = (moon_object_header*)lua_newuserdata( L, sizeof( void* )+off2 );
  p = (void**)MOON_PTR_( obj, off2 );
  *p = NULL;
  obj->vcheck_offset = off1;
  obj->object_offset = off2;
  obj->cleanup_offset = 0;
  obj->flags = MOON_OBJECT_IS_VALID | MOON_OBJECT_IS_POINTER;
  if( off1 > 0 ) {
    moon_object_vcheck_* vc = NULL;
    vc= (moon_object_vcheck_*)MOON_PTR_( obj, off1 );
    vc->check = isvalid;
    vc->tagp = tagp;
    vc->next = nextcheck;
  }
  lua_insert( L, -2 );
  lua_setmetatable( L, -2 );
  if( idx != 0 ) {
    lua_newtable( L );
    lua_pushvalue( L, idx );
    lua_rawseti( L, -2, 1 );
#if LUA_VERSION_NUM < 502
    lua_setfenv( L, -2 );
#else
    lua_setuservalue( L, -2 );
#endif
  }
  return p;
}


MOON_API void moon_killobject( lua_State* L, int idx ) {
  moon_object_header* h = (moon_object_header*)lua_touserdata( L, idx );
  luaL_checkstack( L, 2, "not enough stack space available" );
  if( h == NULL || !lua_getmetatable( L, idx ) )
    moon_type_error_version_( L, idx );
  lua_getfield( L, -1, "__moon_version" );
  if( lua_tointeger( L, -1 ) != MOON_VERSION )
    moon_type_error_version_( L, idx );
  lua_pop( L, 2 );
  moon_object_run_destructor_( h );
}


MOON_API void moon_defcast( lua_State* L, char const* tname1,
                            char const* tname2,
                            moon_object_cast cast ) {
  luaL_checkstack( L, 2, "not enough stack space available" );
  moon_check_metatable_( L, tname1 );
  moon_check_tname_( L, tname2 );
  lua_pushcfunction( L, (lua_CFunction)cast );
  lua_setfield( L, -2, tname2 );
  lua_pop( L, 1 );
}


/* Validates a chain of vcheck objects. The chain won't be long, so
 * a recursive approach should be fine! */
static int moon_validate_vcheck_( moon_object_vcheck_ const* vc ) {
  if( vc->next != NULL )
    return moon_validate_vcheck_( vc->next ) &&
      (vc->check == 0 || vc->check( vc->tagp ));
  else
    return vc->check == 0 || vc->check( vc->tagp );
}


MOON_API void* moon_checkobject( lua_State* L, int idx,
                                 char const* tname ) {
  moon_object_header* h = (moon_object_header*)lua_touserdata( L, idx );
  void* p = NULL;
  int res = 0;
  moon_object_cast cast = 0;
  moon_check_tname_( L, tname );
  luaL_checkstack( L, 3, "not enough stack space available" );
  idx = moon_absindex( L, idx );
  if( h == NULL )
    moon_type_error_( L, idx, tname, luaL_typename( L, idx ) );
  if( lua_islightuserdata( L, idx ) )
    moon_type_error_( L, idx, tname, "lightuserdata" );
  if( !lua_getmetatable( L, idx ) )
    moon_type_error_( L, idx, tname, "userdata" );
  lua_getfield( L, -1, "__moon_version" );
  if( lua_tointeger( L, -1 ) != MOON_VERSION ) {
    lua_pop( L, 2 );
    moon_type_error_version_( L, idx );
  }
  lua_pop( L, 1 );
  luaL_getmetatable( L, tname );
  res = lua_rawequal( L, -1, -2 );
  lua_pop( L, 1 );
  if( !res ) {
    lua_getfield( L, -1, tname );
    cast = (moon_object_cast)lua_tocfunction( L, -1 );
    lua_pop( L, 1 );
    if( cast == 0 ) {
      char const* name = NULL;
      lua_getfield( L, -1, "__name" );
      name = lua_tostring( L, -1 );
      moon_type_error_( L, idx, tname, name ? name : "userdata" );
    }
  }
  lua_pop( L, 1 );
  if( !(h->flags & MOON_OBJECT_IS_VALID) )
    moon_type_error_invalid_( L, idx, tname );
  if( h->vcheck_offset > 0 ) {
    moon_object_vcheck_* vc = NULL;
    vc = (moon_object_vcheck_*)MOON_PTR_( h, h->vcheck_offset );
    if( !moon_validate_vcheck_( vc ) )
      moon_type_error_invalid_( L, idx, tname );
  }
  p = MOON_PTR_( h, h->object_offset );
  if( h->flags & MOON_OBJECT_IS_POINTER )
    p = *((void**)p);
  if( p == NULL )
    moon_type_error_invalid_( L, idx, tname );
  if( cast != 0 ) {
    p = cast( p );
    if( p == NULL )
      moon_type_error_invalid_( L, idx, tname );
  }
  return p;
}


MOON_API void* moon_testobject( lua_State* L, int idx,
                                char const* tname ) {
  moon_object_header* h = (moon_object_header*)lua_touserdata( L, idx );
  void* p = NULL;
  int res = 0;
  moon_object_cast cast = 0;
  moon_check_tname_( L, tname );
  luaL_checkstack( L, 2, "not enough stack space available" );
  if( h == NULL || !lua_getmetatable( L, idx ) )
    return NULL;
  lua_getfield( L, -1, "__moon_version" );
  if( lua_tointeger( L, -1 ) != MOON_VERSION ) {
    lua_pop( L, 2 );
    return NULL;
  }
  lua_pop( L, 1 );
  luaL_getmetatable( L, tname );
  res = lua_rawequal( L, -1, -2 );
  lua_pop( L, 1 );
  if( !res ) {
    lua_getfield( L, -1, tname );
    cast = (moon_object_cast)lua_tocfunction( L, -1 );
    lua_pop( L, 2 );
    if( cast == 0 )
      return NULL;
  } else
    lua_pop( L, 1 );
  if( !(h->flags & MOON_OBJECT_IS_VALID) )
    return NULL;
  if( h->vcheck_offset > 0 ) {
    moon_object_vcheck_* vc = NULL;
    vc = (moon_object_vcheck_*)MOON_PTR_( h, h->vcheck_offset );
    if( !moon_validate_vcheck_( vc ) )
      return NULL;
  }
  p = MOON_PTR_( h, h->object_offset );
  if( h->flags & MOON_OBJECT_IS_POINTER )
    p = *((void**)p);
  if( cast != 0 )
    p = cast( p );
  return p;
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


MOON_API int* moon_atexit( lua_State* L, lua_CFunction func ) {
  int* flag = NULL;
  luaL_checkstack( L, 3, "not enough stack space available" );
  flag = (int*)lua_newuserdata( L, sizeof( int ) );
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


MOON_API void moon_setuvfield( lua_State* L, int i, char const* key ) {
  luaL_checkstack( L, 2, "not enough stack space available" );
#if LUA_VERSION_NUM < 502
  lua_getfenv( L, i );
#else
  lua_getuservalue( L, i );
#endif
  if( !lua_istable( L, -1 ) )
    luaL_error( L, "attempt to add to non-table uservalue" );
  lua_pushvalue( L, -2 );
  lua_setfield( L, -2, key );
  lua_pop( L, 2 );
}


MOON_API int moon_getuvfield( lua_State* L, int i, char const* key ) {
  luaL_checkstack( L, 2, "not enough stack space available" );
#if LUA_VERSION_NUM < 502
  lua_getfenv( L, i );
#else
  lua_getuservalue( L, i );
#endif
  if( lua_istable( L, -1 ) ) {
    int t = 0;
    lua_getfield( L, -1, key );
    lua_replace( L, -2 );
    t = lua_type( L, -1 );
    if( t != LUA_TNIL )
      return t;
  }
  lua_pop( L, 1 );
  return LUA_TNIL;
}


MOON_API void moon_getcache( lua_State* L, int index ) {
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


#define MOON_SEP_ \
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
    fputs( MOON_SEP_ "\n", stderr );
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
        fputs( MOON_SEP_ "\n", stderr );
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
  fputs( MOON_SEP_ "\n", stderr );
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
#if LUA_VERSION_NUM < 502
          unsigned len = (unsigned)lua_objlen( L, i );
#else
          unsigned len = (unsigned)lua_rawlen( L, i );
#endif
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
#if LUA_VERSION_NUM < 502
                   (unsigned)lua_objlen( L, i ),
#else
                   (unsigned)lua_rawlen( L, i ),
#endif
                   lua_touserdata( L, i ) );
          lua_pop( L, 1 );
        } else {
          fprintf( stderr, "(size: %u, ptr: %p)\n",
#if LUA_VERSION_NUM < 502
                   (unsigned)lua_objlen( L, i ),
#else
                   (unsigned)lua_rawlen( L, i ),
#endif
                   lua_touserdata( L, i ) );
        }
        break;
      default: /* thread, table, function */
        fprintf( stderr, "(%p)\n", lua_topointer( L, i ) );
        break;
    }
  }
  fputs( MOON_SEP_ "\n", stderr );
}

#undef MOON_SEP_


/* clean up internally used macros */
#undef MOON_ALIGNOF_
#undef MOON_OBJ_ALIGNMENT_
#undef MOON_PTR_ALIGNMENT_
#undef MOON_GCF_ALIGNMENT_
#undef MOON_VCK_ALIGNMENT_
#undef MOON_ROUNDTO_
#undef MOON_PTR_

#endif /* MOON_C_ */

