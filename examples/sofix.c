#include "moon_dlfix.h"

/* declaration of lua_State as an opaque type */
struct lua_State;
typedef struct lua_State lua_State;

#ifndef EXPORT
#define EXPORT extern
#endif

EXPORT int luaopen_sofix( lua_State* L ) {
  (void)L;
  MOON_DLFIX();
  return 0;
}

