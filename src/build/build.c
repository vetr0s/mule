#include "build/build.h"
#include "fs/fs.h"
#include "log/log.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

bool build_needs_rebuild(const char *out, const char *const *inputs, size_t n,
                         bool *needed, Err *err) {
    int64_t newest = INT64_MIN;
    for (size_t i = 0; i < n; i++) {
        FsStat st = { 0 };
        if (!fs_stat(inputs[i], &st, err)) return false;
        if (st.mtime_ns > newest) newest = st.mtime_ns;
    }
    FsStat o = { 0 };
    Err e = { 0 };
    if (!fs_stat(out, &o, &e) && e.kind != ERR_NOT_FOUND) {
        if (err != NULL) *err = e;
        return false;
    }
    *needed = o.type == FS_MISSING || newest > o.mtime_ns;
    return true;
}

/* Rewrite `d->text` in place into NUL-terminated names, and point `d->paths`
 * at each. The writer never passes the reader, so one buffer serves both. */
static bool build_parse_deps_(Deps *d) {
    char *r = d->text.data, *end = r + d->text.len, *w = r;
    /* Skip the target: everything up to a colon followed by a space or an
     * end of line. */
    while (r < end &&
           !(r[0] == ':' &&
             (r + 1 == end || r[1] == ' ' || r[1] == '\t' || r[1] == '\n')))
        r += r[0] == '\\' && r + 1 < end ? 2 : 1;
    if (r < end) r++;

    for (;;) {
        while (r < end && (*r == ' ' ||
                           *r == '\t' ||
                           (r[0] == '\\' && r + 1 < end && r[1] == '\n')))
            r += *r == '\\' ? 2 : 1;
        if (r == end || *r == '\n' || *r == '\r') break;

        char *name = w;
        while (r < end && *r != ' ' && *r != '\t' && *r != '\n' && *r != '\r') {
            if (r[0] == '\\' && r + 1 < end && r[1] == '\n') break;
            if (r[0] == '\\' && r + 1 < end && (r[1] == ' ' || r[1] == '#'))
                r++;
            else if (r[0] == '$' && r + 1 < end && r[1] == '$')
                r++;
            *w++ = *r++;
        }
        *w++ = '\0';
        if (!arr_push(&d->paths, name)) return false;
    }
    return true;
}

bool build_read_deps(Deps *d, const char *depfile, Err *err) {
    Deps next = { .text = { .alloc = d->text.alloc },
                  .paths = { .alloc = d->paths.alloc } };
    if (!fs_read(&next.text, depfile, err)) return false;
    /* An empty file still needs a buffer to point into. */
    if (!str_reserve(&next.text, 1) || !build_parse_deps_(&next)) {
        err_set(err, ERR_NO_MEMORY, 0, "read %s: out of memory", depfile);
        build_deps_free(&next);
        return false;
    }
    build_deps_free(d);
    *d = next;
    return true;
}

void build_deps_free(Deps *d) {
    str_free(&d->text);
    arr_free(&d->paths);
}

bool build_needs_rebuild_deps(const char *out, const char *depfile,
                              bool *needed, Err *err) {
    FsStat o = { 0 };
    Err e = { 0 };
    if (!fs_stat(out, &o, &e)) {
        if (e.kind != ERR_NOT_FOUND) {
            if (err != NULL) *err = e;
            return false;
        }
        *needed = true;
        return true;
    }
    Deps d = { 0 };
    if (!build_read_deps(&d, depfile, &e)) {
        if (e.kind != ERR_NOT_FOUND) {
            if (err != NULL) *err = e;
            return false;
        }
        *needed = true;
        return true;
    }
    bool stale = false, ok = true;
    for (size_t i = 0; ok && !stale && i < d.paths.len; i++) {
        FsStat st = { 0 };
        e = (Err){ 0 };
        if (fs_stat(d.paths.data[i], &st, &e))
            stale = st.mtime_ns > o.mtime_ns;
        else if (e.kind == ERR_NOT_FOUND)
            stale = true;
        else
            ok = false;
    }
    build_deps_free(&d);
    if (!ok) {
        if (err != NULL) *err = e;
        return false;
    }
    *needed = stale;
    return true;
}

bool build_prepare(const char *out, const char *depfile, const Cmd *c,
                   bool *needed, Err *err) {
    Str stamp = { 0 }, line = { 0 }, old = { 0 };
    if (!str_append_cstr(&stamp, out) ||
        !str_append_cstr(&stamp, ".cmd") ||
        !cmd_render(&line, c) ||
        !str_push(&line, '\n')) {
        err_set(err, ERR_NO_MEMORY, 0, "prepare %s: out of memory", out);
        str_free(&stamp);
        str_free(&line);
        return false;
    }
    Err e = { 0 };
    bool same = fs_read(&old, str_cstr(&stamp), &e) &&
                str_eq(str_view(&old), str_view(&line));
    bool ok = e.kind == ERR_NONE || e.kind == ERR_NOT_FOUND;
    if (!ok && err != NULL) *err = e;

    bool stale = !same;
    if (ok && same) ok = build_needs_rebuild_deps(out, depfile, &stale, err);
    if (ok && stale) {
        e = (Err){ 0 };
        if (!fs_remove(out, &e) && e.kind != ERR_NOT_FOUND) {
            if (err != NULL) *err = e;
            ok = false;
        } else {
            ok = fs_write_atomic(str_cstr(&stamp), str_view(&line), err);
        }
    }
    if (ok) *needed = stale;
    str_free(&stamp);
    str_free(&line);
    str_free(&old);
    return ok;
}

