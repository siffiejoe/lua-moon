/*
 * Example code for userdata handling in the moon toolkit.
 *
 * The moon toolkits provides the following functions for handling
 * userdata in an easy and safe way:
 * -   moon_defobject
 * -   moon_newobject
 * -   moon_newpointer
 * -   moon_newfield
 * -   moon_killobject
 * -   moon_checkobject
 * -   moon_testobject
 * -   moon_defcast
 *
 * Using those functions enables you to
 * -   Create and register a new metatable for a C type in a single
 *     function call, including methods and metamethods with upvalues.
 * -   Have properties and methods for an object at the same time.
 * -   Use C values and pointers to those values in a uniform way.
 * -   Support multiple different ways to create/destroy a C type.
 * -   Expose fields embedded in another object in a type-safe way.
 * -   Bind tagged unions in a type-safe way.
 * -   Define functions that release resources in a safe way before
 *     the object becomes unreachable.
 * -   Use userdata polymorphically (use one method implementation for
 *     multiple similar types).
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <lua.h>
#include <lauxlib.h>
#include "moon.h"


/* Types to be exposed to Lua: */
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


/* Check functions to make sure that embedded userdata are still
 * valid. A change in the tagged union (A_switch) or running a cleanup
 * function (C_close) can make embedded userdata invalid. */
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


/* Types that are similar can share one method implementation, but a
 * pointer to one type has to be transformed to a pointer to the other
 * type. This is probably much more useful when binding C++ libraries
 * with proper type hierarchies! */
static void* C_to_D( void* p ) {
  C* c = p;
  printf( "casting C to D\n" );
  return &(c->d);
}



/* The (meta-)methods are pretty straightforward. Just use
 * `moon_checkobject` instead of `luaL_checkudata`. */
