# MULE

Multi-Use Library Essentials. A small standard library for C23 in the
spirit of stb. It is written to be read as well as used.

It is early. Five modules exist, and `TODO.md` holds the roadmap.

| Module | What it gives you |
| --- | --- |
| `base` | The header every module includes first: feature macros and checked arithmetic. |
| `mem` | An allocator interface and an arena that frees everything in one call. |
| `arr` | `Arr(T)`, a growable array of any element type, with the same layout and rules as `Str`. |
| `str` | `Str`, an owned growable byte string, and `StrView`, a borrowed slice with search, split and trim. |
| `log` | Levelled messages with `file:line`, coloured when the terminal supports it, and an assert that stays in release builds. |

## A taste

```c
#include "mem/mem.c"
#include "str/str.c"

#include <stdio.h>

int main(void) {
    Arena arena = {};
    Str csv = { .alloc = arena_allocator(&arena) };
    if (!str_appendf(&csv, "%s,%s,%d", "mule", "c23", 4)) return 1;

    StrView rest = str_view(&csv), field;
    while (sv_split_next(&rest, SV(","), &field))
        printf("[%.*s]\n", STR_FMT(field));

    arena_free(&arena); /* frees csv along with everything else */
    return 0;
}
```

```sh
cc -std=c23 -Isrc example.c -o example && ./example
```

It prints `[mule]`, `[c23]` and `[4]`, each on its own line.

## Design

- **Failure is a `bool`.** A function that can fail returns `false` and
  leaves its arguments exactly as they were. The tests force every
  allocation to fail in turn to prove it.
- **Zero is a valid value.** `Str s = {};` and `Arena a = {};` are ready to
  use and own nothing.
- **Allocation is explicit.** Code that allocates takes an `Allocator`, and
  null means the libc heap. Point a `Str` at an arena and it grows in place
  inside it.
- **Views over copies.** Splitting, trimming and searching return
  `StrView`s into the original bytes and never allocate.
- **One translation unit.** Each module is a `.h`/`.c` pair. A program
  includes the `.c` files it needs and builds with one compiler call. That
  means nothing to link and lets the optimiser see every function.

## Requirements

- A Unix-like system. macOS and Linux are tested.
- clang 17+ or gcc 13+.
- `make`.
- Optional: `doxygen` for `make docs`, `hyperfine` for `make bench-scale`,
  and `clang-format` for `make format`.

## Using it in a project

Copy `src/` into your tree and add `-Isrc` to your compiler flags. In the one
file that holds `main`, include the `.c` file of every module you use and of
the modules it depends on. `arr` and `str` need `mem`, and `log` needs `mem` and `str`. Every
other file includes only the headers it needs, such as `#include "str/str.h"`.

## Building this repository

| Command | What it does |
| --- | --- |
| `make` | Debug build with AddressSanitizer and UBSan, when the toolchain has them. |
| `make run` | Runs the example word counter in `main.c` on `data/pg2554.txt`. Pass `ARGS=file` for another file. |
| `make test` | Builds and runs every `tests/test_*.c`. `T=name` runs one test. |
| `make release` | Optimised build with `-Werror` and hardening flags. |
| `make docs` | API reference in `build/docs/html/`. |
| `make bench` | Micro-benchmarks for `str`. `bench-corpus` and `bench-scale` run on real data. |

Test output is coloured on a colour terminal. Set `NO_COLOR=1` to turn it
off.

## Layout

```
src/<module>/   one folder per module, holding <module>.h and <module>.c
tests/          one test file per module, plus the test.h harness
bench/          benchmarks against string.h
notes/          guides for learning the library, with runnable examples
main.c          a word counter built on every module
```

## Documentation

- `make docs` builds the API reference from the doc comments in `src/`.
  Every example in it is a region of a test file, so the examples compile.
- `notes/str-and-strview.html` teaches `Str` and `StrView` with programs
  you can run.
- `notes/modern-c-guide.html` explains the C23 features and idioms the code
  uses.
- `AGENTS.md` holds the conventions for anyone changing the code, people
  included.
