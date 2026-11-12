#pragma once
/**
 * @defgroup mod_build Build
 * @brief Write a project's build as a C program that rebuilds itself.
 *
 * A project keeps a `build.c` that includes `mule.h`, and bootstraps it
 * once with nothing but a C compiler:
 *
 *     cc build.c && ./a.out          # or: cc -o build build.c && ./build
 *
 * From then on MULE_REBUILD_SELF() recompiles the build program whenever
 * `build.c` or anything it includes has changed, and runs the new one in
 * its place. Output goes in `.build/` beside the program.
 *
 * @snippet{trimleft} project/build.c build
 *
 *   - Staleness compares modification times to the nanosecond, so an edit
 *     made in the same second as the last build still counts.
 *   - build_prepare() reads the depfile a compiler writes with `-MMD -MF`,
 *     so a header edit rebuilds everything that includes it. It also keeps
 *     each output's command beside it, so a changed flag rebuilds too.
 *   - build_run() and build_start() log each command, quoted so it can be
 *     pasted into a shell, before running it.
 *   - A CompDb collects compile commands into `compile_commands.json`, for
 *     clangd and other tools.
 * @{
 */

#include "arr/arr.h"
#include "base/base.h"
#include "err/err.h"
#include "proc/proc.h"
#include "str/str.h"

/** @name Staleness
 *  Decide what needs building.
 *  @{ */

/** Set `*needed` when @p out is missing or older than any of @p inputs.
 *  @param out  The file that gets built.
 *  @param inputs  The files it is built from.
 *  @param n  Entries in @p inputs.
 *  @param needed  Set to whether @p out must be rebuilt.
 *  @param err  The error to fill, or null.
 *  @return False when an input is missing (#ERR_NOT_FOUND) or cannot be
 *  read. `*needed` is then unchanged. */
MULE_NODISCARD bool build_needs_rebuild(const char *out,
                                        const char *const *inputs, size_t n,
                                        bool *needed, Err *err);

/** Prerequisites listed in a depfile, borrowed from `text`. */
typedef Arr(const char *) DepList;

/** A depfile's prerequisites. Zero-initialise it and free it with
 *  build_deps_free(). */
typedef struct Deps {
    Str text;      /**< The prerequisites, each NUL-terminated. */
    DepList paths; /**< One pointer into `text` per prerequisite. */
} Deps;

/** Read the first rule of a depfile, as `cc -MMD -MF` writes one, into
 *  @p d. It handles continued lines, `\ ` in names and `$$`.
 *  @return False when the file cannot be read or memory runs out. @p d is
 *  then unchanged. */
MULE_NODISCARD bool build_read_deps(Deps *d, const char *depfile, Err *err);
/** Free @p d and reset it to empty. Safe to call twice. */
void build_deps_free(Deps *d);
/** Set `*needed` when @p out is missing, @p depfile is missing, or any
 *  prerequisite in @p depfile is newer than @p out or gone. A missing
 *  prerequisite means rebuild rather than fail, because a header that was
 *  deleted is no longer included.
 *  @return False when a file cannot be read. `*needed` is then unchanged. */
MULE_NODISCARD bool build_needs_rebuild_deps(const char *out,
                                             const char *depfile, bool *needed,
                                             Err *err);
/** Set `*needed` as build_needs_rebuild_deps() does, and also when @p c
 *  differs from the command that last built @p out, which is kept in
 *  `<out>.cmd`. When it is needed, remove @p out and record @p c, so a
 *  run of @p c that fails is retried next time.
 *  @return False when a file cannot be read, written or removed. */
MULE_NODISCARD bool build_prepare(const char *out, const char *depfile,
                                  const Cmd *c, bool *needed, Err *err);

/** @} */

/** @name Rebuilding itself
 *  Keep the build program up to date.
 *  @{ */

/** Rebuild the running program from the file this macro is written in,
 *  when that file or a header it includes has changed. Call it first in
 *  main. See build_rebuild_self(). */
#define MULE_REBUILD_SELF(argc, argv)                                          \
    build_rebuild_self((argc), (argv), __FILE__, NULL)

/** Rebuild the running program from @p source when it is stale, then run
 *  the new program with the same arguments in place of this one, so its
 *  exit code is the caller's. Returns only when nothing needed rebuilding,
 *  or @p source cannot be found from the current directory.
 *
 *  The program is stale when its depfile, `.build/<name>.d` beside it, is
 *  missing or lists a newer file. It compiles with `$CC`, or `cc`, and
 *  `-Wall -Wextra -MMD`. The old program waits in `.build/<name>.old`, and
 *  goes back in place when the compile fails, which exits with status 1.
 *  @param argc  main's argc. Unused, and kept so the call reads like main.
 *  @param argv  main's argv, passed to the new program.
 *  @param source  The program's source, as a path from the current
 *  directory.
 *  @param flags  More compiler arguments, such as `-Isrc`, or null. */
void build_rebuild_self(int argc, char **argv, const char *source,
                        const Cmd *flags);

/** @} */

/** @name Running
 *  Run commands and record them.
 *  @{ */

/** Log @p c with log_info(), quoted, then run it with cmd_run().
 *  @return False as for cmd_run(). */
MULE_NODISCARD bool build_run(const Cmd *c, Err *err);
/** Log @p c with log_info(), quoted, then start it with proc_pool_start().
 *  @return False as for proc_pool_start(). */
MULE_NODISCARD bool build_start(ProcPool *pool, const Cmd *c, Err *err);

/** Compile commands, for `compile_commands.json`. Zero-initialise it and
 *  free it with build_compdb_free(). */
typedef struct CompDb {
    Str json; /**< One JSON fragment per command, each ending in a newline. */
} CompDb;

/** Record that @p c compiles @p file.
 *  @return False when out of memory. @p db is then unchanged. */
MULE_NODISCARD bool build_compdb_add(CompDb *db, const char *file,
                                     const Cmd *c);
/** Write @p db to @p path as a JSON compilation database, with the current
 *  directory as each entry's `directory`. The file is left alone when it
 *  already holds the same text, so tools that watch it do not reload.
 *  @return False when the directory or the file cannot be read or written,
 *  or memory runs out. */
MULE_NODISCARD bool build_compdb_write(const CompDb *db, const char *path,
                                       Err *err);
/** Free @p db. Safe to call twice. */
void build_compdb_free(CompDb *db);

/** @} */

/** @} */
