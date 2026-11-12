/* args: options, positionals and --help from one table.
 *
 *   cli_options --name=Sonya -n 3 --loud a.txt b.txt
 *   cli_options --help
 */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    const char *name = "world";
    uint64_t times = 1;
    bool loud = false;
    const ArgOpt opts[] = {
        { "name", 0, ARG_STR, &name, "WHO", "who to greet (world)" },
        { "times", 'n', ARG_U64, &times, "N", "greet N times (1)" },
        { "loud", 'l', ARG_FLAG, &loud, NULL, "shout" },
    };
    Args a = { .usage = "cli_options [options] [files...]",
               .opts = opts,
               .opt_count = sizeof opts / sizeof opts[0] };

    Err err = { 0 };
    if (!args_parse(&a, argc, argv, &err)) {
        fprintf(stderr, "%s (see --help)\n", err.msg);
        return 2;
    }
    if (a.help) {
        Str help = { 0 };
        bool ok = args_help(&help, &a);
        fputs(ok ? str_cstr(&help) : "out of memory\n", stdout);
        str_free(&help);
        args_free(&a);
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    for (uint64_t i = 0; i < times; i++)
        printf(loud ? "HELLO, %s!\n" : "hello, %s\n", name);
    for (size_t i = 0; i < a.rest.len; i++) printf("file %s\n", a.rest.data[i]);
    args_free(&a);
    return EXIT_SUCCESS;
}
