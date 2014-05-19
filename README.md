#                  Moon -- A Binding Toolkit for Lua                 #

This library provides some compatiblity functions/macros to work with
the Lua C API of Lua 5.1, Lua 5.2, and Lua 5.3 in a uniform way. It
also contains new API functions/macros to ease the binding of C
libraries.


##                        Using this Library                        ##

This package includes a header file `moon.h` and the corresponding
source file `moon.c`. To use this package you need to include the
header file wherever you want to call one of the macros/functions
defined within. If you just have a single source file where you want
to use those functions, you are done: `moon.h` includes `moon.c` and
makes every function `static`. If you have multiple source files that
need functions/macros from this library this approach is flawed,
because you will end up with multiple versions of the same functions.
Instead include the header wherever you need it, but when you compile
those source files, define the macro `MOON_PREFIX` to a unique name
to use for all functions in the `moon` library. You also have to
compile and link `moon.c` using the same define. This approach will
change the common `moon_` prefix to your custom prefix behind the
scenes to avoid linker errors in case another library also links to
`moon.c`.
The header files `moon_flag.h` and `moon_sub.h` can be included
whenever needed, but they depend on the functions defined in `moon.c`.
The `moon_dlfix.h` header is completely independent, but relies on
some platform specific functions.


##                             Reference                            ##

This section lists all provided macros/functions.


###                        `moon.h`/`moon.c`                       ###

The main part of the moon toolkit.


####                         `MOON_CONCAT`                        ####

    #define MOON_CONCAT( _a, _b )

A macro that evaluates both arguments and joins them together. You can
use that to build valid C identifiers with custom prefixes or
suffixes.


####                       `MOON_STRINGIFY`                       ####

    #define MOON_STRINGIFY( _v )

A macro that has the same effect as `#_v`, but works outside of a
macro substitution.


####                      `moon_object_type`                      ####

    typedef struct {
      char const*       metatable_name;
      size_t            userdata_size;
      void (*initializer)( void* p );
      luaL_Reg const*   metamethods;
      luaL_Reg const*   methods
    } moon_object_type;

A struct for collecting all necessary information to define an object
type (using `moon_defobject`).
`metatable_name` is used to identify this object type (e.g. for
`moon_checkudata` and `moon_newobject`).
`userdata_size` is passed to `lua_newuserdata` when constructing such
an object.
The `initializer` is called with the result of `lua_newuserdata`
_before_ the metatable is set. Use this if you want to signal to your
`__gc` metamethod that the object is not yet initialized for the case
an error happens between setting the metatable and successful
initialization. If you don't have a `__gc` metamethod, you usually can
leave it as a `NULL` pointer.
`metamethods` and `methods` are the usual Lua function registration
arrays. The `metamethods` are added directly to the metatable, the
`methods` are put in a table in the `__index` field.  Both can be
`NULL`. If `metamethods` contains an `__index` function _and_
`methods` is not `NULL`, a special function is registered that first
looks in the methods table and then calls the original `__index`
function if necessary.


####                        `moon_lua_reg`                        ####

    typedef struct {
      char const*   require_name;
      char const*   error_name;
      char const*   code;
      size_t        code_length;
    } moon_lua_reg;

This struct is used in a similar way as `luaL_Reg` but for registering
embedded lua modules (using `moon_preload_lua`).
`require_name` is the module name that should be passed to the
`require` function in order to load this module.
`error_name` is used in error messages. If your lua code came from an
external file, `error_name` should start with an `@` as usual.
`code` is the actual Lua code (possibly precompiled) as a C array. The
length in bytes of this C array is given as the `code_length` field.


####                       `moon_defobject`                       ####

    /*  [ -nup, +0, e ]  */
    void moon_defobject( lua_State* L,
                         moon_object_type const* type,
                         int nup );

