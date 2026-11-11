#pragma once
/**
 * @defgroup mod_err Err
 * @brief Why a call failed, for functions that can fail in more than one way.
 *
 * Most functions in MULE fail only when memory runs out, so their `bool`
 * says everything. A function that opens a file, runs a program or parses
 * input can fail for several reasons, and the caller needs to know which.
 * Such a function takes an `Err *` as its last argument.
 *
 *   - It still returns `bool`, so calls chain with `&&` as they do
 *     everywhere else.
 *   - On false it fills the Err. The Err is the one argument a failed call
 *     changes.
 *   - On true it leaves the Err alone, so a chain keeps its first failure.
 *   - A null `Err *` means the caller wants only the `bool`.
 *   - A zero-initialised Err (`Err err = { 0 };`) holds #ERR_NONE and an
 *     empty message.
 *   - The message lives inside the Err. Nothing is allocated, so there is
 *     nothing to free, and running out of memory can still be reported.
 *
 * An Err has three parts. `kind` is portable and is what code branches on.
 * `code` is the raw number behind it, and `msg` is a sentence for a person.
 *
 * A caller's own function can report in the same style:
 *
 * @code
 * static bool load_config(Str *out, const char *path, Err *err) {
 *     FILE *f = fopen(path, "rb");
 *     if (f == NULL) {
 *         err_os(err, errno, "open", sv_from_cstr(path));
 *         return false;
 *     }
 *     bool ok = str_append_file(out, f);
 *     if (!ok && ferror(f))
 *         err_os(err, errno, "read", sv_from_cstr(path));
 *     else if (!ok)
 *         err_set(err, ERR_NO_MEMORY, 0, "read %s: out of memory", path);
 *     fclose(f);
 *     return ok;
 * }
 *
 * Err err = { 0 };
 * if (!load_config(&cfg, "mule.ini", &err) && err.kind != ERR_NOT_FOUND) {
 *     log_error("%s", err.msg); // "open mule.ini: Permission denied"
 *     return 1;
 * }
 * @endcode
 *
 * Read `errno` straight after the call that set it, and pass it in. A later
 * call such as `fclose` may overwrite it.
 * @{
 */

#include "base/base.h"
#include "str/str.h"

/** @name Errors
 *  The error value and the functions that fill it.
 *  @{ */

/** Bytes in Err::msg, including the terminator. A longer message is cut. */
#define ERR_MSG_MAX 256

/** What went wrong, in terms that are the same on every platform. */
typedef enum ErrKind {
    ERR_NONE = 0,   /**< No error. A zeroed Err holds this. */
    ERR_NO_MEMORY,  /**< An allocation failed or a size overflowed. */
    ERR_NOT_FOUND,  /**< A file, directory or program does not exist. */
    ERR_EXISTS,     /**< Something is already where a new one should go. */
    ERR_PERMISSION, /**< The process may not do this. */
    ERR_NOT_DIR,    /**< A directory was needed and something else is there. */
    ERR_IS_DIR,     /**< A directory is where something else was needed. */
    ERR_INVALID,    /**< The input was malformed, such as an unknown flag. */
    ERR_EXIT,   /**< A child process exited nonzero. `code` is its status. */
    ERR_SIGNAL, /**< A signal killed a child. `code` is the signal. */
    ERR_OS,     /**< Any other system failure. `code` is the raw value. */
} ErrKind;

/** Why a call failed. Zero-initialise it, pass its address, and read it
 *  after a call returns false. */
typedef struct Err {
    ErrKind kind;          /**< What went wrong. Branch on this. */
    int code;              /**< `errno`, an exit status or a signal number. */
    char msg[ERR_MSG_MAX]; /**< A sentence for a person. Always terminated. */
} Err;

/** Fill @p e with @p kind, @p code and a printf-formatted message. Does
 *  nothing when @p e is null.
 *  @param e  The error to fill, or null.
 *  @param kind  What went wrong.
 *  @param code  The raw number behind it, or 0.
 *  @param fmt  A printf format for the message, and its arguments after it. */
void err_set(Err *e, ErrKind kind, int code, const char *fmt, ...)
    MULE_PRINTF(4, 5);

/** Fill @p e from an OS error code. It maps @p code to an ErrKind and writes
 *  `op subject: reason`, where the reason is the system's text for @p code.
 *  Does nothing when @p e is null.
 *  @param e  The error to fill, or null.
 *  @param code  `errno` on Unix, read straight after the failing call.
 *  @param op  What was attempted, such as "open" or "mkdir".
 *  @param subject  What it was attempted on, usually a path. */
void err_os(Err *e, int code, const char *op, StrView subject);

/** @} */

/** @} */
