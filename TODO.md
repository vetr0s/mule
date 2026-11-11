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
- [x] Platforms: Unix with clang 17+ or gcc 13+. Windows comes in 0.6.0, so
      new platform code keeps its POSIX calls behind one function per call
      site.
- [x] Language: a C11 floor, written in C99 style.
- [x] Versions: Semantic Versioning, released at milestones. AGENTS.md has
      the rules and the steps.
- [x] The reference is published to https://mule.vetr0s.dev by CI.
- [x] Builds: from 0.3.0, MULE builds itself with a `build.c` written
      against its own `build` module, in the style of tsoding's nob.h. The
      Makefile goes away.

## 0.3.0: build in C

A project's build is a C program. Bootstrapping it needs only a C compiler:
`cc -o build build.c && ./build`. After that the binary rebuilds itself
whenever its source changes. nob.h does this. MULE goes further where nob
cuts corners: sub-second mtimes, header tracking, `compile_commands.json`
and exit codes that pass through. 0.3.0 ships when MULE builds itself this
way and the Makefile is gone.

### `err`: why a call failed

- [x] Implement `err_set` and `err_os` against the sketch in
      `src/err/err.h`. `err_os` maps `errno` to an `ErrKind`, and its reason
      comes from the XSI `strerror_r`, because `strerror` is not thread safe.
- [x] Add `err` to `MODULES` after `str`, with `tests/test_err.c`. Turn the
      header's example into a `@snippet`, and add `err` to the module table
      in `docs/concepts.md`.

### `fs`: files and paths

- [ ] Read a whole file from a path into a `Str`, on top of
      `str_append_file`.
- [ ] Write a file, and write one atomically: write to a temporary file in
      the same directory, then `rename`.
- [ ] `fs_mkdir_p`, `fs_exists`, `fs_remove`, `fs_rename`, and a file type
      from `lstat`: regular, directory, symlink or other.
- [ ] `fs_mtime_ns`: modification time in nanoseconds, from `st_mtim` on
      Linux and `st_mtimespec` on macOS. nob compares whole seconds, which
      misses an edit made in the same second as the build.
- [ ] Copy a file and keep its mode. Copy a directory tree.
- [ ] Walk a directory tree, with a way to skip a subtree or stop.
- [ ] The path of the running executable: `/proc/self/exe` on Linux and
      `_NSGetExecutablePath` on macOS. `argv[0]` is wrong when the binary
      was found on `PATH`.
- [ ] Path functions on `StrView`: basename, dirname, extension, and join
      into a `Str`.

### `proc`: run programs

- [ ] `Cmd`, an `Arr` of argument strings with an append for several at
      once.
- [ ] Render a `Cmd` into a `Str` with shell quoting, so a logged command
      can be pasted back into a shell.
- [ ] Run a `Cmd` and wait: `fork`, `execvp`, `waitpid`. False on a nonzero
      exit or a signal, with the exit code or signal number available.
- [ ] Start a `Cmd` without waiting. A pool caps how many run at once, and
      defaults to the CPU count from `sysconf`.
- [ ] Redirect stdin, stdout and stderr to files.
- [ ] Capture a command's stdout into a `Str`.
- [ ] A process handle the caller does not look inside, so a Windows
      backend fits in 0.6.0 without changing callers.

### `args`: command lines

- [ ] `--name=value`, `--flag`, `-v`, positional arguments and
      subcommands. `./build test --sanitize=off` needs all of them.
- [ ] Generated `--help` output.

### `build`: the nob part

- [ ] `build_needs_rebuild(out, inputs, n)` on nanosecond mtimes. A missing
      output means rebuild, and a missing input is an error.
- [ ] Read a `-MMD` depfile, so a header edit rebuilds every object that
      includes it. nob has no header tracking.
- [ ] `MULE_REBUILD_SELF(argc, argv)`. The binary is stale when `build.c`
      or any file its depfile lists is newer: `mule.h` in a project, the
      `src/` files in MULE. It renames the old
      binary to `.old`, recompiles with `$CC` or `cc` and warnings on, and
      puts the old one back if the compile fails. Then it `execv`s the new
      binary with the same arguments, so the exit code passes through. nob
      runs a child and exits 0 or 1 instead.
- [ ] Log each command through `log`, quoted, before it runs.
- [ ] Write `compile_commands.json` from the compile commands a build ran.
      The Makefile writes one today and nob does not.
- [ ] A minimal example project with its own `build.c`, compiled in CI.

### Docs

- [ ] Revamp the examples section of the docs. It shows wider use cases and
      covers every topic in the library.

### MULE builds itself

- [x] `build.c` includes the `src/` files, not `dist/mule.h`. The build
      generates `dist/mule.h`, so compiling from it would run a stale copy
      of the library. MULE bootstraps with `cc -Isrc -o build build.c`, and
      the self-rebuild adds `-Isrc` itself. Other projects keep `mule.h`.
- [ ] `build.c` at the repo root covers every Makefile target: debug,
      release with the benchmarks, test with the sanitizer and build
      directory options, dist, docs, version, format, the bench and corpus
      targets, `compile_commands.json` and clean.
- [ ] Port `tools/amalgamate.sh` to C inside `build.c`.
- [ ] CI and the Release workflow bootstrap with
      `cc -Isrc -o build build.c`.
- [ ] Delete the Makefile and `tools/amalgamate.sh`. Update the commands
      and pitfalls in AGENTS.md. The GNU Make 3.81 one-second pitfall goes
      with them.

## 0.3.x: patches on 0.3.0

- [ ] Wall-clock ISO 8601 timestamps in `time`.
- [ ] Timestamps in `log`, on top of them.

## 0.4.0: text

- [ ] `utf8`: validate a view, decode the next code point, count code
      points.
- [ ] In-place edits on `Str`: insert, remove a range, replace.
- [ ] An INI or JSON parser built on views.

## 0.5.0: threads

- [ ] A thin wrapper over `<threads.h>` or pthreads.
- [ ] A job queue on top of it.

## 0.6.0: Windows

- [ ] MSVC: a `ckd_*` fallback without the gcc builtins.
- [ ] Windows versions of the POSIX calls in `log`, `rand`, `time`, `fs`
      and `proc`. `proc` uses `CreateProcess`.
- [ ] CI on Windows with MSVC and clang.

## 1.0.0: a stable API

1.0.0 ships when these hold.

- [ ] Every module has a real caller in this repo: `main.c`, `build.c`, a
      benchmark or an example.
- [ ] libFuzzer fuzzes the splitters, the parsers, `args` and the depfile
      reader in CI on Linux. Apple clang ships no libFuzzer.
- [ ] No planned change would break a caller.

## When a caller needs it

- [ ] `sv_find_last`, `sv_trim_left`, `sv_trim_right` and an ASCII
      `sv_eq_ignore_case`.
- [ ] Seed the map hash, if a map is ever keyed on untrusted input.
- [ ] Reuse a per-thread log line buffer, if logging shows up in a profile.
- [ ] Pipe one `Cmd` into another, as nob's chain API does.