This function creates a metatable for the objects described in the
`moon_object_type` struct. All information is transferred to the
metatable, so you can create the `moon_object_type` struct and the
`luaL_Reg` arrays on the C stack. In case a metatable with the same
name already exists, this function raises an error. If `nup` is
non-zero, it pops those upvalues from the current Lua stack top and
makes them available to all registered functions (metamethods _and_
methods). In case the object type has an `__index` function _and_
methods, the original `__index` function will be called with two extra
upvalues at index 1 and 2, so your own upvalues start at index 3.


####                       `moon_newobject`                       ####

    /*  [ -0, +1, e ]  */
    void* moon_newobject( lua_State* L,
                          char const* metatable_name,
                          int env_index );

This function allocates a userdata, sets a metatable, and (if
`env_index` is non-zero) also sets a uservalue/environment. It throws
an error if the `metatable_name` has not been registered via the
`moon_defobject` function. The new userdata object is pushed to the
top of the Lua stack.


####                     `moon_newobject_ref`                     ####

    /*  [ -0, +1, e ]  */
    void* moon_newobject_ref( lua_State* L,
                              char const* metatable_name,
                              int ref_index );

This function works like the `moon_newobject` function above, but if
`ref_index` is non-zero, it also creates a new uservalue table and
puts the referenced value in it. Use this if your new object depends
on the lifetime of another object (located at `ref_index`) and should
not outlive it.


####                       `moon_propindex`                       ####

    /*  [ -nup, +1, e ]  */
    void moon_propindex( lua_State* L,
                         luaL_Reg const* methods,
                         lua_CFunction index,
                         int nup );

This function is used for creating an `__index` metafield. If `index`
is `NULL` but `methods` is not, a table containing all the functions
in `methods` is created and pushed to the top of the stack. If `index`
is not `NULL`, but `methods` is, the `index` function pointer is
simply pushed to the top of the stack. In case both are non `NULL`, a
new C closure is created and pushed to the stack, which first tries to
lookup a key in the methods table, and if unsuccessful then calls the
original `index` function. If both are `NULL`, `nil` is pushed to the
stack. If `nup` is non-zero, the given number of upvalues is popped
from the top of the stack and made available to _all_ registered
functions. (In case `input` _and_ `methods` are not `NULL`, the
`index` function receives two additional upvalues at indices 1 and 2.)
This function is used in the implementation of `moon_defobject`, but
maybe it is useful to you independently.


####                       `moon_finalizer`                       ####

    /*  [ -0, +1, e ]  */
    int* moon_finalizer( lua_State* L,
                         lua_CFunction cleanup );

This function puts an integer-sized userdata (initialized to 0) in the
registry and sets the given `cleanup` function as `__gc` metamethod.
The userdata is also pushed to the top of the Lua stack, and returned
as an `int` pointer.
Use this function if you want to call a cleanup function when the Lua
state is closed, but only if some initialization succeeded. The usual
approach is as follows:

1.  Call `moon_finalizer` registering your cleanup function.
2.  Do your initialization.
3.  If successful, you set the `int` pointer to non-zero, which is
    almost atomic and can't fail, and pop the userdata.
4.  When the cleanup function is called, check for a non-zero value
    before actually cleaning up.


####                       `moon_preload_c`                       ####

    /*  [ -0, +0, e ]  */
    void moon_preload_c( lua_State* L,
                         luaL_Reg const* libs );

If you want to preload the loader functions of C modules (e.g. because
you statically linked them to your executable), you can put them in
the usual `luaL_Reg` array and pass it to `moon_preload_c` which puts
all loaders under their given names in the `package.preload` table (or
its equivalent in the registry). When done you can `require` those C
modules as usual.


####                      `moon_preload_lua`                      ####

    /*  [ -0, +0, e ]  */
    void moon_preload_lua( lua_State* L,
                           moon_lua_reg const* libs );

If you want to embed Lua modules into your executable, you usually put
the Lua source code (or the byte code) in a `char` array. You can then
call this function to load the code and put the resulting chunk in the
`package.preload` table. The function takes a properly terminated
array of `moon_lua_reg` structs, so you can register multiple Lua
modules at once.


####                       `moon_setuvfield`                      ####

    /*  [ -1, +0, e ]  */
    void moon_setuvfield( lua_State* L,
                          int idx,
                          char const* key );

