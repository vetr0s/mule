#include "proc/proc.h"
#include "mem/mem.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __APPLE__
/* <sys/sysctl.h> does not compile under _POSIX_C_SOURCE on macOS. */
int sysctlbyname(const char *name, void *oldp, size_t *oldlenp, void *newp,
                 size_t newlen);
#endif

bool cmd_append_(Cmd *c, const char *const *args, size_t n) {
    if (!arr_reserve(c, n)) return false;
    memcpy(c->data + c->len, args, n * sizeof *args);
    c->len += n;
    return true;
}

/* True when the shell reads `a` as itself without quotes. */
static bool proc_plain_(const char *a) {
    if (*a == '\0') return false;
    for (; *a != '\0'; a++) {
        char ch = *a;
        bool alnum = (ch >= 'a' && ch <= 'z') ||
                     (ch >= 'A' && ch <= 'Z') ||
                     (ch >= '0' && ch <= '9');
        if (!alnum && strchr("@%+=:,./_-", ch) == NULL) return false;
    }
    return true;
}

bool cmd_render(Str *out, const Cmd *c) {
    Str s = { .alloc = out->alloc };
    bool ok = true;
    for (size_t i = 0; ok && i < c->len; i++) {
        const char *a = c->data[i];
        if (i > 0) ok = str_push(&s, ' ');
        if (ok && proc_plain_(a)) {
            ok = str_append_cstr(&s, a);
            continue;
        }
        /* Single quotes keep everything but a quote, which closes the
         * string, adds an escaped quote and reopens it. */
        ok = ok && str_push(&s, '\'');
        for (; ok && *a != '\0'; a++)
            ok = *a == '\'' ? str_append_cstr(&s, "'\\''") : str_push(&s, *a);
        ok = ok && str_push(&s, '\'');
    }
    ok = ok && str_append(out, str_view(&s));
    str_free(&s);
    return ok;
}

static bool proc_open_(int *fd, const char *path, int flags, Err *err) {
    *fd = open(path, flags | O_CLOEXEC, 0666);
    if (*fd >= 0) return true;
    err_os(err, errno, "open", sv_from_cstr(path));
    return false;
}

static void proc_close_(int fd) {
    if (fd >= 0) close(fd);
}

/* proc_start, with stdout on `out_fd` instead when it is not -1. */
static bool proc_spawn_(Proc *p, const Cmd *c, const CmdIo *io, int out_fd,
                        Err *err) {
    if (c->len == 0) {
        err_set(err, ERR_INVALID, 0, "run: empty command");
        return false;
    }
    const CmdIo none = { 0 };
    if (io == NULL) io = &none;
    const int wr = O_WRONLY | O_CREAT | O_TRUNC;
    int in = -1, out = -1, errfd = -1;
    bool ok = (io->in == NULL || proc_open_(&in, io->in, O_RDONLY, err)) &&
              (out_fd >= 0 ||
               io->out == NULL ||
               proc_open_(&out, io->out, wr, err));
    if (ok && io->err != NULL) {
        /* Two opens of one file would each write from offset 0. */
        if (out >= 0 && strcmp(io->err, io->out) == 0)
            errfd = out;
        else
            ok = proc_open_(&errfd, io->err, wr, err);
    }
    int child_out = out_fd >= 0 ? out_fd : out;

    size_t argv_size = (c->len + 1) * sizeof(const char *);
    const char **argv = NULL;
    if (ok && (argv = mem_realloc(NULL, NULL, 0, argv_size)) == NULL) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        ok = false;
    }
    /* The child writes errno here if exec fails. A successful exec closes
     * it, so the parent reads nothing. */
    int status_pipe[2] = { -1, -1 };
    if (ok && pipe(status_pipe) != 0) {
        err_os(err, errno, "pipe", SV(""));
        ok = false;
    }
    pid_t pid = -1;
    if (ok) {
        fcntl(status_pipe[0], F_SETFD, FD_CLOEXEC);
        fcntl(status_pipe[1], F_SETFD, FD_CLOEXEC);
        memcpy(argv, c->data, c->len * sizeof *argv);
        argv[c->len] = NULL;
        fflush(stdout); /* or its buffered text prints after the child's */
        pid = fork();
        if (pid < 0) {
            err_os(err, errno, "fork", SV(""));
            ok = false;
        }
    }
    if (pid == 0) {
        /* Only async-signal-safe calls between fork and exec. */
        if ((in < 0 || dup2(in, STDIN_FILENO) >= 0) &&
            (child_out < 0 || dup2(child_out, STDOUT_FILENO) >= 0) &&
            (errfd < 0 || dup2(errfd, STDERR_FILENO) >= 0))
            /* exec takes char *const[] for historical reasons only. */
            execvp(argv[0], (char *const *)(uintptr_t)argv);
        int code = errno;
        ssize_t n = write(status_pipe[1], &code, sizeof code);
        (void)n;
        _exit(127);
    }

    proc_close_(status_pipe[1]);
    if (ok) {
        int code = 0;
        ssize_t n;
        do n = read(status_pipe[0], &code, sizeof code);
        while (n < 0 && errno == EINTR);
        if (n == (ssize_t)sizeof code) {
            int status;
            while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
            }
            err_os(err, code, "run", sv_from_cstr(c->data[0]));
            ok = false;
        }
    }
    proc_close_(status_pipe[0]);
    proc_close_(in);
    proc_close_(out);
    if (errfd != out) proc_close_(errfd);
    argv = mem_realloc(NULL, argv, argv_size, 0);
    if (ok) p->id_ = pid;
    return ok;
}