bool build_run(const Cmd *c, Err *err) {
    Str line = { 0 };
    if (cmd_render(&line, c)) log_info("%s", str_cstr(&line));
    str_free(&line);
    return cmd_run(c, err);
}

bool build_start(ProcPool *pool, const Cmd *c, Err *err) {
    Str line = { 0 };
    if (cmd_render(&line, c)) log_info("%s", str_cstr(&line));
    str_free(&line);
    return proc_pool_start(pool, c, err);
}

void build_rebuild_self(int argc, char **argv, const char *source,
                        const Cmd *flags) {
    (void)argc;
    /* Run from another directory, the source cannot be found to rebuild. */
    if (!fs_exists(source)) return;

    Str exe = { 0 }, dir = { 0 }, dep = { 0 }, old = { 0 };
    Cmd cc = { 0 };
    Err err = { 0 };
    bool needed = false;
    if (!fs_exe_path(&exe, &err)) log_fatal("%s", err.msg);
    StrView name = fs_basename(str_view(&exe));
    bool ok = fs_join(&dir, fs_dirname(str_view(&exe))) &&
              fs_join(&dir, SV(".build")) &&
              fs_join(&dep, str_view(&dir)) &&
              fs_join(&dep, name) &&
              str_append_cstr(&dep, ".d") &&
              fs_join(&old, str_view(&dir)) &&
              fs_join(&old, name) &&
              str_append_cstr(&old, ".old");
    if (!ok) log_fatal("rebuild %s: out of memory", source);
    if (!build_needs_rebuild_deps(str_cstr(&exe), str_cstr(&dep), &needed,
                                  &err))
        log_fatal("%s", err.msg);

    if (needed) {
        const char *compiler = getenv("CC");
        if (compiler == NULL || compiler[0] == '\0') compiler = "cc";
        ok = cmd_append(&cc, compiler, "-Wall", "-Wextra", "-MMD", "-MF",
                        str_cstr(&dep), "-o", str_cstr(&exe));
        for (size_t i = 0; ok && flags != NULL && i < flags->len; i++)
            ok = cmd_append(&cc, flags->data[i]);
        ok = ok && cmd_append(&cc, source);
        if (!ok) log_fatal("rebuild %s: out of memory", source);

        if (!fs_mkdir_p(str_cstr(&dir), &err) ||
            !fs_rename(str_cstr(&exe), str_cstr(&old), &err))
            log_fatal("%s", err.msg);
        if (!build_run(&cc, &err)) {
            log_error("%s", err.msg);
            Err back = { 0 };
            if (!fs_rename(str_cstr(&old), str_cstr(&exe), &back))
                log_error("%s", back.msg);
            exit(EXIT_FAILURE);
        }
        execv(str_cstr(&exe), argv);
        log_fatal("run %s: %s", str_cstr(&exe), strerror(errno));
    }
    str_free(&exe);
    str_free(&dir);
    str_free(&dep);
    str_free(&old);
    arr_free(&cc);
}

/* Append `s` to `out` as a JSON string, quotes included. */
static bool build_json_str_(Str *out, const char *s) {
    bool ok = str_push(out, '"');
    for (; ok && *s != '\0'; s++) {
        unsigned char ch = (unsigned char)*s;
        if (ch == '"' || ch == '\\')
            ok = str_push(out, '\\') && str_push(out, (char)ch);
        else if (ch < 0x20)
            ok = str_appendf(out, "\\u%04x", ch);
        else
            ok = str_push(out, (char)ch);
    }
    return ok && str_push(out, '"');
}

bool build_compdb_add(CompDb *db, const char *file, const Cmd *c) {
    size_t len = db->json.len;
    bool ok = str_append_cstr(&db->json, "\"file\": ") &&
              build_json_str_(&db->json, file) &&
              str_append_cstr(&db->json, ", \"arguments\": [");
    for (size_t i = 0; ok && i < c->len; i++)
        ok = (i == 0 || str_append_cstr(&db->json, ", ")) &&
             build_json_str_(&db->json, c->data[i]);
    ok = ok && str_append_cstr(&db->json, "]\n");
    if (!ok && db->json.data != NULL) {
        db->json.len = len;
        db->json.data[len] = '\0';
    }
    return ok;
}

bool build_compdb_write(const CompDb *db, const char *path, Err *err) {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof cwd) == NULL) {
        err_os(err, errno, "getcwd", SV(""));
        return false;
    }
    Str out = { 0 }, dir = { 0 }, old = { 0 };
    bool ok = build_json_str_(&dir, cwd) && str_push(&out, '[');
    StrView rest = str_view(&db->json), entry;
    bool first = true;
    while (ok && sv_split_next(&rest, SV("\n"), &entry)) {
        ok = str_append_cstr(&out, first ? "\n" : ",\n") &&
             str_append_cstr(&out, "  {\"directory\": ") &&
             str_append(&out, str_view(&dir)) &&
             str_append_cstr(&out, ", ") &&
             str_append(&out, entry) &&
             str_push(&out, '}');
        first = false;
    }
    ok = ok && str_append_cstr(&out, "\n]\n");
    if (!ok) err_set(err, ERR_NO_MEMORY, 0, "write %s: out of memory", path);

    Err e = { 0 };
    bool same = ok &&
                fs_read(&old, path, &e) &&
                str_eq(str_view(&old), str_view(&out));
    if (ok && !same) ok = fs_write_atomic(path, str_view(&out), err);
    str_free(&out);
    str_free(&dir);
    str_free(&old);
    return ok;
}

void build_compdb_free(CompDb *db) {
    str_free(&db->json);
}