This function pops the value at the top of the stack and stores it
under `key` in the environment/uservalue table of the object at stack
position `idx`.


####                       `moon_getuvfield`                      ####

    /*  [ -0, +1, e ]  */
    int moon_getuvfield( lua_State* L,
                         int idx,
                         char const* key );

This function works similar to `luaL_getmetafield`, but it looks for
`key` in the environment/uservalue table of the object at index `idx`,
and pushed the corresponding value to the top of the stack. If there
is no uservalue table or no such field, `nil` is pushed instead. The
return value is the type of the pushed value.


####                       `moon_light2full`                      ####

    /*  [ -0, +1, e ]  */
    void moon_light2full( lua_State* L,
                          int idx );

This function looks up and pushes a weak-valued table under a private
key in the table given by index `idx` (often `LUA_REGISTRYINDEX`). If
the weak-valued table doesn't exist yet, it is created automatically.
This function is useful to map lightuserdata to full userdata (hence
the name), but it can also be used to cache full userdata for enum
values (using separate caches per enum type), etc.


####                      `moon_lookuptable`                      ####

    /*  [ -0, +1, e ]  */
    void moon_lookuptable( lua_State* L,
                           char const* const names[],
                           unsigned const values[] );

Creates and pushes to the stack a Lua table that maps all given
`names` (up to a terminating `NULL`) to the corresponding value in the
`values` array, and vice versa. The resulting table can be used for
the `moon_pushoption`/`moon_checkoption` APIs or in combination with a
`switch` statement in C.


####                       `moon_pushoption`                      ####

    /*  [ -0, +1, e ]  */
    void moon_pushoption( lua_State* L,
                          unsigned val,
                          unsigned const values[],
                          char const* const names[],
                          int lookuptable );

