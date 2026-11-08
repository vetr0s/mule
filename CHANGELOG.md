# Changelog

Every release of MULE, newest first. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the versions
follow [Semantic Versioning](https://semver.org/spec/v2.0.0.html). Before
1.0.0, a minor release may break the API and a patch release does not.

## [Unreleased]

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

[Unreleased]: https://github.com/vetr0s/mule/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/vetr0s/mule/releases/tag/v0.0.1
