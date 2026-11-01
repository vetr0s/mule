# TODO

MULE, Multi-Use Library Essentials, is a personal standard library for C in
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

- [x] `src/log/`: `log_trace` to `log_fatal` and `log_assert`, a runtime
      level, a compile-time floor, checked format strings, one `fwrite` per
      line, coloured levels, and output to any `FILE *`.
- [ ] Reuse a per-thread line buffer instead of allocating one per message,
      if logging ever shows up in a profile.
- [ ] Timestamps, once the time module exists.

### 2. Arena allocator

- [x] A bump allocator in `src/mem/` with aligned allocation and one call
      that frees everything. `Str` draws from it through its `alloc` field.
- [ ] Save and restore marks, so a function can free just what it allocated.
- [ ] Replace the `malloc` macro hooks in `tests/test.h` with a failing
      `Allocator`.

### 3. Input parsing, added to `str`

- [x] `str_read_line`, `sv_parse_i64` and `sv_parse_f64` replace `scanf`:
      read a line, split it with `sv_token_next`, parse each piece.
- [ ] `sv_parse_u64`, and bases other than 10, when a caller needs them.

### 4. Dynamic array

- [x] `src/arr/`: `Arr(T)` with push, pop, insert, remove, reserve, clear
      and free. It shares the `Str` layout and the allocator.

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
- [x] `src/time/`: `time_now_ns`, promoted from `bench/bench.h`.
- [ ] Wall-clock ISO timestamps in `time`, for the logger.
- [x] `src/rand/`: the tests' splitmix64, with an unbiased `rand_below`.

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
- [x] `main.c` is a word counter built on every module, and `bench-scale`
      times it.
- [ ] Turn `main.c` into a word-frequency counter once the hash map exists.
- [ ] `notes/modern-c-guide.html` exercises 2 and 3 ask for `sv_find` and
      `sv_trim`, which already exist. Rewrite or drop them.
- [ ] Run `make test` on Linux with gcc and sanitizers in CI.
- [ ] Fuzz the splitters and parsers with libFuzzer. Apple clang does not ship
      libFuzzer, so this runs on Linux.
