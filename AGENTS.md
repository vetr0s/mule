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
- Targets are Unix with clang 17+ or gcc 13+. A feature missing from either
  goes behind a fallback in `base.h`, the way `ckd_add` does.

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
  compiles and `notes/str-and-strview.html` still matches it.

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
- macOS libmalloc keeps a large block that grew through realloc resident
  after free. Peak memory climbs by the block's size on every repeat, so
  `bench_corpus` peaks near 8x its largest input. One malloc of the final
  size does not climb. Check peak RSS before raising `SCALE_MIB`.

## Keeping this file current

When you learn a convention, a pitfall or a better practice that the next
session would need, add it to the section where it belongs. When a line here
turns out wrong or stale, fix or delete it in the same change. Keep each
point to what the code and the Makefile cannot already tell a reader.