bool proc_start(Proc *p, const Cmd *c, const CmdIo *io, Err *err) {
    return proc_spawn_(p, c, io, -1, err);
}

/* proc_wait, naming the process `name` in the message when it is known. */
static bool proc_wait_(Proc *p, const char *name, Err *err) {
    if (p->id_ == 0) {
        err_set(err, ERR_INVALID, 0, "wait: no process");
        return false;
    }
    pid_t pid = (pid_t)p->id_;
    p->id_ = 0;
    int status = 0;
    pid_t r;
    do r = waitpid(pid, &status, 0);
    while (r < 0 && errno == EINTR);
    if (r < 0) {
        err_os(err, errno, "wait", SV(""));
        return false;
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) return true;

    char who[32];
    if (name == NULL) {
        snprintf(who, sizeof who, "process %ld", (long)pid);
        name = who;
    }
    if (WIFEXITED(status))
        err_set(err, ERR_EXIT, WEXITSTATUS(status), "%s exited with code %d",
                name, WEXITSTATUS(status));
    else
        err_set(err, ERR_SIGNAL, WTERMSIG(status), "%s was killed by signal %d",
                name, WTERMSIG(status));
    return false;
}

bool proc_wait(Proc *p, Err *err) {
    return proc_wait_(p, NULL, err);
}

bool cmd_run(const Cmd *c, Err *err) {
    Proc p = { 0 };
    return proc_start(&p, c, NULL, err) && proc_wait_(&p, c->data[0], err);
}

bool cmd_capture(const Cmd *c, Str *out, Err *err) {
    int fds[2];
    if (pipe(fds) != 0) {
        err_os(err, errno, "pipe", SV(""));
        return false;
    }
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    fcntl(fds[1], F_SETFD, FD_CLOEXEC);
    Proc p = { 0 };
    bool started = proc_spawn_(&p, c, NULL, fds[1], err);
    close(fds[1]); /* or the read below never sees end of file */
    if (!started) {
        close(fds[0]);
        return false;
    }

    /* Read everything before waiting, or a full pipe blocks the child. */
    Str tmp = { .alloc = out->alloc };
    FILE *f = fdopen(fds[0], "rb");
    bool read_ok = f != NULL && str_append_file(&tmp, f);
    if (!read_ok && (f == NULL || ferror(f)))
        err_os(err, errno, "read output of", sv_from_cstr(c->data[0]));
    else if (!read_ok)
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
    if (f != NULL)
        fclose(f);
    else
        close(fds[0]);

    bool ok = proc_wait_(&p, c->data[0], read_ok ? err : NULL) && read_ok;
    if (ok && !str_append(out, str_view(&tmp))) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        ok = false;
    }
    str_free(&tmp);
    return ok;
}

static size_t proc_cpu_count_(void) {
#ifdef __APPLE__
    int n = 0;
    size_t len = sizeof n;
    if (sysctlbyname("hw.ncpu", &n, &len, NULL, 0) != 0 || n < 1) return 1;
    return (size_t)n;
#else
    long n = sysconf(_SC_NPROCESSORS_ONLN);
    return n > 0 ? (size_t)n : 1;
#endif
}

bool proc_pool_start(ProcPool *pool, const Cmd *c, Err *err) {
    size_t max = pool->max != 0 ? pool->max : proc_cpu_count_();
    /* max is at least 1, but gcc 13 with LTO cannot see that past sysconf. */
    while (pool->running.len > 0 && pool->running.len >= max) {
        ProcJob oldest = pool->running.data[0];
        arr_remove(&pool->running, 0);
        if (!proc_wait_(&oldest.proc, oldest.name, err)) return false;
    }
    if (!arr_reserve(&pool->running, 1)) {
        err_set(err, ERR_NO_MEMORY, 0, "run %s: out of memory", c->data[0]);
        return false;
    }
    Proc p = { 0 };
    if (!proc_start(&p, c, NULL, err)) return false;
    pool->running.data[pool->running.len++] = (ProcJob){ p, c->data[0] };
    return true;
}

bool proc_pool_wait(ProcPool *pool, Err *err) {
    bool ok = true;
    for (size_t i = 0; i < pool->running.len; i++) {
        Err e = { 0 };
        ProcJob *job = &pool->running.data[i];
        if (!proc_wait_(&job->proc, job->name, &e) && ok) {
            ok = false;
            if (err != NULL) *err = e;
        }
    }
    arr_clear(&pool->running);
    return ok;
}

void proc_pool_free(ProcPool *pool) {
    if (!proc_pool_wait(pool, NULL)) {
        /* How the jobs ended does not matter once the pool is going. */
    }
    arr_free(&pool->running);
}
