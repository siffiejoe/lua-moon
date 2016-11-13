#                 Moon -- A C Binding Toolkit for Lua                #

This library provides new convenience functions for binding C types to
Lua as userdata for Lua 5.1, Lua 5.2, and Lua 5.3. It supports objects
with different lifetimes, polymorphic type checking, type-safe binding
of tagged unions or embedded structs, properties and methods at the
same time, and uniform handling of pointers to objects using a simple
and small set of API functions.


##                        Using this Library                        ##

This package includes a header file `moon.h` and the corresponding
source file `moon.c`. To use this package you need to include the
header file wherever you want to call one of the macros/functions
defined within. If you just have a single source file where you want
to use those functions, you are done: `moon.h` includes `moon.c` and
makes every function `static`. If you have multiple source files that
need functions/macros from this library, this approach is flawed,
because you will end up with multiple versions of the same functions.
Instead include the header wherever you need it, but when you compile
those source files, define the macro `MOON_PREFIX` to a unique name
to use for all functions in the `moon` library. You also have to
compile and link `moon.c` using the same define. This approach will
change the common `moon_` prefix to your custom prefix behind the
scenes to avoid linker errors in case another library also links to
`moon.c`.
The header file `moon_flag.h` can be included whenever needed, but it
depends on the functions defined in `moon.c`. The `moon_dlfix.h`
header is completely independent, but relies on some platform specific
functions.


##                             Reference                            ##

This section lists all provided macros/functions.


###                        `moon.h`/`moon.c`                       ###

The main part of the moon toolkit.


####          `MOON_EXPORT`, `MOON_IMPORT`, `MOON_LOCAL`          ####

    #define MOON_EXPORT
    #define MOON_IMPORT
    #define MOON_LOCAL

Macros for specifying symbol visibility.


####                         `MOON_CONCAT`                        ####

    #define MOON_CONCAT( _a, _b )

A macro that evaluates both arguments and joins them together. You can
use that to build valid C identifiers with custom prefixes or
suffixes.


####                       `MOON_STRINGIFY`                       ####

    #define MOON_STRINGIFY( _v )

A macro that has the same effect as `#_v`, but works outside of a
macro substitution.


####                     `moon_object_header`                     ####

    typedef struct {
      unsigned char   flags;
      unsigned char   cleanup_offset;
      unsigned char   vcheck_offset;
      unsigned char   object_offset;
    } moon_object_header;

Common data structure shared by all userdata objects created via the
moon toolkit. The object may have optional fields following the memory
of this header structure, stored at the given offsets. The `flags`
field is a bit mask describing the details of the object. A pointer to
this header can be obtained by using plain `lua_touserdata` on a moon
object.


####                      `moon_object_cast`                      ####

    typedef void* (*moon_object_cast)( void* );

Function pointer type for conversion functions used by `moon_defcast`.


####                   `moon_object_destructor`                   ####

    typedef void (*moon_object_destructor)( void* );

Function pointer type for cleanup functions of moon objects.


####       `MOON_OBJECT_IS_VALID`, `MOON_OBJECT_IS_POINTER`       ####

    #define MOON_OBJECT_IS_VALID    0x01
    #define MOON_OBJECT_IS_POINTER  0x02

Values stored in the `flags` field of the `moon_object_header`
structure. The only value interesting for users of the library is the
`MOON_OBJECT_IS_VALID` flag which is reset automatically by the
`moon_killobject` function to signal that the destructor has already
run.


####                       `moon_defobject`                       ####

    /*  [ -nup, +0, e ]  */
    void moon_defobject( lua_State* L,
                         char const* metatable_name,
                         size_t userdata_size,
                         luaL_Reg const* methods,
                         int nup );

