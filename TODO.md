# TODO

MULE, Multi-User Library Essentials, is a personal standard library for C in
the spirit of stb. This file tracks what comes next and what the existing
modules still need.

## Decided

- [x] Packaging: `.h`/`.c` pairs with a unity build, one folder per module
      under `src/`. The layout rules live in `src/base/base.h`.
- [x] A shared `src/base/base.h`, included first by every header.
- [x] Allocation: `Str` gets an allocator pointer, null meaning the libc
      heap. The arena comes next, and every module uses it.
- [x] Errors: functions return `bool`, and a failed call leaves its argument
      unchanged. Macros only where a function cannot do the job.
- [x] Naming: one prefix per module. Never name anything plain `log`.
- [x] Platforms: Unix with clang 17+ or gcc 13+.

## Modules, in build order

### 1. Logger

- [ ] Levels from trace to fatal. Set the threshold at runtime, plus a
      compile-time floor (`LOG_LEVEL_MIN`) that removes the calls below it.
- [ ] Macros that capture `__FILE__`, `__LINE__` and `__func__`.
- [ ] Format strings checked by the compiler with `gnu::format`, the same way
      `str_appendf` is checked.
- [ ] Format each line into a reused `Str`, then write it with one `fwrite`.
      stdio locks the stream for each call, so lines from different threads
      do not interleave.
- [ ] Output to a `FILE *`, with `stderr` as the default. Color only when
      `isatty` says the output is a terminal.
- [ ] Fatal flushes the log, then calls `abort`. Build `ASSERT` and `PANIC` on
      top of it.
- [ ] Tests write to `tmpfile()` and read the result back, like the
      `append_file` tests.

### 2. Arena allocator

- [x] A bump allocator in `src/mem/` with aligned allocation and one call
      that frees everything. `Str` draws from it through its `alloc` field.
- [ ] Save and restore marks, so a function can free just what it allocated.
- [ ] Replace the `malloc` macro hooks in `tests/test.h` with a failing
      `Allocator`.

### 3. Input parsing, added to `str`

This replaces `scanf`. Do not build a format-string scanner. `printf` with
`STR_FMT`, plus `str_appendf`, already covers the output side.

- [ ] `str_read_line(Str *, FILE *)` reads one line without the `\n`. It must
      handle embedded NULs, so use `getc` and not `fgets`.
- [ ] `sv_parse_i64` and `sv_parse_f64` must consume the whole view or fail,
      and they reject overflow. `strtol` accepts leading spaces and
      `strtoull` accepts `"-1"`. These functions accept neither.
- [ ] The `scanf("%d %d")` pattern then becomes `str_read_line`, then
      `sv_token_next`, then `sv_parse_i64`.

### 4. Dynamic array

- [ ] Typed through macros, so a growable array of any element type needs no
      casts. Reuse the growth policy from `str_grow_`.

### 5. Hash map keyed by `StrView`

- [ ] Open addressing. The main design question is who owns the key bytes:
      the caller, the map, or an arena.
- [ ] It enables word counts, symbol tables and deduplication.

### 6. Files and paths

- [ ] Read a whole file from a path in one call.
- [ ] Write a file atomically: write to a temporary file, then `rename`.
- [ ] Path functions that take a `StrView`: `basename`, `dirname`, extension,
      join.

### 7. Argument parser

- [ ] Handles `--name=value`, `-v`, and positional arguments. Generates
      `--help` output.

### 8. Promote what already exists

- [ ] `tests/test.h` becomes a module. Its allocation counting and
      out-of-memory injection are worth keeping in every project.
- [ ] `bench/bench.h` becomes a module, with `now_ns` as the start of a time
      module: monotonic clock, stopwatch, ISO timestamps for the logger.
- [ ] `next_random` in `tests/test_str.c` becomes a seeded PRNG module.

### Later

- [ ] UTF-8: validate a view, decode the next code point, count code points.
- [ ] In-place edits on `Str`: insert, remove a range, replace.
- [ ] Threads: a thin wrapper over `<threads.h>` or pthreads, then a job queue.
- [ ] An INI or JSON parser built on views.
- [ ] Run a process and capture its output into a `Str`.

## Existing work on `str`

- [ ] `SV(p)` accepts a pointer and silently uses `sizeof(char *) - 1` as the
      length, which is 7 on this machine. Change the body to `"" lit ""` so a
      pointer fails to compile.
- [ ] Add `str_take(Str *)`, which returns the string and zeroes the source.
      It makes moves explicit. Today the guide tells readers to write
      `b = a; a = (Str){};`.
- [ ] Add `sv_find_last`, `sv_trim_left`, `sv_trim_right` and an ASCII
      `sv_eq_ignore_case` when a real caller needs them, and not before.
- [ ] `main.c` only prints `argv`. Replace it with a real program, such as a
      word-frequency counter once the hash map exists. It doubles as an
      integration test.
- [ ] `notes/modern-c-guide.html` exercises 2 and 3 ask for `sv_find` and
      `sv_trim`, which already exist. Rewrite or drop them.
- [ ] Run `make test` on Linux with gcc and sanitizers in CI.
- [ ] Fuzz the splitters and parsers with libFuzzer. Apple clang does not ship
      libFuzzer, so this runs on Linux.
