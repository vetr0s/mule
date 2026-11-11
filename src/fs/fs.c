#include "fs/fs.h"
#include "arr/arr.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

typedef Arr(size_t) FsOffsets_;
typedef Arr(const char *) FsNames_;

static void fs_oom_(Err *err, const char *op, const char *path) {
    err_set(err, ERR_NO_MEMORY, 0, "%s %s: out of memory", op, path);
}

static FsType fs_type_(mode_t m) {
    if (S_ISREG(m)) return FS_FILE;
    if (S_ISDIR(m)) return FS_DIR;
    if (S_ISLNK(m)) return FS_LINK;
    return FS_OTHER;
}

static int64_t fs_mtime_ns_(const struct stat *s) {
#if defined(__APPLE__) && defined(st_mtime)
    /* A program that defined _DARWIN_C_SOURCE gets the BSD layout. */
    return (int64_t)s->st_mtimespec.tv_sec * 1000000000 +
           s->st_mtimespec.tv_nsec;
#elif defined(__APPLE__)
    return (int64_t)s->st_mtime * 1000000000 + s->st_mtimensec;
#else
    return (int64_t)s->st_mtim.tv_sec * 1000000000 + s->st_mtim.tv_nsec;
#endif
}

static bool fs_stat_(const char *path, FsStat *st, Err *err, bool follow) {
    struct stat s;
    if ((follow ? stat(path, &s) : lstat(path, &s)) != 0) {
        err_os(err, errno, follow ? "stat" : "lstat", sv_from_cstr(path));
        return false;
    }
    *st = (FsStat){ .type = fs_type_(s.st_mode),
                    .size = (uint64_t)s.st_size,
                    .mtime_ns = fs_mtime_ns_(&s),
                    .mode = (uint32_t)(s.st_mode & 07777) };
    return true;
}

bool fs_stat(const char *path, FsStat *st, Err *err) {
    return fs_stat_(path, st, err, true);
}

bool fs_lstat(const char *path, FsStat *st, Err *err) {
    return fs_stat_(path, st, err, false);
}

bool fs_exists(const char *path) {
    struct stat s;
    return stat(path, &s) == 0;
}

bool fs_read(Str *out, const char *path, Err *err) {
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        err_os(err, errno, "open", sv_from_cstr(path));
        return false;
    }
    bool ok = str_append_file(out, f);
    if (!ok && ferror(f))
        err_os(err, errno, "read", sv_from_cstr(path));
    else if (!ok)
        fs_oom_(err, "read", path);
    fclose(f);
    return ok;
}

/* Leaves errno set on failure. */
static bool fs_write_all_(int fd, StrView data) {
    const char *p = data.data;
    size_t left = data.len;
    while (left > 0) {
        ssize_t n = write(fd, p, left);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) return false;
        p += n;
        left -= (size_t)n;
    }
    return true;
}

bool fs_write(const char *path, StrView data, Err *err) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0666);
    if (fd < 0) {
        err_os(err, errno, "open", sv_from_cstr(path));
        return false;
    }
    bool ok = fs_write_all_(fd, data);
    if (!ok) err_os(err, errno, "write", sv_from_cstr(path));
    if (close(fd) != 0 && ok) {
        err_os(err, errno, "close", sv_from_cstr(path));
        ok = false;
    }
    return ok;
}

bool fs_write_atomic(const char *path, StrView data, Err *err) {
    Str tmp = { 0 };
    if (!str_appendf(&tmp, "%s.tmpXXXXXX", path)) {
        fs_oom_(err, "write", path);
        return false;
    }
    struct stat s;
    mode_t mode = stat(path, &s) == 0 ? s.st_mode & 07777 : 0644;
    int fd = mkstemp(tmp.data);
    if (fd < 0) {
        err_os(err, errno, "create", str_view(&tmp));
        str_free(&tmp);
        return false;
    }
    /* fsync first, or a crash after the rename can leave an empty file. */
    bool ok = fchmod(fd, mode) == 0 &&
              fs_write_all_(fd, data) &&
              fsync(fd) == 0;
    if (!ok) err_os(err, errno, "write", str_view(&tmp));
    if (close(fd) != 0 && ok) {
        err_os(err, errno, "close", str_view(&tmp));
        ok = false;
    }
    if (ok && rename(tmp.data, path) != 0) {
        err_os(err, errno, "rename", sv_from_cstr(path));
        ok = false;
    }
    if (!ok) unlink(tmp.data);
    str_free(&tmp);
    return ok;
}

