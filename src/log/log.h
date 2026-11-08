#pragma once
/**
 * @file
 * @brief Log: levelled messages for people reading a terminal or a log file.
 *
 *   - Each call writes one line, `LEVEL file:line: message`, with the level
 *     coloured when the output supports it.
 *   - Calls below the runtime level, set with log_set_level(), return before
 *     evaluating their arguments. Calls below #LOG_LEVEL_MIN compile to
 *     nothing.
 *   - Each line goes out in one `fwrite`, so lines from different threads do
 *     not interleave. Set the level and output before starting threads.
 *   - log_fatal() and log_assert() write, flush and abort, whatever the level.
 *
 * @snippet{trimleft} test_log.c log
 *
 * log_color_enabled() follows the conventions other command-line tools use:
 *   - `NO_COLOR` set to anything non-empty turns colour off. It wins over
 *     everything else (https://no-color.org).
 *   - `CLICOLOR_FORCE` or `FORCE_COLOR` set to anything but empty or `0` turns
 *     colour on, even when the output is a pipe or a file.
 *   - Otherwise colour is on when the stream is a terminal and `TERM` is set
 *     and is not `dumb`.
 *
 * @snippet{trimleft} test_log.c color
 */

#include "base/base.h"

#include <stdio.h>

/* ANSI escape sequences. Macros, so they join string literals:
 * `LOG_GREEN "ok" LOG_RESET`. Print them only when log_color_enabled(). */
#define LOG_RED "\x1b[31m"    /**< Red text. */
#define LOG_GREEN "\x1b[32m"  /**< Green text. */
#define LOG_YELLOW "\x1b[33m" /**< Yellow text. */
#define LOG_RESET "\x1b[0m"   /**< Back to the terminal's default style. */

/** How much a message matters, from chattiest to most severe. */
typedef enum LogLevel {
    LOG_TRACE, /**< Step-by-step detail, usually off. */
    LOG_DEBUG, /**< Detail for whoever is debugging. */
    LOG_INFO,  /**< Normal progress worth recording. The default level. */
    LOG_WARN,  /**< Something odd that the program survived. */
    LOG_ERROR, /**< An operation failed. */
    LOG_FATAL, /**< The program cannot continue. */
} LogLevel;

#ifndef LOG_LEVEL_MIN
/** Calls below this level compile to nothing. Define it before the first
 *  include of log.h, for example `-DLOG_LEVEL_MIN=LOG_INFO`. */
#define LOG_LEVEL_MIN LOG_TRACE
#endif

/** Drop messages below @p level from now on. */
void log_set_level(LogLevel level);
/** Write messages to @p f from now on. Null means stderr, the default. */
void log_set_output(FILE *f);
/** True when colour escapes written to @p f will render as colour. Reads the
 *  environment on every call, so call it once and keep the answer. */
MULE_NODISCARD bool log_color_enabled(FILE *f);

/** Log a printf-style message at LOG_TRACE. */
#define log_trace(...) LOG_AT_(LOG_TRACE, __VA_ARGS__)
/** Log a printf-style message at LOG_DEBUG. */
#define log_debug(...) LOG_AT_(LOG_DEBUG, __VA_ARGS__)
/** Log a printf-style message at LOG_INFO. */
#define log_info(...) LOG_AT_(LOG_INFO, __VA_ARGS__)
/** Log a printf-style message at LOG_WARN. */
#define log_warn(...) LOG_AT_(LOG_WARN, __VA_ARGS__)
/** Log a printf-style message at LOG_ERROR. */
#define log_error(...) LOG_AT_(LOG_ERROR, __VA_ARGS__)
/** Log a printf-style message at LOG_FATAL, flush, and abort. */
#define log_fatal(...) log_fatal_(__FILE__, __LINE__, __VA_ARGS__)
/** Abort through log_fatal() when @p cond is false. Unlike assert(), it stays
 *  in release builds. */
#define log_assert(cond)                                                       \
    ((cond) ? (void)0                                                          \
            : log_fatal_(__FILE__, __LINE__, "assertion failed: %s", #cond))

/** @cond */
extern LogLevel log_level_;

#define LOG_AT_(level, ...)                                                    \
    ((level) >= LOG_LEVEL_MIN && (level) >= log_level_                         \
         ? log_write_((level), __FILE__, __LINE__, __VA_ARGS__)                \
         : (void)0)

void log_write_(LogLevel level, const char *file, int line, const char *fmt,
                ...) MULE_PRINTF(4, 5);
MULE_NORETURN void log_fatal_(const char *file, int line, const char *fmt, ...)
    MULE_PRINTF(3, 4);
/** @endcond */
