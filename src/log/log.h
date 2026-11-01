#pragma once
/**
 * @file
 * @brief Log: output for people reading a terminal.
 *
 * This holds terminal colour detection for now. Levels and log calls come
 * next and will use it.
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

/** True when colour escapes written to @p f will render as colour. Reads the
 *  environment on every call, so call it once and keep the answer. */
[[nodiscard]] bool log_color_enabled(FILE *f);
