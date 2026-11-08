# How MULE works {#concepts}

[TOC]

Each module's topic page explains that module. This page explains the
rules every module shares. Learn these once and each function behaves the
way you expect before you read its documentation.

## Using the library {#using}

A project copies one file, `dist/mule.h`. Include it anywhere for the
declarations. In exactly one `.c` file, define `MULE_IMPLEMENTATION` first,
and the library compiles into that file:

```c
#define MULE_IMPLEMENTATION
#include "mule.h"
```

Include it before any system header. @ref mod_base sets `_POSIX_C_SOURCE`,
which only takes effect before the first one. Define `MULE_TEST` as well to
get the @ref mod_test harness.

Inside this repository, programs build as one translation unit instead.
Each test and example includes the `.c` files it needs, as in
`#include "str/str.c"`. That is why every example here compiles with a
single compiler call.

The library needs a C11 compiler and is written in the style of C99. It is
tested with clang 17+ and gcc 13+ on macOS and Linux.

## Modules {#modules}

Each module is a header and a source file under `src/`. They depend on each
other in this order, so a module only uses the ones above it.

| Module | Gives you | Uses |
| --- | --- | --- |
| @ref mod_base | Feature macros, `bool`, checked arithmetic | |
| @ref mod_mem | Allocator, Arena | base |
| @ref mod_str | Str, StrView | mem |
| @ref mod_arr | Arr(T) | mem |
| @ref mod_map | Map(E), keyed by StrView | str, arr |
| @ref mod_rand | Rand | base |
| @ref mod_time | time_now_ns() | base |
| @ref mod_log | log_info() and friends, log_assert() | str |
| @ref mod_test | TEST, CHECK, RUN | log |

## Failure is a bool {#errors}

A function that can fail returns `bool`. False means it did nothing: a
failed call leaves every argument byte for byte as it was. The tests prove
this by making each allocation fail in turn and comparing the arguments
afterwards.

Because nothing changes on failure, calls chain with `&&`, and the chain
stops at the first one that fails:

```c
bool ok = str_append_cstr(&s, "Rodion") &&
          str_push(&s, ' ') &&
          str_appendf(&s, "age %d", 23);
```

The compiler warns when a result that must be checked is ignored. gcc warns
even through a `(void)` cast.

Two other kinds of failure do not return `bool`:
  - **Programming errors** are caught by `assert`, such as slicing past the
    end of a view with sv_slice(). Each function lists them under
    *Precondition*. They vanish from release builds.
  - **Unrecoverable states** go through log_assert() and log_fatal(), which
    print where they happened and abort, in release builds too.

## Zero is a valid value {#zero}

Str, StrView, Arena, Arr and Rand are ready to use when zero-initialised,
and own nothing until something is put in them:

```c
Str s = { 0 };
Arena arena = { 0 };
IntArr xs = { 0 };
Rand r = { 0 };
```

The free functions return a value to this empty state, keeping only its
allocator. Calling one twice is safe, and a struct whose fields were
zero-initialised can be freed even when it was only half filled in.

## Memory {#memory}

### Who allocates

Every allocation goes through an @ref mod_mem "Allocator". A Str or an Arr
holds a pointer to one in its `alloc` field, and null means the libc heap.
Point it at an arena with arena_allocator(), and the value grows inside the
arena and is freed with it:

@snippet{trimleft} test_str.c arena_str

### Who frees

The value that owns memory frees it. A Str is freed with str_free(), an Arr
with arr_free(), a Map with map_free(), and an arena with everything in it by arena_free(). A struct
that holds owning fields gets its own `_free` function that frees each one.

Never copy an owning struct by assignment. `Str b = a;` copies the pointer,
not the bytes, and freeing both is a double free. Copy the contents with
str_from(), or move them by copying the struct and resetting the source to
`{ 0 }`.

### How long a pointer lives

Anything that points into a buffer dies when the buffer moves:
  - A StrView into a Str, and the pointer from str_cstr(), die at the next
    call that may grow that Str.
  - A pointer into an Arr's `data` dies at the next arr_push(), arr_insert() or
    arr_reserve().
  - A pointer into a Map's `data` dies at the next map_put() that adds a key,
    or map_reserve(). An index from map_put() lives until map_clear().
  - A Map's keys borrow their bytes, so each key's owner must outlive the map.
  - Memory from an arena dies at arena_reset() or arena_free().
  - An Arena must not move once arena_allocator() has been called on it.

Reserve the full size first with str_reserve() or arr_reserve(), and the
appends that fit will not move the buffer.

## Naming {#naming}

  - Every public function starts with its module's prefix, such as `str_`,
    `sv_`, `arena_` and `log_`. Types are `CamelCase`, and macros that do
    not look like functions are `UPPER_CASE`.
  - Some operations are macros that look like functions, such as arr_push()
    and log_info(). They are macros because no function could do the job:
    work for any element type, or capture the caller's file and line.
    The reference marks each one as a macro.
  - Names ending in `_` are internal and may change. They are left out of
    this reference.

## Threads {#threads}

The library keeps no global state except the log level and the log output.
Set both before starting threads. Each log line is written with one
`fwrite`, so lines from different threads do not interleave. Every other
value belongs to whoever holds it, so give each thread its own, or lock
around a shared one.

## Versions {#versions}

Releases follow Semantic Versioning. Each one is a GitHub release with
`mule.h` attached, and `CHANGELOG.md` lists what changed. Before 1.0.0, a
minor release may break the API, and a patch release never does.
MULE_VERSION and the `MULE_VERSION_*` macros in @ref mod_base say which
version a program was built with.

This reference follows the `main` branch, which may be ahead of the latest
release.

## Where to go next {#next}

  - The [Str and StrView tutorial](str-and-strview.html) teaches the two
    string types with programs you can run.
  - **Examples** lists complete programs. Each one links every library call
    to its documentation.
  - Each function's documentation links to its definition, so you can read
    how it works, comments included.
  - The tests in `tests/` exercise every function, including its failure
    paths, and most examples in this reference are cut from them.
