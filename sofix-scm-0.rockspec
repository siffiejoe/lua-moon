package="sofix"
version="scm-0"
source = {
  url = "git://github.com/siffiejoe/lua-moon.git",
}
description = {
  summary = "Fix linking problems with Lua API",
  detailed = [[
    Extension modules on Unixes expect the Lua API to be
    exported from the main executable. This is a problem
    if the Lua library is dynamically linked. This small
    extension module (which doesn't depend on the Lua API
    itself) fixes the import settings of the shared Lua
    library so that other extension modules can access
    the Lua API.
  ]],
  homepage = "https://github.com/siffiejoe/lua-moon/",
  license = "Tiny"
}
dependencies = {
  "lua >= 5.1, < 5.4"
}
build = {
  type = "builtin",
  modules = {
    sofix = {
      sources = { "examples/sofix.c" },
      incdirs = { "." }
    }
  }
}