static int A_index( lua_State* L ) {
  A* a = moon_checkobject( L, 1, "A" );
  char const* key = luaL_checkstring( L, 2 );
  if( 0 == strcmp( key, "tag" ) ) {
    lua_pushstring( L, a->tag == TYPE_B ? "b" : "c" );
  } else if( 0 == strcmp( key, "b" ) && a->tag == TYPE_B ) {
    /* To avoid creating the sub-userdata on every __index access, the
     * userdata values are cached in the uservalue field of the parent
     * using the same field name as in the struct. */
    if( moon_getuvfield( L, 1, "b" ) == LUA_TNIL ) {
      /* Create a new userdata that represents a field in another
       * object already exposed to Lua. A gc function is unnecessary
       * since the parent userdata already takes care of that.
       * However, the parent userdata must be kept alive long enough
       * (by putting the value at index 1 into the uservalue table of
       * the new userdata), and the new userdata may only be used as
       * long as the `a->tag` field satisfies the `type_b_check`
       * function! */
      void** p = moon_newfield( L, "B", 1, type_b_check, &(a->tag) );
      /* The userdata stores a pointer to the `a->b` field. */
      *p = &(a->u.b);
      lua_pushvalue( L, -1 );
      moon_setuvfield( L, 1, "b" );
    }
  } else if( 0 == strcmp( key, "c" ) && a->tag == TYPE_C ) {
    if( moon_getuvfield( L, 1, "c" ) == LUA_TNIL ) {
      void** p = moon_newfield( L, "C", 1, type_c_check, &(a->tag) );
      *p = &(a->u.c);
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
  char const* key = luaL_checkstring( L, 2 );
  if( 0 == strcmp( key, "f" ) )
    lua_pushnumber( L, b->f );
  else
    lua_pushnil( L );
  return 1;
}


static int B_newindex( lua_State* L ) {
  B* b = moon_checkobject( L, 1, "B" );
  char const* key = luaL_checkstring( L, 2 );
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
  /* You can get a pointer to the `moon_object_header` structure
   * common to all objects created via the moon toolkit by using the
   * normal `lua_touserdata` function: */
  moon_object_header* h = lua_touserdata( L, 1 );
  char const* key = luaL_checkstring( L, 2 );
  /* The C object type has been/will be registered with two additional
   * upvalues. Since the type has a custom `__index` metamethod *and*
   * normal methods, those upvalues can be found at index 3 and 4. */
  printf( "__index C (uv1: %d, uv2: %d)\n",
          (int)lua_tointeger( L, lua_upvalueindex( 3 ) ),
          (int)lua_tointeger( L, lua_upvalueindex( 4 ) ) );
  if( 0 == strcmp( key, "d" ) ) {
    if( moon_getuvfield( L, 1, "d" ) == LUA_TNIL ) {
      /* The `object_valid_check` makes sure that the parent object
       * has the `MOON_OBJECT_IS_VALID` flag set. This flag is reset
       * by the `moon_killobject` function. */
      void** p = moon_newfield( L, "D", 1, object_valid_check, &h->flags );
      *p = &(c->d);
      lua_pushvalue( L, -1 );
      moon_setuvfield( L, 1, "d" );
    }
  } else
    lua_pushnil( L );
  return 1;
}


static int C_newindex( lua_State* L ) {
  C* c = moon_checkobject( L, 1, "C" );
  char const* key = luaL_checkstring( L, 2 );
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
  /* Use `moon_testobject` here to test it. `moon_checkobject` would
   * make more sense in practice! */
  if( !moon_testobject( L, 1, "C" ) )
    luaL_error( L, "need a 'C' object" );
  /* Run cleanup function (if any) to release resources and mark the
   * C object as invalid. `moon_checkobject` will raise an error for
   * invalid objects. */
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
  char const* key = luaL_checkstring( L, 2 );
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
  char const* key = luaL_checkstring( L, 2 );
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


static int objex_getAmethods( lua_State* L ) {
  if( moon_getmethods( L, "A" ) == LUA_TNIL )
    lua_pushnil( L );
  return 1;
}


static int objex_newA( lua_State* L ) {
  /* Create a new A object. The memory is allocated inside the
   * userdata when using `moon_newobject`. Here no cleanup function
   * is used (0 pointer). */
  A* ud = moon_newobject( L, "A", 0 );
  ud->tag = TYPE_B;
  ud->u.b.f = 0.0;
  /* `moon_newobject`, and `moon_newpointer` don't allocate a
   * uservalue table by default. `moon_newfield` only does if the
   * given index is non-zero. If you need a uservalue table (e.g. to
   * cache embedded userdatas), you have to add one yourself: */
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
  /* Create a C object and register a dummy cleanup function (just
   * for tests): */
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
  /* `moon_newpointer` without a cleanup function can be used to
   * expose a global variable to Lua: */
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
  /* Usually, `moon_newpointer` is used when your API handles memory
   * allocation and deallocation for its types: */
  void** p = moon_newpointer( L, "D", freeD );
  /* The cleanup function will only run if the pointer is set to a
   * non-NULL value. `moon_checkobject` also will raise an error when
   * passed a NULL object! */
  *p = newD( x, y );
  if( !*p )
    luaL_error( L, "memory allocation error" );
  return 1;
}


int luaopen_objex( lua_State* L ) {
  luaL_Reg const objex_funcs[] = {
    { "getAmethods", objex_getAmethods },
    { "newA", objex_newA },
    { "newB", objex_newB },
    { "newC", objex_newC },
    { "newD", objex_newD },
    { "getD", objex_getD },
    { "makeD", objex_makeD },
    { NULL, NULL }
  };
  /* You put metamethods and normal methods in the same luaL_Reg
   * array. `moon_defobject` puts the functions in the right place
   * automatically. */
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
    /* A C object can be cast to a D object (see below), so C objects
     * can use the methods of the D type! */
    { "printmeD", D_printme },
    { "close", C_close },
    { NULL, NULL }
  };
  luaL_Reg const D_methods[] = {
    { "__index", D_index },
    { "__newindex", D_newindex },
    { "printme", D_printme },
    { NULL, NULL }
  };
  /* All object types must be defined once (this creates the
   * metatables): */
  moon_defobject( L, "A", sizeof( A ), A_methods, 0 );
  moon_defobject( L, "B", sizeof( B ), B_methods, 0 );
  lua_pushinteger( L, 1 );
  lua_pushinteger( L, 2 );
  moon_defobject( L, "C", sizeof( C ), C_methods, 2 );
  moon_defobject( L, "D", sizeof( D ), D_methods, 0 );
  /* Add a type cast from a C object to the embedded D object. The
   * cast is executed automatically during moon_checkobject. */
  moon_defcast( L, "C", "D", C_to_D );
#if LUA_VERSION_NUM < 502
  luaL_register( L, "objex", objex_funcs );
#else
  luaL_newlib( L, objex_funcs );
#endif
  return 1;
}


