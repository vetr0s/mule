#include "args/args.h"

#include <string.h>

static const ArgOpt *args_find_long_(const Args *a, StrView name) {
    for (size_t i = 0; i < a->opt_count; i++) {
        const char *n = a->opts[i].name;
        if (n != NULL && str_eq(sv_from_cstr(n), name)) return &a->opts[i];
    }
    return NULL;
}

static const ArgOpt *args_find_short_(const Args *a, char c) {
    for (size_t i = 0; i < a->opt_count; i++) {
        if (a->opts[i].short_name == c) return &a->opts[i];
    }
    return NULL;
}

/* Check `value` for `o`, and store it when `apply` is set. */
static bool args_set_(const ArgOpt *o, const char *value, bool apply,
                      Err *err) {
    uint64_t n = 0;
    if (o->kind == ARG_U64 && !sv_parse_u64(sv_from_cstr(value), 10, &n)) {
        err_set(err, ERR_INVALID, 0, "%s%s: expected a number, got '%s'",
                o->name != NULL ? "--" : "-",
                o->name != NULL ? o->name : (char[]){ o->short_name, '\0' },
                value);
        return false;
    }
    if (!apply) return true;
    switch (o->kind) {
    case ARG_FLAG:
        *(bool *)o->out = true;
        break;
    case ARG_STR:
        *(const char **)o->out = value;
        break;
    case ARG_U64:
        *(uint64_t *)o->out = n;
        break;
    }
    return true;
}

/* One pass over argv. The first only checks and counts positionals, the
 * second stores, so a failed parse changes nothing. */
static bool args_pass_(Args *a, int argc, char **argv, bool apply,
                       size_t *positionals, Err *err) {
    bool options = true, have_cmd = a->cmd != NULL;
    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        if (options && strcmp(arg, "--") == 0) {
            options = false;
        } else if (options && arg[0] == '-' && arg[1] == '-') {
            StrView name = sv_from_cstr(arg + 2);
            size_t eq = sv_find_byte(name, '=');
            if (eq != SV_NPOS) name = sv_slice(name, 0, eq);
            const ArgOpt *o = args_find_long_(a, name);
            if (o == NULL && str_eq(name, SV("help")) && eq == SV_NPOS) {
                if (apply) a->help = true;
                continue;
            }
            if (o == NULL) {
                err_set(err, ERR_INVALID, 0, "unknown option '%s'", arg);
                return false;
            }
            const char *value = eq != SV_NPOS ? arg + 2 + eq + 1 : NULL;
            if (o->kind == ARG_FLAG && value != NULL) {
                err_set(err, ERR_INVALID, 0, "--%s takes no value", o->name);
                return false;
            }
            if (o->kind != ARG_FLAG && value == NULL) {
                if (i + 1 == argc) {
                    err_set(err, ERR_INVALID, 0, "--%s needs a value", o->name);
                    return false;
                }
                value = argv[++i];
            }
            if (!args_set_(o, value, apply, err)) return false;
        } else if (options && arg[0] == '-' && arg[1] != '\0') {
            /* A group of short options, the last of which may take a value. */
            for (const char *c = arg + 1; *c != '\0'; c++) {
                const ArgOpt *o = args_find_short_(a, *c);
                if (o == NULL && *c == 'h') {
                    if (apply) a->help = true;
                    continue;
                }
                if (o == NULL) {
                    err_set(err, ERR_INVALID, 0, "unknown option '-%c' in '%s'",
                            *c, arg);
                    return false;
                }
                if (o->kind == ARG_FLAG) {
                    if (!args_set_(o, NULL, apply, err)) return false;
                    continue;
                }
                const char *value = c[1] != '\0' ? c + 1 : NULL;
                if (value == NULL && i + 1 == argc) {
                    err_set(err, ERR_INVALID, 0, "-%c needs a value", *c);
                    return false;
                }
                if (value == NULL) value = argv[++i];
                if (!args_set_(o, value, apply, err)) return false;
                break;
            }
        } else if (a->cmd_count > 0 && !have_cmd) {
            size_t k = 0;
            while (k < a->cmd_count && strcmp(a->cmds[k].name, arg) != 0) k++;
            if (k == a->cmd_count) {
                err_set(err, ERR_INVALID, 0, "unknown command '%s'", arg);
                return false;
            }
            have_cmd = true;
            if (apply) a->cmd = a->cmds[k].name;
        } else if (apply) {
            a->rest.data[a->rest.len++] = arg;
        } else {
            (*positionals)++;
        }
    }
    return true;
}

bool args_parse(Args *a, int argc, char **argv, Err *err) {
    size_t positionals = 0;
    if (!args_pass_(a, argc, argv, false, &positionals, err)) return false;
    if (!arr_reserve(&a->rest, positionals)) {
        err_set(err, ERR_NO_MEMORY, 0, "parse arguments: out of memory");
        return false;
    }
    return args_pass_(a, argc, argv, true, NULL, err);
}

/* The left column of an option's help line, such as "-j, --jobs=N". */
static bool args_opt_left_(Str *s, const ArgOpt *o) {
    const char *value = o->value_name != NULL ? o->value_name : "VALUE";
    bool ok = o->short_name != 0 ? str_appendf(s, "-%c", o->short_name)
                                 : str_append_cstr(s, "  ");
    if (ok && o->name != NULL)
        ok = str_appendf(s, "%s--%s", o->short_name != 0 ? ", " : "  ",
                         o->name);
    if (ok && o->kind != ARG_FLAG)
        ok = o->name != NULL ? str_appendf(s, "=%s", value)
                             : str_appendf(s, " %s", value);
    return ok;
}

bool args_help(Str *out, const Args *a) {
    Str s = { .alloc = out->alloc }, left = { .alloc = out->alloc };
    bool ok = str_appendf(&s, "usage: %s\n", a->usage != NULL ? a->usage : "");

    /* One column width for commands and options alike. */
    size_t width = sizeof "-h, --help" - 1;
    for (size_t i = 0; ok && i < a->cmd_count; i++) {
        size_t n = strlen(a->cmds[i].name);
        if (n > width) width = n;
    }
    for (size_t i = 0; ok && i < a->opt_count; i++) {
        str_clear(&left);
        ok = args_opt_left_(&left, &a->opts[i]);
        if (left.len > width) width = left.len;
    }

    if (ok && a->cmd_count > 0) ok = str_append_cstr(&s, "\ncommands:\n");
    for (size_t i = 0; ok && i < a->cmd_count; i++) {
        const ArgCmd *c = &a->cmds[i];
        ok = str_appendf(&s, "  %-*s  %s\n", (int)width, c->name,
                         c->help != NULL ? c->help : "");
    }
    if (ok) ok = str_append_cstr(&s, "\noptions:\n");
    for (size_t i = 0; ok && i < a->opt_count; i++) {
        str_clear(&left);
        ok = args_opt_left_(&left, &a->opts[i]) &&
             str_appendf(&s, "  %-*s  %s\n", (int)width, str_cstr(&left),
                         a->opts[i].help != NULL ? a->opts[i].help : "");
    }
    bool builtin_help = args_find_long_(a, SV("help")) == NULL &&
                        args_find_short_(a, 'h') == NULL;
    if (ok && builtin_help)
        ok = str_appendf(&s, "  %-*s  %s\n", (int)width, "-h, --help",
                         "show this help");
    ok = ok && str_append(out, str_view(&s));
    str_free(&s);
    str_free(&left);
    return ok;
}

void args_free(Args *a) {
    arr_free(&a->rest);
}
