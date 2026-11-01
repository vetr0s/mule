# MULE

MULE, Multi-Use Library Essentials, is a personal standard library for C.
Each module lives in its own folder under `src/`. `TODO.md` is the roadmap.
Check off what you finish and add what you find.

`CLAUDE.md` is a symlink to this file. Edit `AGENTS.md`.

## Layout

The folder, include and naming rules live in the file comment of
`src/base/base.h`. Read it before adding, moving or including a file.

## Conventions

- A function that can fail returns `bool`. A failed call leaves its arguments
  byte-for-byte as they were. `check_unchanged_on_oom` in
  `tests/test_str.c` proves this for every `str` operation, so new
  operations join its table.
- A zero-initialised struct is a valid empty value: `Str s = {};`,
  `Arena a = {};`. New types keep that property.
- Code that allocates takes a `const Allocator *`, and null means the heap.
  It allocates through `mem_realloc`. Only `src/mem/` calls libc
  allocation directly.
- Write a macro only for what a function cannot do: `sizeof` a literal,
  capture `__FILE__` and `__LINE__`, or work for any element type.
- Mark every function whose result must be checked `[[nodiscard]]`.
- A hot path that stores through `s->data` copies the fields it needs into
  locals first, as `str_push` and `str_append` do. A `char` store may alias
  any field of `*s`, so otherwise the compiler reloads them on every call.
- Targets are Unix with clang 17+ or gcc 13+. A feature missing from either
  goes behind a fallback in `base.h`, the way `ckd_add` does.
- Projects use `dist/mule.h`, which `tools/amalgamate.sh` joins from `src/`.
  A new module joins `MODULES` in the Makefile, after the modules it
  includes. Commit the regenerated `dist/mule.h` with every change to
  `src/`. CI fails when it is stale.

## Tests and docs

- Each module has `tests/test_<module>.c`, a unity file that includes
  `test/test.h` first. The harness counts live allocations and fails a test that
  leaks. `test_fail_alloc_after(n)` makes allocation fail on demand.
- Every public declaration has a doc comment. Examples are regions of the
  test files pulled in with `@snippet`, so every documented example compiles
  and runs.
- Watch a new test fail once against a broken implementation before you
  trust it.

## Done

- `make test`, `make release` and `make docs` all pass. `make release`
  builds with `-Werror`. `make docs` fails on any undocumented member.
- When a public API changes, every file in `notes/str-examples/` still
  compiles with `cc -std=c23 -Isrc`. `10_dangling.c` and `11_double_free.c`
  are bugs on purpose and only need to compile.
- `notes/str-and-strview.html` is generated. Edit
  `notes/str-and-strview.tmpl.html` or the examples, then run
  `python3 tools/gen-guide.py`. It inlines each example and the
  standard-html stylesheet from `~/.claude/skills/`.

## Known pitfalls

- `str_with_capacity` and `str_from` initialise. They do not free what the
  `Str` already held, so call `str_free` first on a `Str` that owns memory.
- An `Arena` must stay put once `arena_allocator` has been called. The
  allocator's context points at it.
- Doxygen reads both branches of an `#if`. Wrap internal macros in
  `/** @cond */` and `/** @endcond */`.
- Doxygen wants a comment on each member. A `@name` group comment does not
  count, so give each macro a trailing `/**< ... */`.
- In the Makefile, a `:=` variable that calls a function must come after the
  function's definition, or it expands to nothing.
- macOS ships GNU Make 3.81, which compares timestamps to the second. A
  file edited and rebuilt within one second looks up to date. Use `make -B`
  when a script edits and rebuilds in a loop, such as a mutation check.
- AddressSanitizer on macOS does not detect leaks, and Apple clang ships no
  libFuzzer. The allocation counter in `test/test.h` covers leaks. Fuzzing needs
  Linux.
- macOS 26 libmalloc keeps a large block grown by realloc dirty after free,
  and never reuses it. `mem_realloc` grows by malloc and copy on Apple for
  that reason. `heap_growth_returns_memory` only runs without ASan, because
  ASan replaces libmalloc. Run `make test SANITIZE=` after touching it.

## Benchmarks

- Compare a change by building the benchmark before and after it, then
  running each binary several times. One run misleads, and a change
  elsewhere can move a micro-benchmark 4x by shifting what gets inlined.
  Explain a shift like that from the disassembly before accepting it.
- Measure memory with `/usr/bin/time -l` on macOS. `bench_corpus` holds the
  input and the rebuilt copy, and peaks near 3.5x its largest file.
- `base.h` sets `_POSIX_C_SOURCE`, which hides BSD extensions on macOS,
  such as `ru_maxrss` in `struct rusage`. Define `_DARWIN_C_SOURCE` before it
  in a scratch probe. `<mach/mach.h>` works without it.

## Keeping this file current

When you learn a convention, a pitfall or a better practice that the next
session would need, add it to the section where it belongs. When a line here
turns out wrong or stale, fix or delete it in the same change. Keep each
point to what the code and the Makefile cannot already tell a reader.
