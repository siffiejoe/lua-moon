#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


typedef struct {
  int x;
  int y;
} D;

typedef struct {
  D d;
} C;

typedef struct {
  double f;
} B;

#define TYPE_B 1
#define TYPE_C 2

typedef struct {
  int tag;
  union {
    B b;
    C c;
  } u;
} A;


static int type_b_check( void* p ) {
  int* tagp = p;
  int res = *tagp == TYPE_B;
  return res;
}

static int type_c_check( void* p ) {
  int* tagp = p;
  int res = *tagp == TYPE_C;
  return res;
}

static int object_valid_check( void* p ) {
  unsigned char* flagp = p;
  int res = *flagp & MOON_OBJECT_IS_VALID;
  return res;
}


static int A_index( lua_State* L ) {
  A* a = moon_checkobject( L, 1, "A" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  if( 0 == strcmp( key, "tag" ) ) {
    lua_pushstring( L, a->tag == TYPE_B ? "b" : "c" );
  } else if( 0 == strcmp( key, "b" ) && a->tag == TYPE_B ) {
    if( moon_getuvfield( L, 1, "b" ) == LUA_TNIL ) {
      /* use the uservalue table as a cache for the embedded userdata */
      void** p = moon_newfield( L, "B", 1, type_b_check, &(a->tag) );
      *p = &(a->u.b);
      lua_replace( L, -2 );
      lua_pushvalue( L, -1 );
      moon_setuvfield( L, 1, "b" );
    }
  } else if( 0 == strcmp( key, "c" ) && a->tag == TYPE_C ) {
    if( moon_getuvfield( L, 1, "c" ) == LUA_TNIL ) {
      /* use the uservalue table as a cache for the embedded userdata */
      void** p = moon_newfield( L, "C", 1, type_c_check, &(a->tag) );
      *p = &(a->u.c);
      lua_replace( L, -2 );
      lua_pushvalue( L, -1 );
      moon_setuvfield( L, 1, "c" );
    }
  } else
    lua_pushnil( L );
  return 1;
}


static int A_switch( lua_State* L ) {
  A* a = moon_checkobject( L, 1, "A" );
  lua_settop( L, 1 );
  if( a->tag == TYPE_B ) {
    a->tag = TYPE_C;
    a->u.c.d.x = 0;
    a->u.c.d.y = 0;
  } else {
    a->tag = TYPE_B;
    a->u.b.f = 0.0;
  }
  return 1;
}


static int A_printme( lua_State* L ) {
  A* a = moon_checkobject( L, 1, "A" );
  if( a->tag == TYPE_B )
    printf( "A { u.b.f = %g }\n", a->u.b.f );
  else
    printf( "A { u.c.d = { x = %d, y = %d } }\n",
            a->u.c.d.x, a->u.c.d.y );
  return 0;
}


static int B_index( lua_State* L ) {
  B* b = moon_checkobject( L, 1, "B" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  if( 0 == strcmp( key, "f" ) )
    lua_pushnumber( L, b->f );
  else
    lua_pushnil( L );
  return 1;
}


static int B_newindex( lua_State* L ) {
  B* b = moon_checkobject( L, 1, "B" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  if( 0 == strcmp( key, "f" ) )
    b->f = luaL_checknumber( L, 3 );
  return 0;
}


static int B_printme( lua_State* L ) {
  B* b = moon_checkobject( L, 1, "B" );
  printf( "B { f = %g }\n", b->f );
  return 0;
}


static int C_index( lua_State* L ) {
  C* c = moon_checkobject( L, 1, "C" );
  moon_object_header* h = lua_touserdata( L, 1 );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  printf( "__index C (uv1: %d, uv2: %d)\n",
          (int)lua_tointeger( L, lua_upvalueindex( 3 ) ),
          (int)lua_tointeger( L, lua_upvalueindex( 4 ) ) );
  if( 0 == strcmp( key, "d" ) ) {
    if( moon_getuvfield( L, 1, "d" ) == LUA_TNIL ) {
      /* use the uservalue table as a cache for the embedded userdata */
      void** p = moon_newfield( L, "D", 1, object_valid_check, &h->flags );
      *p = &(c->d);
      lua_replace( L, -2 );
      lua_pushvalue( L, -1 );
      moon_setuvfield( L, 1, "d" );
    }
  } else
    lua_pushnil( L );
  return 1;
}


static int C_newindex( lua_State* L ) {
  C* c = moon_checkobject( L, 1, "C" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  printf( "__newindex C (uv1: %d, uv2: %d)\n",
          (int)lua_tointeger( L, lua_upvalueindex( 1 ) ),
          (int)lua_tointeger( L, lua_upvalueindex( 2 ) ) );
  if( 0 == strcmp( key, "d" ) ) {
    D* d = moon_checkobject( L, 3, "D" );
    c->d = *d;
  }
  return 0;
}


static int C_close( lua_State* L ) {
  /* test moon_testobject; moon_checkobject would work better here! */
  if( !moon_testobject( L, 1, "C" ) )
    luaL_error( L, "need a 'C' object" );
  moon_killobject( L, 1 );
  return 0;
}


static int C_printme( lua_State* L ) {
  C* c = moon_checkobject( L, 1, "C" );
  printf( "C { d = { x = %d, y = %d } } (uv1: %d, uv2: %d)\n",
           c->d.x, c->d.y,
           (int)lua_tointeger( L, lua_upvalueindex( 1 ) ),
           (int)lua_tointeger( L, lua_upvalueindex( 2 ) ) );
  return 0;
}


static int D_index( lua_State* L ) {
  D* d = moon_checkobject( L, 1, "D" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  if( 0 == strcmp( key, "x" ) )
    lua_pushinteger( L, d->x );
  else if( 0 == strcmp( key, "y" ) )
    lua_pushinteger( L, d->y );
  else
    lua_pushnil( L );
  return 1;
}


static int D_newindex( lua_State* L ) {
  D* d = moon_checkobject( L, 1, "D" );
  size_t n = 0;
  char const* key = luaL_checklstring( L, 2, &n );
  if( 0 == strcmp( key, "x" ) )
    d->x = (int)moon_checkint( L, 3, INT_MIN, INT_MAX );
  else if( 0 == strcmp( key, "y" ) )
    d->y = (int)moon_checkint( L, 3, INT_MIN, INT_MAX );
  return 0;
}


static int D_printme( lua_State* L ) {
  D* d = moon_checkobject( L, 1, "D" );
  printf( "D { x = %d, y = %d }\n", d->x, d->y );
  return 0;
}


static int objex_newA( lua_State* L ) {
  A* ud = moon_newobject( L, "A", 0 );
  ud->tag = TYPE_B;
  ud->u.b.f = 0.0;
  /* we need a uservalue table as cache for embedded userdata
   * fields */
  lua_newtable( L );
#if LUA_VERSION_NUM < 502
  lua_setfenv( L, -2 );
#else
  lua_setuservalue( L, -2 );
#endif
  return 1;
}


static int objex_newB( lua_State* L ) {
  B* b = moon_newobject( L, "B", 0 );
  b->f = 0.0;
  return 1;
}


static void C_destructor( void* p ) {
  printf( "destroying C: %p\n", p );
}

static int objex_newC( lua_State* L ) {
  C* c = moon_newobject( L, "C", C_destructor );
  c->d.x = 0;
  c->d.y = 0;
  printf( "creating C: %p\n", (void*)c );
  lua_newtable( L );
#if LUA_VERSION_NUM < 502
  lua_setfenv( L, -2 );
#else
  lua_setuservalue( L, -2 );
#endif
  return 1;
}


static int objex_newD( lua_State* L ) {
  D* d = moon_newobject( L, "D", 0 );
  d->x = 0;
  d->y = 0;
  return 1;
}


static int objex_getD( lua_State* L ) {
  static D d = { 1, 2 };
  void** ud = moon_newpointer( L, "D", 0 );
  *ud = &d;
  return 1;
}


static D* newD( int x, int y ) {
  D* d = malloc( sizeof( *d ) );
  if( d ) {
    printf( "allocating D: %p\n", (void*)d );
    d->x = x;
    d->y = y;
  }
  return d;
}

static void freeD( void* d ) {
  printf( "deallocating D: %p\n", d );
  free( d );
}

static int objex_makeD( lua_State* L ) {
  int x = (int)moon_checkint( L, 1, INT_MIN, INT_MAX );
  int y = (int)moon_checkint( L, 2, INT_MIN, INT_MAX );
  void** p = moon_newpointer( L, "D", freeD );
  *p = newD( x, y );
  if( !*p )
    luaL_error( L, "memory allocation error" );
  return 1;
}


int luaopen_objex( lua_State* L ) {
  luaL_Reg const objex_funcs[] = {
    { "newA", objex_newA },
    { "newB", objex_newB },
    { "newC", objex_newC },
    { "newD", objex_newD },
    { "getD", objex_getD },
    { "makeD", objex_makeD },
    { NULL, NULL }
  };
  luaL_Reg const A_methods[] = {
    { "__index", A_index },
    { "switch", A_switch },
    { "printme", A_printme },
    { NULL, NULL }
  };
  luaL_Reg const B_methods[] = {
    { "__index", B_index },
    { "__newindex", B_newindex },
    { "printme", B_printme },
    { NULL, NULL }
  };
  luaL_Reg const C_methods[] = {
    { "__index", C_index },
    { "__newindex", C_newindex },
    { "printme", C_printme },
    { "close", C_close },
    { NULL, NULL }
  };
  luaL_Reg const D_methods[] = {
    { "__index", D_index },
    { "__newindex", D_newindex },
    { "printme", D_printme },
    { NULL, NULL }
  };
  moon_defobject( L, "A", sizeof( A ), A_methods, 0 );
  moon_defobject( L, "B", sizeof( B ), B_methods, 0 );
  lua_pushinteger( L, 1 );
  lua_pushinteger( L, 2 );
  moon_defobject( L, "C", sizeof( C ), C_methods, 2 );
  moon_defobject( L, "D", sizeof( D ), D_methods, 0 );
#if LUA_VERSION_NUM < 502
  luaL_register( L, "objex", objex_funcs );
#else
  luaL_newlib( L, objex_funcs );
#endif
  return 1;
}


