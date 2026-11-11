# Changelog

Every release of MULE, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the versions
follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Before
1.0.0, a minor release may break the API and a patch release does not.

## [Unreleased]

### Added

- `err`: `Err`, which says why a call that can fail more than one way
  failed. It holds a portable `ErrKind`, the raw code and a message, and
  needs no allocation. `err_set` fills one, and `err_os` fills one from an
  `errno` value.
- `sv_find_last`, `sv_trim_left` and `sv_trim_right`.
- `fs`: files, directories and paths. Whole-file reads and writes, an
  atomic write, `fs_mkdir_p`, `fs_remove_all`, copying files and trees, a
  sorted `fs_walk`, `FsStat` with nanosecond modification times, the
  running program's path, and `StrView` path pieces.
- `args`: command-line parsing from a table of options, with subcommands,
  positionals and generated `--help`. A failed parse changes nothing.

## [0.2.0] - 2026-11-08

### Added

- `sv_parse_u64`: parses a whole view as an unsigned integer in bases 2 to
  36.
- `arena_mark` and `arena_restore`: a function can free just what it
  allocated from an arena.
- `mem_set_heap`: installs an `Allocator` behind every null allocator and
  every arena block.

### Changed

- Breaking for `MULE_TEST` users: the test harness no longer redefines
  `malloc`, `calloc`, `realloc` and `free`. `test_init` installs a counting,
  failing `Allocator` with `mem_set_heap` instead. A direct `malloc` that leaks
  no longer fails a test. A direct `free` of library memory leaves the count
  raised, so a correct test fails with a false leak. To migrate, allocate
  through `mem_realloc` or an `Allocator`, and do not free library memory with
  `free`.
- The examples in `notes/str-examples/` and the Str and StrView guide include
  `dist/mule.h`, as a project does. `make examples` compile-checks them, and
  CI runs it.

## [0.1.0] - 2026-11-07

### Added

- `map`: `Map(E)`, a hash map from `StrView` keys to entries of any struct
  type. Entries stay in insertion order in an array until a remove, which
  moves the last entry into the hole. The map borrows its keys rather than
  copying them.
- `map_remove`: removes a key by moving the last entry into its place. It
  never allocates.
- `str_take`: moves the buffer out of a `Str` and leaves the source empty.

## [0.0.1] - 2026-11-07

The first release. `mule.h` is the whole library as one header.

### Added

- `base`: the POSIX feature macro, `bool`, checked arithmetic with
  `ckd_add`, `ckd_sub` and `ckd_mul`, and the version macros.
- `mem`: the `Allocator` interface, and an `Arena` that frees everything at
  once and grows its newest allocation in place.
- `str`: `Str`, an owned growable byte string, and `StrView`, a borrowed
  slice. Appending, formatting, reading files and lines, search, slicing,
  trimming, splitting, tokenising, and integer and float parsing.
- `arr`: `Arr(T)`, a growable array of any element type with the layout and
  rules of `Str`.
- `rand`: a seeded splitmix64 generator with an unbiased `rand_below`.
- `time`: a monotonic nanosecond clock.
- `log`: levelled, coloured messages with `file:line`, and an assert that
  stays in release builds.
- `test`: a unit-test harness that fails tests that leak or hang, and makes
  allocation fail on demand.

[Unreleased]: https://github.com/vetr0s/mule/compare/v0.2.0...HEAD
[0.2.0]: https://github.com/vetr0s/mule/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/vetr0s/mule/compare/v0.0.1...v0.1.0
[0.0.1]: https://github.com/vetr0s/mule/releases/tag/v0.0.1