Lua often uses strings of enums or flags (at least if you don't need
bit operations to combine them somehow), see e.g. `luaL_checkoption`.
This function does the reverse: It takes an enum value `val` and
pushes the matching string to the Lua stack. How this matching string
is obtained depends on the given arguments. If `lookuptable` is
non-zero, the table at the given index is used to map `val` to a
string name, otherwise the arrays `values` and `names` (the latter
must be `NULL`-terminated`) are searched for a match. If no string
name could be found, the enum value is pushed as a number.


####                      `moon_checkoption`                      ####

    /*  [ -0, +0, v ]  */
    unsigned moon_checkoption( lua_State* L,
                               int idx,
                               char const* def,
                               char const* const names[],
                               unsigned const values[],
                               int lookuptable );


This function works similar to `luaL_checkoption`, but it returns the
enum value taken from the `values` array directly instead of an array
index. If `lookuptable` is non-zero, the table at this index is used
instead of the `names` and `values` arrays.


####                      `moon_stack_assert`                     ####

    /*  [ -0, +0, v ]  */
    void moon_stack_assert( lua_State* L,
                            ... );

This "function" is implemented as a macro that evaluates to `void` if
`NDEBUG` is defined. If it is not, it tries to match the type names
given as arguments to the values at the top of the Lua stack. Every
mismatch is reported on `stderr`, and finally the whole Lua stack is
dumped to `stderr` using the `moon_stack` function below, and an error
is raised. You can use `"*"` as wildcard type name.


####                         `moon_stack`                         ####

    /*  [ -0, +0, - ]  */
    void moon_stack( lua_State* L );

This "function" is also implemented as a macro that evaluates to
`void` in case `NDEBUG` is defined. Otherwise it prints the current
contents of the Lua stack in human-readable format to `stderr`.


####                       `moon_checkudata`                      ####

Compatibility macro that directly calls `luaL_checkudata` on Lua 5.3,
but provides slightly better error messages on 5.2 and below.


####                       `moon_testudata`                       ####

Compatibilty macro for `luaL_testudata`, but is also available on Lua
5.1.


####                        `moon_absindex`                       ####

Compatiblity macro for `lua_absindex`, but also available on Lua 5.1.


####                        `moon_register`                       ####

Compatibility macro for putting the functions given in a `luaL_Reg`
array into a Lua table at the top of the stack. It is equivalent to
`luaL_register` with a `NULL` name, or `luaL_setfuncs` without
upvalues.


####                      `moon_setuservalue`                     ####

Compatibility macro that evaluates to `lua_setuservalue` or
`lua_setfenv` depending on the Lua version.


####                      `moon_getuservalue`                     ####

Compatibility macro that evaluates to `lua_getuservalue` or
`lua_getfenv` depending on the Lua version.


####                         `moon_rawlen`                        ####

Compatibility macro that evaluates to `lua_rawlen` or `lua_objlen`
depending on the Lua version.


###                          `moon_flag.h`                         ###

`moon_flag.h` is a macro file, that can be included multiple times and
each time defines a new userdata type as a type-safe representation of
a given enum/flag type in Lua. The resulting userdata values support
`+` (bitwise or, in Lua 5.3 also `|`), `-` (create a new value with
certain bits cleared), and calling (test if all bits are set). For Lua
5.3 also bitwise "and" and bitwise "not" are defined.
Parameters are passed as macros before including the macro file. Any
arguments and all internal macros are `#undef`ed before leaving the
macro file. The following parameters are recognized:

*   `MOON_FLAG_NAME`: A metatable name used for defining a userdata
    type.
*   `MOON_FLAG_TYPE`: The underlying enum/flag type that is handled by
    the custom userdata.
*   `MOON_FLAG_SUFFIX`: All defined functions have this suffix (and
    the `moon_flag_` prefix) to make them unique.
*   `MOON_FLAG_NOBITOPS` (optional): If this macro is defined, no
    metamethods for bitwise operations are created. This includes
    `__add`, `__sub`, and `__call` metamethods. If you do this, you
    should think about using strings and `moon_checkoption` instead of
    userdata for representing your flags in Lua.
*   `MOON_FLAG_NORELOPS` (optional): If this macro is defined, the
    `__eq` metamethod is not created.
*   `MOON_FLAG_USECACHE` (optional): The constructor function for this
    flag looks in a local cache before creating a new full userdata,
    and returns the cached value if possible. This way each enum/flag
    value has at most one userdata associated with it.
*   `MOON_FLAG_EQMETHOD( _a, _b )` (optional): If you need a custom
    comparison operation instead of the usual `==`, define this macro.

The following (static) functions will be defined, unless they are
disabled via one of the parameter macros above:

    /*  [ -nup, +0, e ]  */
    void moon_flag_def_SUFFIX( lua_State* L, int nup );
    /*  [ -0, +1, e ]  */
    void moon_flag_new_SUFFIX( lua_State* L, TYPE value );
    /*  [ -0, +0, e ]  */
    TYPE moon_flag_get_SUFFIX( lua_State* L, int idx );

    int moon_flag_add_SUFFIX( lua_State* L );
    int moon_flag_sub_SUFFIX( lua_State* L );
    int moon_flag_call_SUFFIX( lua_State* L );
    int moon_flag_and_SUFFIX( lua_State* L ); /* Lua 5.3+ */
    int moon_flag_not_SUFFIX( lua_State* L ); /* Lua 5.3+ */
    int moon_flag_eq_SUFFIX( lua_State* L );

The last six are metamethods and not supposed to be called from C.
`moon_flag_def_SUFFIX` defines the new type, creates the metatable and
registers all metamethods. `moon_flag_new_SUFFIX` pushes a userdata
representing the given value to the top of the Lua stack, while
`moon_flag_get_SUFFIX` returns the corresponding enum value from a
userdata on the Lua stack.


###                          `moon_sub.h`                          ###

`moon_sub.h` is another macro file that can be used to create userdata
that represent embedded structs inside another userdata. It can be
included multiple times, and each time it defines static functions for
creating and handling sub-userdata. One requirement is that the parent
userdata has an environment/uservalue set, because it is used to keep
track of and cache the sub-userdata. The sub-userdata has `__index`,
`__newindex`, and (in Lua 5.2+) `__pairs` metamethods defined. The
following parameters can be used to control the definition of the
sub-userdata:

*   `MOON_SUB_NAME`: A metatable name used for defining a userdata
    type.
*   `MOON_SUB_TYPE`: The type of the embedded struct to be wrapped.
*   `MOON_SUB_SUFFIX`: A suffix to make the defined static functions
    unique to avoid linking problems.
*   `MOON_SUB_PARENT`: The type of the embedding (parent) struct.
*   `MOON_SUB_FIELD`: The field name of the embedded struct inside of
    the parent struct.
*   `MOON_SUB_ELEMENTS( macro )`: This is a higher-order macro, which
    takes another macro as argument and calls it once for every data
    element of the embedded struct that you want to expose to Lua
    passing four arguments:
    1.  the field identifier of the data element
    2.  a function or macro which takes a `lua_State*` and a value of
        the same type as the data element, and pushes a suitable Lua
        value to the top of the stack (e.g. `lua_pushinteger`, etc.,
        are compatible)
    3.  a macro which takes a `lua_State*`, an lvalue which
        represents the data element in the embedded, and an index of
        a suitable Lua value on the stack, and assigns that value to
        the data element in the embedded struct
    4.  a boolean flag, which indicates whether an assignment is
        allowed at all (the 3rd argument must still evaluate to
        something syntactically correct, but the result is never
        executed)
*   `MOON_SUB_POINTER` (optional): If the parent userdata contains a
    pointer to the embedding struct instead of the struct itself, you
    need to define this macro.
*   `MOON_SUB_NOPAIRS` (optional): Avoid defining iterator function
    and the `__pairs` metamethod to save memory. This flag is
    redundant in Lua 5.1.

The following (static) functions will be defined, unless they are
disabled via one of the parameter macros above:

    /*  [ -nup, +0, e ]  */
    void moon_sub_def_SUFFIX( lua_State* L, int nup );
    /*  [ -0, +1, e ]  */
    void moon_sub_new_SUFFIX( lua_State* L, int idx );

    int moon_sub_index_SUFFIX( lua_State* L );
    int moon_sub_newindex_SUFFIX( lua_State* L );
    int moon_sub_next_SUFFIX( lua_State* L ); /* Lua 5.2+ */
    int moon_sub_pairs_SUFFIX( lua_State* L ); /* Lua 5.2+ */

The last four are metamethods and not supposed to be called from C.
`moon_sub_def_SUFFIX` defines the new type, creates the metatable and
registers all metamethods. `moon_flag_new_SUFFIX` pushes a userdata
representing the embedded struct to the top of the Lua stack. Use it
in the `__index` metamethod of the parent userdata.


###                         `moon_dlfix.h`                         ###

On Linux and BSDs (and possibly other Unix machines) binary extension
modules aren't linked to the Lua library directly, but instead expect
the main executable to reexport the Lua API. If you don't have control
over the main executable (e.g. you are writing a plugin for a 3rd
party program), you are out of luck. This header file tries to
reexport the Lua API from the shared library linked to your plugin to
make it available for extension modules. It relies on some platform
specific tricks, so it probably won't work everywhere.


####                         `MOON_DLFIX`                         ####

    #define MOON_DLFIX()

This macro uses the dynamic linker to search for the Lua API in an
already loaded shared library and reexport it for extension modules.
If the necessary linker tricks don't work on the given platform, this
macro evaluates to a `void` expression, and you will continue to get
the usual unresolved symbol errors when loading a binary extension
module.


####                     `MOON_DLFIX_LIBNAME`                     ####

If the builtin heuristics fail to find the shared Lua library you can
specify the library name/path by defining this macro.


##                              Contact                             ##

Philipp Janda, siffiejoe(a)gmx.net

Comments and feedback are always welcome.


##                              License                             ##

`moon` is *copyrighted free software* distributed under the Tiny
license. The full license text follows:

    moon (c) 2013-2014 Philipp Janda

    You may do anything with this work that copyright law would normally
    restrict, so long as you retain the above notice(s) and this license
    in all redistributed copies and derived works.  There is no warranty.


