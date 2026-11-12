# Changelog

Every release of MULE, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the versions
follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Before
1.0.0, a minor release may break the API and a patch release does not.

## [Unreleased]

### Added

- Build profiles. `--profile=debug`, `nosan` or `release` picks the flags
  for `./build`, `run`, `test` and `examples`, and each builds into
  `.build/<profile>/`.
- `./build ci` runs every check CI runs on a push except the docs. Both
  workflows now call it.
- Arenas under AddressSanitizer poison what they have not handed out and
  put a redzone after each allocation, so an overflow is reported.
- `MULE_ASAN`, which is 1 when AddressSanitizer is on.
- `./build format --check` reports unformatted files without changing
  them, and CI runs it. `notes/str-examples/` is formatted too.

### Changed

- `--profile=nosan` replaces `--no-sanitize`. Test binaries moved from
  `.build/tests/` to `.build/<profile>/tests/`.
- `data/pg2554.txt` is no longer in the repository. `./build data`
  downloads it, and `./build run` does too when it is missing.

### Fixed

- The test harness compiles with `-Werror` under glibc's
  `_FORTIFY_SOURCE`. Its hang report ignored what `write` returned, which
  glibc marks `warn_unused_result`.
- `proc_pool_start` builds with `-Werror` under gcc 13 with LTO, which
  reported a null dereference that could not happen.

## [0.3.0] - 2026-11-11

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
- `proc`: run programs. `Cmd` borrows its arguments and renders them as a
  pasteable shell line. `cmd_run` and `cmd_capture` wait, `proc_start` does
  not, and a `ProcPool` caps how many run at once and names the program
  in a failed job's message. A missing program, an exit code and a signal
  each come back as their own `ErrKind`.
- `build`: a project's build as a C program. `MULE_REBUILD_SELF` rebuilds
  the build program when its source or any header it includes changes,
  then runs the new one with the same arguments and exit code. Staleness
  compares nanosecond times and reads `-MMD` depfiles, and `build_prepare`
  also rebuilds when an output's command changes. Commands are logged
  quoted before they run, and `CompDb` writes `compile_commands.json`.
  `examples/project/` is a complete example.
- `examples/`: a small program for each module, listed in the reference
  beside the `str` guide's examples. `./build examples` builds and runs
  them all.

### Changed

- The repository builds with `build.c` instead of `make`. Bootstrap it
  with `cc -Isrc -o build build.c`, then run `./build --help`. Output moves
  from `build/` to `.build/`.

### Removed

- The Makefile and `tools/amalgamate.sh`.

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

[Unreleased]: https://github.com/vetr0s/mule/compare/v0.3.0...HEAD
[0.3.0]: https://github.com/vetr0s/mule/compare/v0.2.0...v0.3.0
[0.2.0]: https://github.com/vetr0s/mule/compare/v0.1.0...v0.2.0
[0.1.0]: https://github.com/vetr0s/mule/compare/v0.0.1...v0.1.0
[0.0.1]: https://github.com/vetr0s/mule/releases/tag/v0.0.1