This function creates a new metatable and registers the functions in
the `luaL_Reg` array (functions starting with double underscores `__`
go into the metatable, the rest goes into a table used by the
`__index` metafield). In case a metatable with the same name already
exists, an error is raised. The `userdata_size` is stored in the
metatable for the `moon_newobject` function -- use a size of 0 to
prohibit use of `moon_newobject` (e.g. for incomplete types). If `nup`
is non-zero, it pops those upvalues from the current Lua stack top and
makes them available to all registered functions (metamethods *and*
methods). A `__gc` metamethod and a default `__tostring` metamethod
are provided by `moon_defobject` as well.


####                       `moon_newobject`                       ####

    /*  [ -0, +1, e ]  */
    void* moon_newobject( lua_State* L,
                          char const* metatable_name,
                          moon_object_destructor destructor );

This function allocates a userdata, sets a metatable, and stores the
cleanup function for later use by the `__gc` metamethod or the
`moon_killobject` function. It throws an error if the `metatable_name`
has not been registered via the `moon_defobject` function. The new
userdata object is pushed to the top of the Lua stack, and a pointer
to the payload (*not* the `moon_object_header` structure) is returned.


####                       `moon_newpointer`                      ####

    /*  [ -0, +1, e ]  */
    void** moon_newpointer( lua_State* L,
                            char const* metatable_name,
                            moon_object_destructor destructor );

This function allocates a userdata suitable for storing a pointer,
sets a metatable, and stores the cleanup function for later use by the
`__gc` metamethod or the `moon_killobject` function. It is equivalent
to `moon_newobject` with the difference that the payload is stored as
a pointer, not inside the userdata memory. The pointer is initialized
to `NULL` when this function returns, and may be set by assigning to
the memory location pointed to by the return value.


####                        `moon_newfield`                       ####

    /*  [ -0, +1, e ]  */
    void** moon_newfield( lua_State* L,
                          char const* metatable_name,
                          int idx,
                          int (*isvalid)( void* p ),
                          void* p );

This function allocates a userdata suitable for storing a pointer, and
sets a metatable. It is similar to `moon_newpointer`, but it is
intended for exposing a data structure embedded within another
userdata (referenced by stack position `idx`). The resulting moon
object keeps the parent userdata alive by storing a reference in its
uservalue table. If `idx` is `0`, no uservalue table is set. Setting a
cleanup function is not possible, because the parent userdata is
responsible for cleaning up memory and other resources.
If an `isvalid` function pointer is provided, it is called by the
`moon_checkobject`/`moon_testobject` functions to check whether the
object is still valid. This can be used to make sure that a tagged
union used as parent userdata still has the correct type, or that the
parent userdata hasn't released any necessary resources prior to
garbage collection. If the value referenced by `idx` is a moon object
that also has an `isvalid` check registered, all checks are performed
in the order from parent object(s) to child object.


####                       `moon_getmethods`                      ####

    /*  [ -0, +(0|1), e ]  */
    int moon_getmethods( lua_State* L,
                         char const* tname );

If the metatable identified by `tname` has methods registered, pushes
the methods table and returns `LUA_TTABLE`. Otherwise nothing is
pushed and `LUA_TNIL` is returned. This function only works for moon
objects and raises an error if the metatable `tname` wasn't registered
via `moon_defobject`.


####                       `moon_killobject`                      ####

    /*  [ -0, +0, e ]  */
    void moon_killobject( lua_State* L,
                          int idx );

If the moon object at the given stack index is valid, its cleanup
function is run, and the object is marked as invalid (to prevent the
cleanup function from running again). This function can be used to
reclaim resources before the object becomes unreachable.


####                        `moon_defcast`                        ####

    /*  [ -0, +0, e ]  */
    void moon_defcast( lua_State* L,
                       char const* tname1,
                       char const* tname2,
                       moon_object_cast cast );

Registers the conversion function `cast` for converting userdata
objects of type `tname1` to type `tname2`. The `cast` function is
called automatically by `moon_checkobject( L, idx, tname2 )` when
applied to an object of type `tname1`, so function implementations can
be reused without extra work. The metatable `tname1` must already
exist and belong to a moon object type (created via `moon_defobject`).