bool fs_mkdir_p(const char *path, Err *err) {
    Str p = { 0 };
    if (!str_from_cstr(&p, path)) {
        fs_oom_(err, "mkdir", path);
        return false;
    }
    while (p.len > 1 && p.data[p.len - 1] == '/') p.data[--p.len] = '\0';

    bool ok = true;
    /* Make each prefix that ends before a slash, then the whole path. */
    for (size_t i = 1; ok && i <= p.len; i++) {
        if (i < p.len && p.data[i] != '/') continue;
        if (p.data[i - 1] == '/') continue;
        char saved = p.data[i];
        p.data[i] = '\0';
        struct stat s;
        if (mkdir(p.data, 0777) != 0) {
            int code = errno;
            if (code != EEXIST) {
                err_os(err, code, "mkdir", sv_from_cstr(p.data));
                ok = false;
            } else if (stat(p.data, &s) != 0 || !S_ISDIR(s.st_mode)) {
                err_os(err, i == p.len ? EEXIST : ENOTDIR, "mkdir",
                       sv_from_cstr(p.data));
                ok = false;
            }
        }
        p.data[i] = saved;
    }
    str_free(&p);
    return ok;
}

bool fs_remove(const char *path, Err *err) {
    if (remove(path) == 0) return true;
    err_os(err, errno, "remove", sv_from_cstr(path));
    return false;
}

static int fs_name_cmp_(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

/* The entries of `dir`, sorted, as pointers into `buf`. */
static bool fs_list_(const char *dir, Str *buf, FsNames_ *names, Err *err) {
    DIR *d = opendir(dir);
    if (d == NULL) {
        err_os(err, errno, "open", sv_from_cstr(dir));
        return false;
    }
    FsOffsets_ offs = { 0 };
    bool ok = true;
    for (;;) {
        errno = 0;
        struct dirent *e = readdir(d);
        if (e == NULL) {
            if (errno != 0) {
                err_os(err, errno, "read", sv_from_cstr(dir));
                ok = false;
            }
            break;
        }
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0)
            continue;
        size_t off = buf->len;
        if (!(arr_push(&offs, off) &&
              str_append_cstr(buf, e->d_name) &&
              str_push(buf, '\0'))) {
            fs_oom_(err, "read", dir);
            ok = false;
            break;
        }
    }
    closedir(d);

    if (ok && !arr_reserve(names, offs.len)) {
        fs_oom_(err, "read", dir);
        ok = false;
    }
    for (size_t i = 0; ok && i < offs.len; i++)
        names->data[names->len++] = buf->data + offs.data[i];
    if (ok) qsort(names->data, names->len, sizeof *names->data, fs_name_cmp_);
    arr_free(&offs);
    return ok;
}

/* Cut `path` back to `len` bytes after a child was joined onto it. */
static void fs_truncate_(Str *path, size_t len) {
    path->len = len;
    path->data[len] = '\0';
}

static bool fs_remove_tree_(Str *path, Err *err) {
    struct stat s;
    if (lstat(path->data, &s) != 0) {
        if (errno == ENOENT) return true;
        err_os(err, errno, "lstat", str_view(path));
        return false;
    }
    if (S_ISDIR(s.st_mode)) {
        Str buf = { 0 };
        FsNames_ names = { 0 };
        bool ok = fs_list_(path->data, &buf, &names, err);
        size_t base = path->len;
        for (size_t i = 0; ok && i < names.len; i++) {
            if (!fs_join(path, sv_from_cstr(names.data[i]))) {
                fs_oom_(err, "remove", path->data);
                ok = false;
                break;
            }
            ok = fs_remove_tree_(path, err);
            fs_truncate_(path, base);
        }
        str_free(&buf);
        arr_free(&names);
        if (!ok) return false;
    }
    if (remove(path->data) != 0) {
        err_os(err, errno, "remove", str_view(path));
        return false;
    }
    return true;
}

bool fs_remove_all(const char *path, Err *err) {
    Str p = { 0 };
    if (!str_from_cstr(&p, path)) {
        fs_oom_(err, "remove", path);
        return false;
    }
    bool ok = fs_remove_tree_(&p, err);
    str_free(&p);
    return ok;
}

