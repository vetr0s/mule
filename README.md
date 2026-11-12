# MULE

[![CI](https://github.com/vetr0s/mule/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/vetr0s/mule/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/vetr0s/mule)](https://github.com/vetr0s/mule/releases/latest)
[![Reference](https://img.shields.io/badge/reference-mule.vetr0s.dev-blue)](https://mule.vetr0s.dev)

Multi-Use Library Essentials. A small standard library for C11, written in
the style of C99 and the spirit of stb. It is written to be read as well as
used.

It is early. Fourteen modules exist. `dev/TODO.md` holds the roadmap, and the
work is tracked in [issues](https://github.com/vetr0s/mule/issues) under
one milestone per release.

| Module | What it gives you |
| --- | --- |
| `base` | The header every module includes first: feature macros and checked arithmetic. |
| `mem` | An allocator interface and an arena that frees everything in one call. |
| `err` | `Err`, which says why a call failed: a kind to branch on, the OS code and a message. |
| `arr` | `Arr(T)`, a growable array of any element type, with the same layout and rules as `Str`. |
| `map` | `Map(E)`, a hash map from `StrView` keys to entries of any struct type. Entries stay in insertion order until a remove, which moves the last entry into the hole. |
| `rand` | A seeded splitmix64 generator that gives the same sequence on every platform, with an unbiased `rand_below`. |
| `str` | `Str`, an owned growable byte string, and `StrView`, a borrowed slice with search, split and trim. |
| `time` | A monotonic nanosecond clock for timing code. |
| `log` | Levelled messages with `file:line`, coloured when the terminal supports it, and an assert that stays in release builds. |
| `fs` | Files, directories and paths: read and write whole files, atomic writes, walk a tree, cut a path into pieces. |
| `args` | Command lines: long and short options, subcommands and positionals from one table, which also writes `--help`. |
| `proc` | Run programs: wait for one, capture its output, redirect its streams to files, or run many at once in a pool. |
| `build` | Write a project's build in C: a build program that rebuilds itself, nanosecond staleness with header tracking from depfiles, and `compile_commands.json`. |
| `test` | A unit-test harness that fails tests on leaks and hangs, and makes allocation fail on demand. |

## A taste

```c
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>

int main(void) {
    Arena arena = { 0 };
    Str csv = { .alloc = arena_allocator(&arena) };
    if (!str_appendf(&csv, "%s,%s,%d", "mule", "c99", 4)) return 1;

    StrView rest = str_view(&csv), field;
    while (sv_split_next(&rest, SV(","), &field))
        printf("[%.*s]\n", STR_FMT(field));

    arena_free(&arena); /* frees csv along with everything else */
    return 0;
}
```

```sh
cc -std=c11 -Idist example.c -o example && ./example
```

It prints `[mule]`, `[c99]` and `[4]`, each on its own line.

## Design

- **Failure is a `bool`.** A function that can fail returns `false` and
  leaves its arguments exactly as they were. The tests force every
  allocation to fail in turn to prove it.
- **Zero is a valid value.** `Str s = { 0 };` and `Arena a = { 0 };` are
  ready to use and own nothing.
- **Allocation is explicit.** Code that allocates takes an `Allocator`, and
  null means the libc heap. Point a `Str` at an arena and it grows in place
  inside it.
- **Views over copies.** Splitting, trimming and searching return
  `StrView`s into the original bytes and never allocate.
- **One header to use, many files to read.** Each module is a `.h`/`.c`
  pair under `src/`. `./build dist` joins them into `dist/mule.h`, so a project
  copies one file and has nothing to link.

## Requirements

- A Unix-like system. macOS and Linux are tested.
- clang 17+ or gcc 13+.
- Optional: `doxygen` 1.18 for `./build docs`, `hyperfine` for
  `./build bench-scale`, and `clang-format` for `./build format`.
  `./build data` downloads one book with `curl`. `./build bench-corpus`
  downloads about 1.6 GB with `curl`, `gunzip` and
  `unzip`, and needs about 4 GB of RAM.

## Using it in a project

Download `mule.h` from the
[latest release](https://github.com/vetr0s/mule/releases/latest), or copy
`dist/mule.h` for the newest code. Include it in any file that uses the
library. In exactly one `.c` file, define `MULE_IMPLEMENTATION` before the
include to compile the library into that file. Include it before any system
header, because it sets `_POSIX_C_SOURCE`.

For tests, define `MULE_TEST` too and the `test` harness comes with it.
`test_init` installs an `Allocator` as the heap, so the leak counting and
allocation failure reach the library's own allocations. `tests/test_dist.c`
is a complete example.

Releases follow Semantic Versioning, and `CHANGELOG.md` lists what changed
in each. Before 1.0.0, a minor release may break the API.

## Building this repository

The build is a C program, `build.c`. Compile it once:

```sh
cc -Isrc -o build build.c
```

After that it rebuilds itself whenever `build.c` or the library changes.
Output goes to `.build/`, or to `--out=DIR`. The compiler is `$CC`, or
`cc`. `./build --help` lists every command and option.

A profile is a set of flags, and each one builds into its own folder, so
switching profiles never rebuilds another one's output. `--profile` picks
one for `debug`, `run`, `test` and `examples`:

| Profile | Flags |
| --- | --- |
| `debug` | `-O0` with AddressSanitizer and UBSan, when the toolchain has them. The default. |
| `nosan` | `-O0` without sanitizers, so the real allocator runs. |
| `release` | `-O3`, LTO, hardening and `-Werror`. |

| Command | What it does |
| --- | --- |
| `./build` | Debug build with AddressSanitizer and UBSan, when the toolchain has them, and `compile_commands.json`. |
| `./build run [file]` | Runs the example word counter in `dev/main.c` on `data/pg2554.txt`, or on `file`. |
| `./build data` | Downloads `data/pg2554.txt`, the book that `dev/main.c` and the str guide read. `run` does it when the file is missing. |
| `./build test [names]` | Builds and runs every `tests/test_*.c`. Names run only those tests. |
| `./build ci` | Every check CI runs on a push except the docs: a stale `dist/mule.h`, the tests under each profile, the release build, the examples and the example project. |
| `./build release` | Optimised build with `-Werror` and hardening flags, and the benchmarks. |
| `./build dist` | Regenerates `dist/mule.h` from `src/`. Builds of `main` and the tests do it too. |
| `./build docs` | API reference in `.build/docs/html/`. |
| `./build examples` | Builds and runs `docs/examples/`, and compile-checks `docs/guide/str-examples/` against `dist/mule.h`. |
| `./build bench` | Micro-benchmarks for `str`. `bench-corpus` and `bench-scale` run on real data. |

Test output is coloured on a colour terminal. Set `NO_COLOR=1` to turn it
off.

## Layout

```
src/<module>/   one folder per module, holding <module>.h and usually <module>.c
dist/mule.h     the single-header library, generated from src/ and committed
build.c         the build, which also generates dist/mule.h
tests/          one test file per module, and test_dist.c for dist/mule.h
docs/           what a reader reads: the Doxygen inputs and these two
  examples/     one small program per module, and a project built by build.c
  guide/        guides for learning the library, with runnable examples
dev/            what a maintainer uses
  bench/        benchmarks against string.h
  tools/        the generator for the guide
  specs/        designs, one per release
  reviews/      code reviews against those designs
  main.c        a word counter that uses dist/mule.h
  TODO.md       decisions and the goal of each release
```

## Documentation

- The reference is online at https://mule.vetr0s.dev, built from `main`.
- `./build docs` builds the reference in `.build/docs/html/`. It has a topic
  per module, a page on the rules every module shares (`docs/concepts.md`),
  and the example programs with every library call linked. Every snippet in
  it is a region of a test file, so the examples compile.
- `docs/guide/str-and-strview.html` teaches `Str` and `StrView` with programs
  you can run.
- `AGENTS.md` holds the conventions for anyone changing the code, people
  included.