####                      `moon_checkobject`                      ####

    /*  [ -0, +0, v ]  */
    void* moon_checkobject( lua_State* L,
                            int idx,
                            char const* metatable_name );

This function ensures that the value stored at stack index `idx`

1.  is a full userdata
2.  is a moon object
3.  has the metatable identified by `metatable_name` or has a
    cast function to `metatable_name` registered
4.  has the `MOON_OBJECT_IS_VALID` flag set
5.  all `isvalid` functions return a non-zero value (for objects
    created via `moon_newfield`)
6.  contains a non-`NULL` pointer value (for objects created via
    `moon_newpointer` or `moon_newfield`).

If any of those conditions are false, an error is raised. Otherwise
this function returns a pointer to the object's memory (meaning the
objects created via `moon_newpointer` and `moon_newfield` are
dereferenced once, and if necessary the registered cast function is
called).


####                       `moon_testobject`                      ####

    /*  [ -0, +0, e ]  */
    void* moon_testobject( lua_State* L,
                           int idx,
                           char const* metatable_name );

Performs the same checks as `moon_checkobject`, but returns `NULL` if
any of those conditions are false instead of raising an error.


####                        `moon_checkint`                       ####

    /*  [ -0, +0, v ]  */
    lua_Integer moon_checkint( lua_State* L,
                               int idx,
                               lua_Integer min,
                               lua_Integer max );

This function works like `luaL_checkinteger` but additionally ensures
that the given value is in the range [`min`, `max`], or else an error
is thrown.


####                         `moon_optint`                        ####

    /*  [ -0, +0, v ]  */
    lua_Integer moon_optint( lua_State* L,
                             int idx,
                             lua_Integer min,
                             lua_Integer max,
                             lua_Integer def );

Similar to `moon_checkint` but uses the default value `def` if the
value at the given stack position is `nil` or `none`.


####                         `moon_atexit`                        ####

    /*  [ -0, +1, e ]  */
    int* moon_atexit( lua_State* L,
                      lua_CFunction cleanup );

This function puts an integer-sized userdata (initialized to 0) in the
registry and sets the given `cleanup` function as `__gc` metamethod.
The userdata is also pushed to the top of the Lua stack, and returned
as an `int` pointer.
Use this function if you want to call a cleanup function when the Lua
state is closed, but only if some initialization succeeded. The usual
approach is as follows:

1.  Call `moon_atexit` registering your cleanup function.
2.  Do your initialization.
3.  If successful, you set the `int` pointer to non-zero, which is
    almost atomic and can't fail, and pop the userdata.
4.  When the cleanup function is called, check for a non-zero value
    before actually cleaning up.


####                       `moon_setuvfield`                      ####

    /*  [ -1, +0, e ]  */
    void moon_setuvfield( lua_State* L,
                          int idx,
                          char const* key );

This function pops the value at the top of the stack and stores it
under `key` in the environment/uservalue table of the object at stack
position `idx`.


####                       `moon_getuvfield`                      ####

    /*  [ -0, +(0|1), e ]  */
    int moon_getuvfield( lua_State* L,
                         int idx,
                         char const* key );

This function works similar to `luaL_getmetafield`, but it looks for
`key` in the environment/uservalue table of the object at index `idx`,
and pushes the corresponding value to the top of the stack. If there
is no uservalue table or no such field, nothing is pushed and
`LUA_TNIL` is returned. Otherwise, the return value is the type of the
pushed value.


####                        `moon_getcache`                       ####

    /*  [ -0, +1, e ]  */
    void moon_getcache( lua_State* L,
                        int idx );

This function looks up and pushes a weak-valued table under a private
key in the table given by index `idx` (often `LUA_REGISTRYINDEX`). If
the weak-valued table doesn't exist yet, it is created automatically.
This function is useful to map lightuserdata to full userdata, but it
can also be used to cache full userdata for enum values (using
separate caches per enum type), etc.