bool fs_rename(const char *from, const char *to, Err *err) {
    if (rename(from, to) == 0) return true;
    err_os(err, errno, "rename", sv_from_cstr(from));
    return false;
}

bool fs_copy(const char *from, const char *to, Err *err) {
    int in = open(from, O_RDONLY | O_CLOEXEC);
    if (in < 0) {
        err_os(err, errno, "open", sv_from_cstr(from));
        return false;
    }
    struct stat s;
    if (fstat(in, &s) != 0) {
        err_os(err, errno, "stat", sv_from_cstr(from));
        close(in);
        return false;
    }
    int out = open(to, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (out < 0) {
        err_os(err, errno, "open", sv_from_cstr(to));
        close(in);
        return false;
    }

    char buf[16384];
    bool ok = true;
    for (;;) {
        ssize_t n = read(in, buf, sizeof buf);
        if (n < 0 && errno == EINTR) continue;
        if (n < 0) {
            err_os(err, errno, "read", sv_from_cstr(from));
            ok = false;
        } else if (n > 0 && !fs_write_all_(out, (StrView){ buf, (size_t)n })) {
            err_os(err, errno, "write", sv_from_cstr(to));
            ok = false;
        }
        if (n <= 0 || !ok) break;
    }
    if (ok && fchmod(out, s.st_mode & 07777) != 0) {
        err_os(err, errno, "chmod", sv_from_cstr(to));
        ok = false;
    }
    if (close(out) != 0 && ok) {
        err_os(err, errno, "close", sv_from_cstr(to));
        ok = false;
    }
    close(in);
    return ok;
}

/* Append the target of the link at `path` to `out`, unchanged on failure. */
static bool fs_readlink_(const char *path, Str *out, Err *err) {
    Str tmp = { .alloc = out->alloc };
    for (size_t want = 256;; want *= 2) {
        if (!str_reserve(&tmp, want)) {
            fs_oom_(err, "readlink", path);
            str_free(&tmp);
            return false;
        }
        ssize_t n = readlink(path, tmp.data, tmp.capacity);
        if (n < 0) {
            err_os(err, errno, "readlink", sv_from_cstr(path));
            str_free(&tmp);
            return false;
        }
        if ((size_t)n < tmp.capacity) {
            tmp.len = (size_t)n;
            tmp.data[n] = '\0';
            break;
        }
    }
    bool ok = str_append(out, str_view(&tmp));
    if (!ok) fs_oom_(err, "readlink", path);
    str_free(&tmp);
    return ok;
}

static bool fs_copy_tree_(Str *from, Str *to, Err *err) {
    struct stat s;
    if (lstat(from->data, &s) != 0) {
        err_os(err, errno, "lstat", str_view(from));
        return false;
    }
    if (S_ISREG(s.st_mode)) return fs_copy(from->data, to->data, err);
    if (S_ISLNK(s.st_mode)) {
        Str target = { 0 };
        bool ok = fs_readlink_(from->data, &target, err);
        if (ok && symlink(str_cstr(&target), to->data) != 0) {
            err_os(err, errno, "symlink", str_view(to));
            ok = false;
        }
        str_free(&target);
        return ok;
    }
    if (!S_ISDIR(s.st_mode)) {
        err_set(err, ERR_INVALID, 0, "copy %s: not a file, directory or link",
                from->data);
        return false;
    }

    if (mkdir(to->data, 0700) != 0 && errno != EEXIST) {
        err_os(err, errno, "mkdir", str_view(to));
        return false;
    }
    Str buf = { 0 };
    FsNames_ names = { 0 };
    bool ok = fs_list_(from->data, &buf, &names, err);
    size_t from_len = from->len, to_len = to->len;
    for (size_t i = 0; ok && i < names.len; i++) {
        StrView name = sv_from_cstr(names.data[i]);
        if (!(fs_join(from, name) && fs_join(to, name))) {
            fs_oom_(err, "copy", from->data);
            ok = false;
        } else {
            ok = fs_copy_tree_(from, to, err);
        }
        fs_truncate_(from, from_len);
        fs_truncate_(to, to_len);
    }
    str_free(&buf);
    arr_free(&names);
    /* Set the mode last, so a read-only directory can still be filled. */
    if (ok && chmod(to->data, s.st_mode & 07777) != 0) {
        err_os(err, errno, "chmod", str_view(to));
        ok = false;
    }
    return ok;
}

bool fs_copy_tree(const char *from, const char *to, Err *err) {
    Str f = { 0 }, t = { 0 };
    bool ok = str_from_cstr(&f, from) && str_from_cstr(&t, to);
    if (!ok)
        fs_oom_(err, "copy", from);
    else
        ok = fs_copy_tree_(&f, &t, err);
    str_free(&f);
    str_free(&t);
    return ok;
}

static bool fs_walk_dir_(Str *path, FsWalkFn *fn, void *ctx, Err *err,
                         bool *stop) {
    Str buf = { 0 };
    FsNames_ names = { 0 };
    bool ok = fs_list_(path->data, &buf, &names, err);
    size_t base = path->len;
    for (size_t i = 0; ok && !*stop && i < names.len; i++) {
        struct stat s;
        if (!fs_join(path, sv_from_cstr(names.data[i]))) {
            fs_oom_(err, "walk", path->data);
            ok = false;
        } else if (lstat(path->data, &s) != 0) {
            err_os(err, errno, "lstat", str_view(path));
            ok = false;
        } else {
            FsType type = fs_type_(s.st_mode);
            FsWalkAct act = fn(ctx, path->data, type);
            if (act == FS_WALK_STOP)
                *stop = true;
            else if (act == FS_WALK_NEXT && type == FS_DIR)
                ok = fs_walk_dir_(path, fn, ctx, err, stop);
        }
        fs_truncate_(path, base);
    }
    str_free(&buf);
    arr_free(&names);
    return ok;
}

bool fs_walk(const char *root, FsWalkFn *fn, void *ctx, Err *err) {
    struct stat s;
    if (lstat(root, &s) != 0) {
        err_os(err, errno, "lstat", sv_from_cstr(root));
        return false;
    }
    FsType type = fs_type_(s.st_mode);
    if (fn(ctx, root, type) != FS_WALK_NEXT || type != FS_DIR) return true;

    Str path = { 0 };
    if (!str_from_cstr(&path, root)) {
        fs_oom_(err, "walk", root);
        return false;
    }
    bool stop = false;
    bool ok = fs_walk_dir_(&path, fn, ctx, err, &stop);
    str_free(&path);
    return ok;
}

bool fs_exe_path(Str *out, Err *err) {
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(NULL, &size); /* fails, and sets the size needed */
    Str raw = { 0 };
    if (!str_reserve(&raw, size)) {
        fs_oom_(err, "find", "the executable");
        return false;
    }
    char real[PATH_MAX];
    bool ok = _NSGetExecutablePath(raw.data, &size) == 0 &&
              realpath(raw.data, real) != NULL;
    if (!ok) err_os(err, errno, "find", SV("the executable"));
    str_free(&raw);
    if (ok && !str_append_cstr(out, real)) {
        fs_oom_(err, "find", "the executable");
        ok = false;
    }
    return ok;
#else
    return fs_readlink_("/proc/self/exe", out, err);
#endif
}

static StrView fs_strip_slashes_(StrView path) {
    while (path.len > 1 && path.data[path.len - 1] == '/') path.len--;
    return path;
}

StrView fs_basename(StrView path) {
    path = fs_strip_slashes_(path);
    size_t at = sv_find_last(path, SV("/"));
    if (at == SV_NPOS || path.len == 1) return path;
    return sv_slice(path, at + 1, path.len);
}

StrView fs_dirname(StrView path) {
    path = fs_strip_slashes_(path);
    size_t at = sv_find_last(path, SV("/"));
    if (at == SV_NPOS) return SV(".");
    if (at == 0) return SV("/");
    return fs_strip_slashes_(sv_slice(path, 0, at));
}

StrView fs_extension(StrView path) {
    StrView base = fs_basename(path);
    size_t at = sv_find_last(base, SV("."));
    if (at == SV_NPOS || at == 0 || str_eq(base, SV("..")))
        return sv_slice(base, base.len, base.len);
    return sv_slice(base, at, base.len);
}

bool fs_join(Str *path, StrView part) {
    if (path->len > 0)
        while (part.len > 0 && part.data[0] == '/')
            part = sv_slice(part, 1, part.len);
    if (part.len == 0) return true;
    bool slash = path->len > 0 && path->data[path->len - 1] != '/';
    if (!str_reserve(path, part.len + slash)) return false;
    return (!slash || str_push(path, '/')) && str_append(path, part);
}
