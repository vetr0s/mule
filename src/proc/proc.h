#pragma once
/**
 * @defgroup mod_proc Proc
 * @brief Run other programs, wait for them, and run several at once.
 *
 * A Cmd is an Arr of argument strings, the program first. It borrows them,
 * so each string must outlive every run of the command. cmd_run() starts
 * the program and waits for it. proc_start() starts it and returns a Proc
 * to wait on later, and a ProcPool caps how many run at once.
 *
 * @snippet{trimleft} test_proc.c proc
 *
 *   - The program is found on `PATH` when its name has no slash.
 *   - A run succeeds when the program exits 0. Otherwise the Err says why:
 *     #ERR_NOT_FOUND when the program does not exist, #ERR_EXIT with the
 *     exit status in `code`, or #ERR_SIGNAL with the signal number.
 *   - The child inherits stdin, stdout and stderr unless a CmdIo redirects
 *     them to files.
 *
 * A Proc is a handle to look at through these functions only. Its field
 * holds a process id today and will hold a Windows handle in 0.6.0.
 * @{
 */

#include "arr/arr.h"
#include "base/base.h"
#include "err/err.h"
#include "str/str.h"

/** @name Commands
 *  Build a command and print it.
 *  @{ */

/** A program and its arguments, borrowed. Zero-initialise it and free it
 *  with arr_free(). */
typedef Arr(const char *) Cmd;

/** Append any number of `const char *` arguments to the Cmd at @p c.
 *  @return False when out of memory. The Cmd is then unchanged. */
#define cmd_append(c, ...)                                                     \
    cmd_append_((c), (const char *const[]){ __VA_ARGS__ },                     \
                sizeof((const char *const[]){ __VA_ARGS__ }) /                 \
                    sizeof(const char *))

/** Append @p c to @p out as a shell command line, quoting each argument
 *  that needs it, so it can be pasted into a shell.
 *  @return False when out of memory. @p out is then unchanged. */
MULE_NODISCARD bool cmd_render(Str *out, const Cmd *c);

/** @} */

/** @name Running
 *  Start a program, and wait for it.
 *  @{ */

/** Files to connect to a child's standard streams. A null path leaves the
 *  stream as the parent's. stdout and stderr may name the same file. */
typedef struct CmdIo {
    const char *in;  /**< Read as stdin. */
    const char *out; /**< Created or truncated, and written as stdout. */
    const char *err; /**< Created or truncated, and written as stderr. */
} CmdIo;

/** A running program. Zero means none. */
typedef struct Proc {
    int64_t id_; /**< Internal. */
} Proc;

/** Start @p c without waiting for it.
 *  @param p  Set to the new process.
 *  @param c  The command. It must not be empty.
 *  @param io  Redirections, or null for none.
 *  @param err  The error to fill, or null.
 *  @return False when a redirected file cannot be opened, the program
 *  cannot be started, or @p c is empty (#ERR_INVALID). @p p is then
 *  unchanged. */
MULE_NODISCARD bool proc_start(Proc *p, const Cmd *c, const CmdIo *io,
                               Err *err);
/** Wait for @p p to end, and set it to zero.
 *  @return False when it exited nonzero or was killed by a signal, or @p p
 *  is zero (#ERR_INVALID). */
MULE_NODISCARD bool proc_wait(Proc *p, Err *err);
/** Start @p c and wait for it.
 *  @return False as for proc_start() and proc_wait(). */
MULE_NODISCARD bool cmd_run(const Cmd *c, Err *err);
/** Run @p c and append what it writes to stdout to @p out.
 *  @return False as for cmd_run(), or when the output cannot be read or
 *  stored. @p out is then unchanged. */
MULE_NODISCARD bool cmd_capture(const Cmd *c, Str *out, Err *err);

/** @} */

/** @name Pools
 *  Run several commands at once.
 *  @{ */

/** A job in a pool. */
typedef struct ProcJob {
    Proc proc;        /**< The running process. */
    const char *name; /**< The program, borrowed from the Cmd, for messages. */
} ProcJob;

/** The jobs a pool is running. */
typedef Arr(ProcJob) ProcJobs;

/** Runs commands in parallel, at most `max` at a time. Zero-initialise it,
 *  and set `max` or leave it 0 for the number of CPUs. */
typedef struct ProcPool {
    ProcJobs running; /**< Started and not yet waited for, oldest first. */
    size_t max;       /**< How many may run at once. 0 means the CPU count. */
} ProcPool;

/** Start @p c in @p pool. When the pool is full it first waits for the
 *  oldest job.
 *  @return False when that job failed, which stops the build early, or
 *  when @p c cannot be started. @p c has not started then. Call
 *  proc_pool_wait() to wait for the rest. */
MULE_NODISCARD bool proc_pool_start(ProcPool *pool, const Cmd *c, Err *err);
/** Wait for every job in @p pool.
 *  @return False when any of them failed. The Err describes the first. */
MULE_NODISCARD bool proc_pool_wait(ProcPool *pool, Err *err);
/** Wait for any jobs still running, ignoring how they end, and free the
 *  pool's memory. */
void proc_pool_free(ProcPool *pool);

/** @} */

/** @cond */
MULE_NODISCARD bool cmd_append_(Cmd *c, const char *const *args, size_t n);
/** @endcond */

/** @} */