####                      `moon_stack_assert`                     ####

    /*  [ -0, +0, v ]  */
    void moon_stack_assert( lua_State* L,
                            ... );

This "function" is implemented as a macro that evaluates to `void` if
`NDEBUG` is defined. If it is not, it tries to match the type
specifications (strings) given as arguments to the values at the top
of the Lua stack. Every mismatch is reported on `stderr`, and finally
the whole Lua stack is dumped to `stderr` using the `moon_stack`
function below, and an error is raised. Currently the following type
specifications are supported:

*   `"n"`: nil
*   `"b"`: boolean
*   `"l"`: lightuserdata
*   `"i"`: integer (equivalent to `"d"` before Lua 5.3)
*   `"d"`: number (think `double`)
*   `"s"`: string
*   `"t"`: table
*   `"f"`: function
*   `"u"`: userdata
*   `"c"`: coroutine
*   `"a"`: any (non-nil) value

You can combine the single letter options to express alternatives, so
e.g. `"tf"` means: table or function.


####                         `moon_stack`                         ####

    /*  [ -0, +0, - ]  */
    void moon_stack( lua_State* L );

This "function" is also implemented as a macro that evaluates to
`void` in case `NDEBUG` is defined. Otherwise it prints the current
contents of the Lua stack in human-readable format to `stderr`.


####                        `moon_absindex`                       ####

Compatiblity macro for `lua_absindex`, but also available on Lua 5.1
as a function.


####                         `moon_derive`                        ####

    int moon_derive( lua_State* L );

A `lua_CFunction` that may be registered as part of your module and
allows Lua code to subclass a moon object. When called it expects two
strings as arguments: a new (non-existing) type name and an old
(existing) type name of a moon object type. It sets up the new type
name as an alias to the old type but with a different methods table.
The new methods table (initially a copy of the methods of the old
type) is returned.


####                        `moon_downcast`                       ####

    int moon_downcast( lua_State* L );

A `lua_CFunction` that may be registered as part of your module and
allows Lua code to change the type of a moon object to a type created
via `moon_derive`. It is typically used in constructors of derived
types, and expects a moon object and the new type name as arguments.
If successful, the object (with its metatable replaced) is returned.


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

*   `MOON_FLAG_NAME` (required): A metatable name used for defining a
    userdata type.
*   `MOON_FLAG_TYPE` (required): The underlying enum/flag type that is
    handled by the custom userdata.
*   `MOON_FLAG_SUFFIX` (required): All defined functions have this
    suffix (and the `moon_flag_` prefix) to make them unique.
*   `MOON_FLAG_NOBITOPS` (optional): If this macro is defined, no
    metamethods for bitwise operations are created. This includes
    `__add`, `__sub`, and `__call` metamethods. If you do this, you
    should think about using strings and `luaL_checkoption` instead of
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

    /*  [ -0, +0, e ]  */
    void moon_flag_def_SUFFIX( lua_State* L );
    /*  [ -0, +1, e ]  */
    void moon_flag_new_SUFFIX( lua_State* L, TYPE value );
    /*  [ -0, +0, v ]  */
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
userdata on the Lua stack (or raises an error).


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


####                    `MOON_DLFIX_LIBPREFIX`                    ####

For some OSes *all* loaded shared libraries is searched for an ELF
object that contains the Lua symbols. Since this procedure also finds
shared objects that have the Lua library as a dependency, the library
name is checked for a known prefix to make sure that only the actual
Lua library is exported. The default is `"liblua"`, but you can change
it by defining this macro.


##                              Contact                             ##

Philipp Janda, siffiejoe(a)gmx.net

Comments and feedback are always welcome.


##                              License                             ##

`moon` is *copyrighted free software* distributed under the Tiny
license. The full license text follows:

    moon (c) 2013-2016 Philipp Janda

    You may do anything with this work that copyright law would normally
    restrict, so long as you retain the above notice(s) and this license
    in all redistributed copies and derived works.  There is no warranty.


