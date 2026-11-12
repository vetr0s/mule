# TODO

MULE, Multi-Use Library Essentials, is a personal standard library for C in
the spirit of stb. This file is the roadmap, grouped by the release that
ships each item. `CHANGELOG.md` records what already shipped, so a release
deletes its checked items from here.

While the major version is 0, a new module bumps the minor version and an
addition to an existing module bumps the patch. Each milestone below
follows that rule.

## Decided

- [x] Packaging: `.h`/`.c` pairs with a unity build, one folder per module
      under `src/`. The layout rules live in `src/base/base.h`.
- [x] A shared `src/base/base.h`, included first by every header.
- [x] Allocation: code that allocates takes an `Allocator`, and null means
      the libc heap.
- [x] Errors: functions return `bool`, and a failed call leaves its argument
      unchanged. Macros only where a function cannot do the job.
- [x] Error reasons: a call that can fail more than one way also takes an
      optional `Err *` last, filled on false. Never `errno`.
      `docs/concepts.md` has the rule.
- [x] Naming: one prefix per module. Never name anything plain `log`.
- [x] Platforms: Unix with clang 17+ or gcc 13+. Windows comes in 0.7.0, so
      new platform code keeps its POSIX calls behind one function per call
      site.
- [x] Language: a C11 floor, written in C99 style.
- [x] Versions: Semantic Versioning, released at milestones. AGENTS.md has
      the rules and the steps.
- [x] The reference is published to https://mule.vetr0s.dev by CI.
- [x] Builds: from 0.3.0, MULE builds itself with a `build.c` written
      against its own `build` module, in the style of tsoding's nob.h. The
      Makefile goes away.
- [x] Testing: every kind of check is a `./build` profile or command, so
      CI is a list of `./build` calls that reproduce locally. Checks run
      per push, nightly and at release. `specs/v0.4.0.md` has the design.

## 0.3.x: patches on 0.3.0

- [ ] Wall-clock ISO 8601 timestamps in `time`.
- [ ] Timestamps in `log`, on top of them.

## 0.4.0: testing

The library's API stays as it is. `specs/v0.4.0.md` holds the design and
the acceptance criteria.

- [x] Profiles in `build.c`, each building into `.build/<name>/`, and
      `./build ci` for the push tier. They replace `--no-sanitize`.
- [ ] `./build ci --tier=nightly`, with the first nightly check.
- [ ] ASan poisoning for arenas, so an overflow between two arena
      allocations is reported.
- [ ] Consumer builds: `mule.h` in two linked files, under c11, c17 and
      c23, with `-DLOG_LEVEL_MIN`, and `-Wall -Werror` on gcc and clang.
- [ ] GitHub annotations for failed checks under `GITHUB_ACTIONS`.
- [ ] libFuzzer targets for the parsers, the splitters, `args` and the
      depfile reader, short per push and long nightly. Apple clang ships
      no libFuzzer, so they run on Linux.
- [ ] A nightly workflow.
- [ ] An MSan profile.
- [ ] A Fil-C trial, then a nightly profile if it earns one. Its
      toolchain in CI is approved.
- [ ] Allocation budgets in unit tests.
- [ ] Instruction counts under cachegrind against a committed baseline.
      valgrind in CI is approved.
- [ ] `./build bench-compare <ref>`, `./build mutate <file>` and a
      coverage profile.
- [ ] A golden output for `main.c` on `data/pg2554.txt`.
- [ ] The Release workflow builds an example against the `mule.h` it
      just published.

## 0.5.0: text

- [ ] `utf8`: validate a view, decode the next code point, count code
      points.
- [ ] In-place edits on `Str`: insert, remove a range, replace.
- [ ] An INI or JSON parser built on views.

## 0.6.0: threads

- [ ] A thin wrapper over `<threads.h>` or pthreads.
- [ ] A job queue on top of it.
- [ ] A TSan profile in the nightly tier.

## 0.7.0: Windows

- [ ] MSVC: a `ckd_*` fallback without the gcc builtins.
- [ ] Windows versions of the POSIX calls in `log`, `rand`, `time`, `fs`
      and `proc`. `proc` uses `CreateProcess`.
- [ ] CI on Windows with MSVC and clang.

## 1.0.0: a stable API

1.0.0 ships when these hold.

- [ ] Every module has a real caller in this repo: `main.c`, `build.c`, a
      benchmark or an example.
- [ ] No planned change would break a caller.

## When a caller needs it

- [ ] An ASCII `sv_eq_ignore_case`.
- [ ] Seed the map hash, if a map is ever keyed on untrusted input.
- [ ] Reuse a per-thread log line buffer, if logging shows up in a profile.
- [ ] Pipe one `Cmd` into another, as nob's chain API does.
