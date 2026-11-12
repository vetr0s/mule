# TODO

MULE, Multi-Use Library Essentials, is a personal standard library for C in
the spirit of stb. This file is the roadmap: what is decided, and what each
release is for. The work items are GitHub issues, and each release is a
milestone that holds its issues. `CHANGELOG.md` records what shipped.

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
      per push, nightly and at release. `dev/specs/v0.4.0.md` has the design.
- [x] Tracking: bugs, features and action items are GitHub issues, filed
      under the milestone of the release that ships them. This file keeps
      the decisions and the goal of each release.

## Releases

| Release | Goal |
| --- | --- |
| [0.3.1](https://github.com/vetr0s/mule/milestone/1) | Wall-clock timestamps in `time`, and in `log` lines on top of them. |
| [0.4.0](https://github.com/vetr0s/mule/milestone/2) | Testing. Every check is a `./build` command, run per push, nightly and at release. The API does not change. `dev/specs/v0.4.0.md` holds the design. |
| [0.5.0](https://github.com/vetr0s/mule/milestone/3) | Text: UTF-8, in-place edits on `Str`, and a parser built on views. |
| [0.6.0](https://github.com/vetr0s/mule/milestone/4) | Threads: a thin wrapper, a job queue, and TSan. |
| [0.7.0](https://github.com/vetr0s/mule/milestone/5) | Windows, with MSVC and clang. |
| [1.0.0](https://github.com/vetr0s/mule/milestone/6) | A stable API: every module has a real caller, and no planned change would break one. |

Ideas that wait for a caller that needs them carry the
[`parked`](https://github.com/vetr0s/mule/labels/parked) label and no
milestone.
