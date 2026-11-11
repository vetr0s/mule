#pragma once
/**
 * @defgroup mod_args Args
 * @brief Command-line options, positional arguments and subcommands.
 *
 * Describe the options in a table of ArgOpt, each pointing at the variable
 * it sets, and pass the table to args_parse() in an Args. The same table
 * writes the `--help` text with args_help().
 *
 * @snippet{trimleft} test_args.c args
 *
 * args_parse() understands:
 *   - `--name=value` and `--name value`, `-n value` and `-nvalue`.
 *   - `--flag` and `-f`, and flags grouped as `-vq`.
 *   - `--`, after which every argument is positional.
 *   - `-h` and `--help`, which set `help` unless an option claims them.
 *   - With subcommands in `cmds`, the first positional must name one, and
 *     lands in `cmd`. The other positionals land in `rest`.
 *
 * It checks every argument before setting anything, so a failed parse
 * leaves every variable as it was. String values point into `argv`, so
 * they live as long as `argv` does.
 * @{
 */

#include "arr/arr.h"
#include "base/base.h"
#include "err/err.h"
#include "str/str.h"

/** @name Describing a command line
 *  The tables args_parse() reads.
 *  @{ */

/** What an option's `out` points at. */
typedef enum ArgKind {
    ARG_FLAG, /**< A `bool`, set to true when the option appears. */
    ARG_STR,  /**< A `const char *`, set to the value. */
    ARG_U64,  /**< A `uint64_t`, set to the value read in base 10. */
} ArgKind;

/** One option. */
typedef struct ArgOpt {
    const char *name;       /**< The long name without `--`, or null. */
    char short_name;        /**< The letter after `-`, or 0 for none. */
    ArgKind kind;           /**< What `out` points at. */
    void *out;              /**< The variable the option sets. */
    const char *value_name; /**< Shown in the help, such as "N". */
    const char *help;       /**< One line for the help. */
} ArgOpt;

/** One subcommand. */
typedef struct ArgCmd {
    const char *name; /**< What the user types. */
    const char *help; /**< One line for the help. */
} ArgCmd;

/** Positional arguments, borrowed from `argv`. */
typedef Arr(const char *) ArgList;

/** A command line's description, and what parsing it found. A zeroed Args
 *  takes no options, and every argument is positional. */
typedef struct Args {
    const char *usage;  /**< The usage line for the help, after "usage: ". */
    const ArgOpt *opts; /**< The options. */
    size_t opt_count;   /**< Entries in `opts`. */
    const ArgCmd *cmds; /**< The subcommands, or null for none. */
    size_t cmd_count;   /**< Entries in `cmds`. */
    const char *cmd;    /**< Set to the subcommand given, or left null. */
    bool help;          /**< Set when `-h` or `--help` appears. */
    ArgList rest;       /**< Positional arguments are appended here. */
} Args;

/** @} */

/** @name Parsing
 *  Read a command line, and describe it.
 *  @{ */

/** Parse `argv[1]` to `argv[argc - 1]` against @p a.
 *  @return False with #ERR_INVALID for an unknown option or subcommand, a
 *  missing value, a value given to a flag, or a number that does not
 *  parse, and with #ERR_NO_MEMORY when `rest` cannot grow. The message
 *  names the argument. Every variable and @p a are then unchanged. */
MULE_NODISCARD bool args_parse(Args *a, int argc, char **argv, Err *err);
/** Append the help text for @p a to @p out: the usage line, the
 *  subcommands and the options, each with its help line.
 *  @return False when memory runs out. @p out is then unchanged. */
MULE_NODISCARD bool args_help(Str *out, const Args *a);
/** Free `rest`. Safe to call twice. */
void args_free(Args *a);

/** @} */

/** @} */
