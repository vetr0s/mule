/* MULE's build, written in C against MULE itself.
 *
 *   cc -Isrc -o build build.c        once; after that it rebuilds itself
 *   ./build [command] [options] [names...]
 *   ./build --help                   lists the commands and options
 *
 * It includes the files in src/ rather than dist/mule.h, because it is what
 * generates dist/mule.h: built from that file, it would run a stale copy of
 * the library after every edit to src/. The self-rebuild adds -Isrc itself.
 *
 * Output goes to .build/ unless --out says otherwise. Each profile builds
 * into its own folder, as debug/main, debug/tests/ and release/bench/, beside
 * docs/ and scale/. Each compile writes a depfile beside its output, so an
 * edit to any header rebuilds what includes it. All strings and commands live
 * in one arena that lives as long as the program, so nothing here frees memory.
 */
#include "base/base.h"

#include "args/args.c"
#include "arr/arr.c"
#include "build/build.c"
#include "err/err.c"
#include "fs/fs.c"
#include "log/log.c"
#include "mem/mem.c"
#include "proc/proc.c"
#include "str/str.c"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* In dependency order: dist/mule.h joins them in this order, and the
 * reference shows a topic per module in this order. */
static const char *const modules[] = { "base", "mem",  "str",  "err", "arr",
                                       "map",  "rand", "time", "log", "fs",
                                       "args", "proc", "build" };
enum { MODULE_COUNT = sizeof modules / sizeof modules[0] };

static const char *const warn_flags[] = {
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wshadow",
    "-Wconversion",
    "-Wsign-conversion",
    "-Wdouble-promotion",
    "-Wformat=2",
    "-Wundef",
    "-Wcast-align",
    "-Wcast-qual",
    "-Wnull-dereference",
    "-Wstrict-prototypes",
    "-Wmissing-prototypes",
    "-Wmissing-declarations",
    "-Wpointer-arith",
    "-Wwrite-strings",
    "-Wswitch-enum",
    "-Wimplicit-fallthrough",
    "-Wvla",
    "-Walloca",
};
/* Kept only when the compiler takes them silently. */
static const char *const gcc_warn_flags[] = {
    "-Wformat-overflow=2",
    "-Wduplicated-cond",
    "-Wduplicated-branches",
    "-Wlogical-op",
};
static const char *const elf_harden_flags[] = {
    "-fno-plt",        "-s",           "-Wl,-O1",
    "-Wl,--as-needed", "-Wl,-z,relro", "-Wl,-z,now",
};
static const char *const sanitize_flag = "-fsanitize=address,undefined";

/* Five books: Crime and Punishment, Moby-Dick, Pride and Prejudice, War and
 * Peace, and Don Quijote in Spanish for non-ASCII UTF-8. */
static const char *const gutenberg_ids[] = { "2554", "2701", "1342", "2600",
                                             "2000" };
#define CORPUS "data/corpus"
#define PIZZACHILI "https://pizzachili.dcc.uchile.cl/texts"
static const char *const corpus_files[] = {
    CORPUS "/gutenberg.txt",  CORPUS "/english.200MB", CORPUS "/sources.200MB",
    CORPUS "/dblp.xml.200MB", CORPUS "/enwik9",
};
enum { CORPUS_COUNT = sizeof corpus_files / sizeof corpus_files[0] };

static Arena arena_;
static const Allocator *mem_;
static Err err_;
static ProcPool pool_;

static struct {
    const char *out;
    const char *cc;
    const char *scale;
    const char *profile;
    bool check;
    uint64_t jobs;
} opt = { .out = ".build", .scale = "10,100,1000", .profile = "debug" };

/* Stop the build with the last error. */
static void must(bool ok) {
    if (ok) return;
    log_error("%s", err_.msg);
    exit(EXIT_FAILURE);
}

static void oom(bool ok) {
    if (!ok) log_fatal("out of memory");
}

/* A formatted string that lives as long as the program. */
MULE_PRINTF(1, 2) static const char *fmt(const char *f, ...) {
    Str s = { .alloc = mem_ };
    va_list ap;
    va_start(ap, f);
    oom(str_vappendf(&s, f, ap));
    va_end(ap);
    return str_cstr(&s);
}

static Cmd cmd_new(void) {
    return (Cmd){ .alloc = mem_ };
}

/* Append arguments to a command, stopping the build when memory runs out. */
#define add(c, ...) oom(cmd_append((c), __VA_ARGS__))

static void add_list(Cmd *c, const char *const *args, size_t n) {
    for (size_t i = 0; i < n; i++) add(c, args[i]);
}

/* --- Finding files ------------------------------------------------------- */

typedef struct Glob {
    Cmd *files;
    size_t root_len;
    size_t depth;
    const char *suffix;
} Glob;

static FsWalkAct glob_visit_(void *ctx, const char *path, FsType type) {
    Glob *g = ctx;
    size_t depth = 0;
    for (const char *p = path + g->root_len; *p != '\0'; p++)
        depth += *p == '/';
    if (type == FS_DIR) return depth < g->depth ? FS_WALK_NEXT : FS_WALK_SKIP;
    if (type == FS_FILE &&
        sv_ends_with(sv_from_cstr(path), sv_from_cstr(g->suffix)))
        add(g->files, fmt("%s", path));
    return FS_WALK_NEXT;
}

/* Append the files under `dir`, at most `depth` levels down, whose names end
 * in `suffix`, sorted. */
static void glob(Cmd *files, const char *dir, size_t depth,
                 const char *suffix) {
    Glob g = { files, strlen(dir), depth, suffix };
    must(fs_walk(dir, glob_visit_, &g, &err_));
}

/* The name of `path` without its directory or extension. */
static const char *stem(const char *path) {
    StrView base = fs_basename(sv_from_cstr(path));
    base.len -= fs_extension(base).len;
    return fmt("%.*s", STR_FMT(base));
}

/* --- Profiles ----------------------------------------------------------- */

/* A set of flags to build with. Each builds into <out>/<name>/, so switching
 * profiles never rebuilds another profile's outputs. */
typedef struct Profile {
    const char *name;
    bool release;  /* the optimised flags rather than the debug ones */
    bool sanitize; /* ASan and UBSan, when the compiler has them */
} Profile;

static const Profile profiles[] = {
    { "debug", false, true },
    { "nosan", false, false },
    { "release", true, false },
};
enum { PROFILE_COUNT = sizeof profiles / sizeof profiles[0] };

static const Profile *profile(const char *name) {
    for (size_t i = 0; i < PROFILE_COUNT; i++)
        if (strcmp(profiles[i].name, name) == 0) return &profiles[i];
    log_fatal("no profile '%s': debug, nosan or release", name);
}

/* Compile an empty program once per flag, all at once, and report which
 * flags the compiler took: with no output at all when `quiet`, else with
 * exit status 0. */
static void probe(const char *const *flags, size_t n, bool quiet, bool *keep) {
    const char *dir = fmt("%s/probe", opt.out);
    const char *src = fmt("%s/probe.c", dir);
    must(fs_mkdir_p(dir, &err_) &&
         fs_write(src, SV("int main(void) { return 0; }\n"), &err_));
    Proc *procs = mem_realloc(mem_, NULL, 0, n * sizeof *procs);
    const char **logs = mem_realloc(mem_, NULL, 0, n * sizeof *logs);
    oom(procs != NULL && logs != NULL);
    for (size_t i = 0; i < n; i++) {
        Cmd c = cmd_new();
        add(&c, opt.cc, flags[i], src, "-o", "/dev/null");
        logs[i] = fmt("%s/probe-%zu.log", dir, i);
        CmdIo io = { .out = logs[i], .err = logs[i] };
        procs[i] = (Proc){ 0 };
        must(proc_start(&procs[i], &c, &io, &err_));
    }
    for (size_t i = 0; i < n; i++) {
        FsStat st = { 0 };
        keep[i] = proc_wait(&procs[i], NULL) &&
                  (!quiet || (fs_stat(logs[i], &st, NULL) && st.size == 0));
    }
}

/* The flags `p` builds with, worked out on first use. The probes run once. */
static const Cmd *flags_of(const Profile *p) {
    enum {
        GCC = sizeof gcc_warn_flags / sizeof gcc_warn_flags[0],
        ELF = sizeof elf_harden_flags / sizeof elf_harden_flags[0]
    };
    static bool probed = false;
    static bool keep[GCC + ELF + 1];
    static Cmd done[PROFILE_COUNT];
    Cmd *f = &done[p - profiles];
    if (f->len > 0) return f;
    if (!probed) {
        probed = true;
        const char *flags[GCC + ELF + 1];
        memcpy(flags, gcc_warn_flags, sizeof gcc_warn_flags);
        memcpy(flags + GCC, elf_harden_flags, sizeof elf_harden_flags);
        flags[GCC + ELF] = sanitize_flag;
        probe(flags, GCC + ELF, true, keep);
        probe(flags + GCC + ELF, 1, false, keep + GCC + ELF);
    }

    *f = cmd_new();
    add(f, "-std=c11");
    add_list(f, warn_flags, sizeof warn_flags / sizeof warn_flags[0]);
    for (size_t i = 0; i < GCC; i++)
        if (keep[i]) add(f, gcc_warn_flags[i]);
    if (p->release) {
        add(f, "-Werror", "-O3", "-march=native", "-flto=auto",
            "-fstack-protector-strong", "-D_FORTIFY_SOURCE=3", "-DNDEBUG");
        for (size_t i = 0; i < ELF; i++)
            if (keep[GCC + i]) add(f, elf_harden_flags[i]);
        return f;
    }
    add(f, "-O0", "-g3", "-fno-omit-frame-pointer", "-fstack-protector-strong");
    if (p->sanitize && keep[GCC + ELF])
        add(f, sanitize_flag, "-fno-sanitize-recover=all");
    add(f, "-DDEBUG");
    return f;
}

/* Where `p` puts its output: <out>/<name>/`sub`. */
static const char *out_of(const Profile *p, const char *sub) {
    return fmt("%s/%s/%s", opt.out, p->name, sub);
}

/* --- Compiling ----------------------------------------------------------- */

/* The command that compiles `src` into `out`. `lib` is a library flag such
 * as -lm, or null. */
static Cmd compile_cmd(const char *src, const char *out, const Cmd *flags,
                       const char *lib) {
    Cmd c = cmd_new();
    add(&c, opt.cc, "-Isrc", "-Idist", "-MMD", "-MP", "-MF", fmt("%s.d", out));
    add_list(&c, flags->data, flags->len);
    add(&c, src, "-o", out);
    if (lib != NULL) add(&c, lib);
    return c;
}

/* Start compiling `src` into `out`, unless `out` is up to date. */
static void compile(const char *src, const char *out, const Cmd *flags,
                    const char *lib) {
    Cmd c = compile_cmd(src, out, flags, lib);
    bool needed = false;
    must(fs_mkdir_p(fmt("%.*s", STR_FMT(fs_dirname(sv_from_cstr(out)))),
                    &err_) &&
         build_prepare(out, fmt("%s.d", out), &c, &needed, &err_));
    if (needed) must(build_start(&pool_, &c, &err_));
}

/* --- dist/mule.h --------------------------------------------------------- */

static const char dist_preamble[] =
    "/* MULE: Multi-Use Library Essentials, as one header.\n"
    " *\n"
    " * Generated from src/ by `./build dist`. Edit src/ and regenerate.\n"
    " *\n"
    " * Include it anywhere for the declarations. In exactly one .c file, "
    "define\n"
    " * MULE_IMPLEMENTATION first to compile the library into that file:\n"
    " *\n"
    " *     #define MULE_IMPLEMENTATION\n"
    " *     #include \"mule.h\"\n"
    " *\n"
    " * Include it before any system header. It sets _POSIX_C_SOURCE, which "
    "only\n"
    " * takes effect before the first one.\n"
    " *\n"
    " * Define MULE_TEST as well to get the test harness. test_init installs "
    "its\n"
    " * counting, failing Allocator as the heap, so its leak counting and "
    "allocation\n"
    " * failure cover the whole library.\n"
    " */\n"
    "#ifndef MULE_H\n"
    "#define MULE_H\n";

/* Append `path` to `out` without its include guard pragma or its includes of
 * other modules, which the single header makes redundant. */
static void emit(Str *out, const char *path) {
    Str text = { .alloc = mem_ };
    must(fs_read(&text, path, &err_));
    oom(str_appendf(out, "\n/* --- %s --- */\n\n", path));
    StrView rest = str_view(&text), line;
    while (sv_split_next(&rest, SV("\n"), &line)) {
        if (sv_starts_with(line, SV("#pragma once")) ||
            sv_starts_with(line, SV("#include \"")))
            continue;
        oom(str_append(out, line) && str_push(out, '\n'));
    }
}

/* Regenerate dist/mule.h, writing it only when its text changes, so what
 * includes it is not rebuilt for nothing. True when it wrote. */
static bool dist(void) {
    Str out = { .alloc = mem_ }, old = { .alloc = mem_ };
    oom(str_append_cstr(&out, dist_preamble));
    for (size_t i = 0; i < MODULE_COUNT; i++)
        emit(&out, fmt("src/%s/%s.h", modules[i], modules[i]));
    oom(str_append_cstr(&out, "\n#endif /* MULE_H */\n"));

    oom(str_append_cstr(&out, "\n#if defined(MULE_TEST) && "
                              "!defined(MULE_TEST_DONE)\n"
                              "#define MULE_TEST_DONE\n"));
    emit(&out, "src/test/test.h");
    oom(str_append_cstr(&out, "\n#endif /* MULE_TEST */\n"));

    oom(str_append_cstr(&out, "\n#if defined(MULE_IMPLEMENTATION) && "
                              "!defined(MULE_IMPLEMENTATION_DONE)\n"
                              "#define MULE_IMPLEMENTATION_DONE\n"));
    for (size_t i = 0; i < MODULE_COUNT; i++) {
        const char *c = fmt("src/%s/%s.c", modules[i], modules[i]);
        if (fs_exists(c)) emit(&out, c);
    }
    oom(str_append_cstr(&out, "\n#endif /* MULE_IMPLEMENTATION */\n"));

    Err ignored = { 0 };
    if (fs_read(&old, "dist/mule.h", &ignored) &&
        str_eq(str_view(&old), str_view(&out)))
        return false;
    must(fs_mkdir_p("dist", &err_) &&
         fs_write_atomic("dist/mule.h", str_view(&out), &err_));
    log_info("wrote dist/mule.h");
    return true;
}

/* The version in src/base/base.h. */
static const char *version(void) {
    Str text = { .alloc = mem_ };
    must(fs_read(&text, "src/base/base.h", &err_));
    StrView rest = str_view(&text), line;
    StrView key = SV("#define MULE_VERSION \"");
    while (sv_split_next(&rest, SV("\n"), &line)) {
        if (!sv_starts_with(line, key)) continue;
        StrView v = sv_slice(line, key.len, line.len);
        size_t end = sv_find_byte(v, '"');
        if (end != SV_NPOS) return fmt("%.*s", STR_FMT(sv_slice(v, 0, end)));
    }
    log_fatal("no MULE_VERSION in src/base/base.h");
}

/* --- Commands ------------------------------------------------------------ */

static void wait_all(void) {
    must(proc_pool_wait(&pool_, &err_));
}

static void run(Cmd *c) {
    must(build_run(c, &err_));
}

static const char *release_main(void) {
    return out_of(profile("release"), "main");
}

static void cmd_debug(const Profile *p) {
    dist();
    compile("main.c", out_of(p, "main"), flags_of(p), NULL);
    wait_all();
}

/* compile_commands.json, for clangd: main.c, the tests and the benchmarks,
 * all with the debug flags, as the unity files that pull in src/. */
static void cmd_compile_commands(void) {
    const Profile *p = profile("debug");
    Cmd files = cmd_new();
    add(&files, "main.c");
    glob(&files, "tests", 1, ".c");
    glob(&files, "bench", 1, ".c");
    CompDb db = { .json = { .alloc = mem_ } };
    for (size_t i = 0; i < files.len; i++) {
        StrView dir = fs_dirname(sv_from_cstr(files.data[i]));
        const char *out = str_eq(dir, SV("."))
                              ? out_of(p, stem(files.data[i]))
                              : out_of(p, fmt("%.*s/%s", STR_FMT(dir),
                                              stem(files.data[i])));
        Cmd c = compile_cmd(files.data[i], out, flags_of(p), NULL);
        oom(build_compdb_add(&db, files.data[i], &c));
    }
    must(build_compdb_write(&db, "compile_commands.json", &err_));
}

static void benchmarks(void) {
    const Profile *p = profile("release");
    Cmd srcs = cmd_new();
    glob(&srcs, "bench", 1, ".c");
    for (size_t i = 0; i < srcs.len; i++) {
        const char *name = stem(srcs.data[i]);
        const char *bin = out_of(p, fmt("bench/%s", name));
        compile(srcs.data[i], bin, flags_of(p),
                strcmp(name, "bench_corpus") == 0 ? "-lm" : NULL);
    }
}

static void cmd_release(void) {
    dist();
    compile("main.c", release_main(), flags_of(profile("release")), NULL);
    benchmarks();
    wait_all();
}

static int cmd_test(const Profile *p, const ArgList *names) {
    dist();
    Cmd srcs = cmd_new(), bins = cmd_new();
    glob(&srcs, "tests", 1, ".c");
    for (size_t i = 0; i < srcs.len; i++) {
        const char *name = stem(srcs.data[i]);
        if (strncmp(name, "test_", 5) != 0) continue;
        const char *bin = out_of(p, fmt("tests/%s", name));
        compile(srcs.data[i], bin, flags_of(p), NULL);
        add(&bins, bin);
    }
    wait_all();

    int pass = 0, fail = 0;
    for (size_t i = 0; i < bins.len; i++) {
        Cmd c = cmd_new();
        add(&c, bins.data[i]);
        add_list(&c, names->data, names->len);
        if (cmd_run(&c, NULL))
            pass++;
        else
            fail++;
    }
    printf("== %d test file(s) passed, %d failed ==\n", pass, fail);
    return fail == 0 ? 0 : 1;
}

static void cmd_docs(void) {
    const char *out = fmt("%s/docs", opt.out);
    Str dirs = { .alloc = mem_ };
    for (size_t i = 0; i < MODULE_COUNT; i++)
        oom(str_appendf(&dirs, "src/%s ", modules[i]));
    /* The test harness is not a module of the library, so it comes last. */
    oom(str_append_cstr(&dirs, "src/test"));
    if (setenv("MULE_DOC_MODULES", str_cstr(&dirs), 1) != 0 ||
        setenv("MULE_DOC_VERSION", version(), 1) != 0 ||
        setenv("MULE_DOC_OUT", out, 1) != 0)
        log_fatal("setenv failed");
    must(fs_mkdir_p(out, &err_));
    Cmd c = cmd_new();
    add(&c, "doxygen", "Doxyfile");
    run(&c);
    printf("open %s/html/index.html\n", out);
}

/* Build and run each program in examples/. The guide's examples only
 * compile, because 10_dangling.c and 11_double_free.c are bugs on purpose. */
static void cmd_examples(const Profile *p) {
    dist();
    Cmd c = cmd_new();
    add(&c, opt.cc, "-std=c11", "-Idist", "-fsyntax-only");
    glob(&c, "notes/str-examples", 1, ".c");
    run(&c);

    Cmd flags = cmd_new(), srcs = cmd_new(), bins = cmd_new();
    add_list(&flags, flags_of(p)->data, flags_of(p)->len);
    if (!p->release) add(&flags, "-Werror");
    glob(&srcs, "examples", 1, ".c");
    for (size_t i = 0; i < srcs.len; i++) {
        const char *bin = out_of(p, fmt("examples/%s", stem(srcs.data[i])));
        compile(srcs.data[i], bin, &flags, NULL);
        add(&bins, bin);
    }
    wait_all();
    for (size_t i = 0; i < bins.len; i++) {
        c = cmd_new();
        add(&c, bins.data[i]);
        run(&c);
    }
}

/* Format every source file in place, or with --check only report the ones
 * that differ. */
static void cmd_format(void) {
    Cmd c = cmd_new();
    add(&c, "clang-format");
    if (opt.check)
        add(&c, "--dry-run", "--Werror");
    else
        add(&c, "-i");
    add(&c, "main.c", "build.c");
    glob(&c, "src", 2, ".c");
    glob(&c, "src", 2, ".h");
    glob(&c, "tests", 1, ".c");
    glob(&c, "tests", 1, ".h");
    glob(&c, "bench", 1, ".c");
    glob(&c, "bench", 1, ".h");
    glob(&c, "examples", 2, ".c");
    glob(&c, "notes/str-examples", 1, ".c");
    run(&c);
}

/* Run `c` with its stdout in the file `out`. */
static void run_to(Cmd *c, const char *out) {
    Str line = { .alloc = mem_ };
    oom(cmd_render(&line, c));
    log_info("%s > %s", str_cstr(&line), out);
    CmdIo io = { .out = out };
    Proc p = { 0 };
    must(proc_start(&p, c, &io, &err_) && proc_wait(&p, &err_));
}

/* Download `url` to `dest` through a temporary, so a failed download leaves
 * nothing that fs_exists would take for the file. */
static void fetch(const char *url, const char *dest) {
    const char *tmp = fmt("%s.tmp", dest);
    Cmd c = cmd_new();
    add(&c, "curl", "-fsSL", "-o", tmp, url);
    run(&c);
    must(fs_rename(tmp, dest, &err_));
}

/* The Project Gutenberg book `id` as data/pg<id>.txt, downloaded if missing. */
static const char *book(const char *id) {
    const char *path = fmt("data/pg%s.txt", id);
    must(fs_mkdir_p("data", &err_));
    if (!fs_exists(path))
        fetch(fmt("https://www.gutenberg.org/cache/epub/%s/pg%s.txt", id, id),
              path);
    return path;
}

/* Download a gzipped Pizza & Chili text into `dest`. */
static void fetch_gz(const char *path, const char *dest) {
    const char *gz = fmt("%s.gz", dest), *tmp = fmt("%s.tmp", dest);
    fetch(fmt("%s/%s.gz", PIZZACHILI, path), gz);
    Cmd c = cmd_new();
    add(&c, "gunzip", "-c", gz);
    run_to(&c, tmp);
    must(fs_rename(tmp, dest, &err_) && fs_remove(gz, &err_));
}

/* Download each missing corpus file into data/corpus/, which clean keeps. */
static void cmd_corpus(void) {
    must(fs_mkdir_p(CORPUS, &err_));
    if (!fs_exists(corpus_files[0])) {
        Str all = { .alloc = mem_ };
        for (size_t i = 0; i < sizeof gutenberg_ids / sizeof *gutenberg_ids;
             i++)
            must(fs_read(&all, book(gutenberg_ids[i]), &err_));
        must(fs_write_atomic(corpus_files[0], str_view(&all), &err_));
    }
    if (!fs_exists(corpus_files[1]))
        fetch_gz("nlang/english.200MB", corpus_files[1]);
    if (!fs_exists(corpus_files[2]))
        fetch_gz("code/sources.200MB", corpus_files[2]);
    if (!fs_exists(corpus_files[3]))
        fetch_gz("xml/dblp.xml.200MB", corpus_files[3]);
    /* The first 10^9 bytes of English Wikipedia, from the Hutter Prize. */
    if (!fs_exists(corpus_files[4])) {
        const char *zip = CORPUS "/enwik9.zip", *tmp = CORPUS "/enwik9.tmp";
        fetch("https://mattmahoney.net/dc/enwik9.zip", zip);
        Cmd c = cmd_new();
        add(&c, "unzip", "-p", zip, "enwik9");
        run_to(&c, tmp);
        must(fs_remove(zip, &err_) && fs_rename(tmp, corpus_files[4], &err_));
    }
}

/* Write exactly `bytes` bytes to `path`: `text` repeated when it is not
 * empty, or else the numbers from 1 up, one per line. */
static void write_scaled(const char *path, StrView text, uint64_t bytes) {
    const char *tmp = fmt("%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (f == NULL) log_fatal("open %s: %s", tmp, strerror(errno));
    char line[32];
    for (uint64_t left = bytes, n = 1; left > 0; n++) {
        StrView piece = text;
        if (text.len == 0) {
            int len = snprintf(line, sizeof line, "%llu\n",
                               (unsigned long long)n);
            piece = (StrView){ line, (size_t)len };
        }
        size_t take = piece.len < left ? piece.len : (size_t)left;
        if (fwrite(piece.data, 1, take, f) != take)
            log_fatal("write %s: %s", tmp, strerror(errno));
        left -= take;
    }
    if (fclose(f) != 0) log_fatal("close %s: %s", tmp, strerror(errno));
    must(fs_rename(tmp, path, &err_));
}

/* The generated files for bench-scale, in out/scale/, made when missing.
 * bench_corpus peaks near 3.5x its largest file: 4096 MiB needs about 14 GB. */
static void scale_files(Cmd *text, Cmd *seq, Str *mibs) {
    const char *dir = fmt("%s/scale", opt.out);
    must(fs_mkdir_p(dir, &err_));
    Str book = { .alloc = mem_ };
    must(fs_read(&book, corpus_files[0], &err_));
    StrView rest = sv_from_cstr(opt.scale), mib;
    while (sv_split_next(&rest, SV(","), &mib)) {
        uint64_t n = 0;
        if (!sv_parse_u64(sv_trim(mib), 10, &n) || n == 0)
            log_fatal("--scale: expected sizes in MiB, got '%s'", opt.scale);
        const char *t = fmt("%s/text-%lluM", dir, (unsigned long long)n);
        const char *s = fmt("%s/seq-%lluM", dir, (unsigned long long)n);
        if (!fs_exists(t)) write_scaled(t, str_view(&book), n * 1048576);
        if (!fs_exists(s)) write_scaled(s, SV(""), n * 1048576);
        add(text, t);
        add(seq, s);
        oom(str_appendf(mibs, "%s%llu", mibs->len ? "," : "",
                        (unsigned long long)n));
    }
}

/* Run the benchmark `name` on `files`, after `flag` unless it is null. */
static void bench_on(const char *name, const char *flag,
                     const char *const *files, size_t n) {
    Cmd c = cmd_new();
    add(&c, out_of(profile("release"), fmt("bench/%s", name)));
    if (flag != NULL) add(&c, flag);
    add_list(&c, files, n);
    run(&c);
}

static void cmd_bench(const char *which) {
    dist();
    if (strcmp(which, "bench") != 0) cmd_corpus();
    compile("main.c", release_main(), flags_of(profile("release")), NULL);
    benchmarks();
    wait_all();

    if (strcmp(which, "bench") == 0) {
        bench_on("bench_str", NULL, NULL, 0);
    } else if (strcmp(which, "bench-corpus") == 0) {
        bench_on("bench_corpus", NULL, corpus_files, CORPUS_COUNT);
        bench_on("bench_views", NULL, corpus_files, CORPUS_COUNT);
    } else if (strcmp(which, "bench-map") == 0) {
        bench_on("bench_map", NULL, corpus_files, CORPUS_COUNT);
    } else {
        Cmd text = cmd_new(), seq = cmd_new(), h = cmd_new();
        Str mibs = { .alloc = mem_ };
        scale_files(&text, &seq, &mibs);
        bench_on("bench_corpus", "--scale", text.data, text.len);
        bench_on("bench_corpus", "--scale", seq.data, seq.len);
        add(&h, "hyperfine", "-N", "--warmup", "1", "--export-markdown",
            fmt("%s/scale/main.md", opt.out), "-L", "kind", "text,seq", "-L",
            "mib", str_cstr(&mibs),
            fmt("%s %s/scale/{kind}-{mib}M", release_main(), opt.out));
        run(&h);
    }
}

static void cmd_run_main(const Profile *p, const ArgList *rest) {
    cmd_debug(p);
    Cmd c = cmd_new();
    add(&c, out_of(p, "main"));
    if (rest->len == 0)
        add(&c, book("2554"));
    else
        add_list(&c, rest->data, rest->len);
    run(&c);
}

static void ci_step(const char *name) {
    printf("== ci: %s ==\n", name);
    fflush(stdout);
}

/* Every check CI runs on a push, except the docs, which need doxygen 1.18.
 * It stops at the first that fails. */
static int cmd_ci(void) {
    ci_step("dist/mule.h matches src/");
    if (dist()) {
        log_error("dist/mule.h was stale; commit the regenerated one");
        return 1;
    }
    ArgList all = { 0 };
    ci_step("test, debug");
    if (cmd_test(profile("debug"), &all) != 0) return 1;
    /* glibc's realloc can grow in place, which ASan's never does, and
     * heap_growth_returns_memory needs the real libmalloc on macOS. */
    ci_step("test, nosan");
    if (cmd_test(profile("nosan"), &all) != 0) return 1;
    /* -O3 and LTO see through the tests, and -DNDEBUG drops their asserts. */
    ci_step("test, release");
    if (cmd_test(profile("release"), &all) != 0) return 1;
    ci_step("release");
    cmd_release();
    ci_step("examples");
    cmd_examples(profile("debug"));
    ci_step("main on the book");
    Cmd c = cmd_new();
    add(&c, release_main(), book("2554"));
    run(&c);

    ci_step("examples/project");
    if (chdir("examples/project") != 0)
        log_fatal("chdir examples/project: %s", strerror(errno));
    c = cmd_new();
    add(&c, opt.cc, "build.c");
    run(&c);
    c = cmd_new();
    add(&c, "./a.out", "run");
    run(&c);
    if (chdir("../..") != 0) log_fatal("chdir ../..: %s", strerror(errno));
    return 0;
}

static const ArgCmd commands[] = {
    { "debug", "debug build of main, and compile_commands.json (default)" },
    { "release", "release build of main with -Werror, and the benchmarks" },
    { "run", "debug build, then run main on a file (data/pg2554.txt)" },
    { "test", "build and run every tests/test_*.c, or only the named tests" },
    { "ci", "every check CI runs on a push, except docs" },
    { "dist", "regenerate dist/mule.h, the single-header library" },
    { "docs", "Doxygen reference in <out>/docs/html/" },
    { "examples",
      "build and run examples/, compile-check notes/str-examples/" },
    { "version", "print the version in src/base/base.h" },
    { "bench", "micro-benchmarks of str_append and friends" },
    { "bench-corpus", "benchmarks on the corpus, against string.h" },
    { "bench-map", "main.c's word-count phases and the map on the corpus" },
    { "bench-scale", "benchmarks on generated files, and hyperfine on main" },
    { "data", "download data/pg2554.txt, the book main and the guide read" },
    { "corpus", "download the benchmark corpus into data/corpus/" },
    { "format", "clang-format every source file, or --check them" },
    { "clean", "remove <out>, including generated scale files" },
};

int main(int argc, char **argv) {
    Cmd self = { 0 };
    if (!cmd_append(&self, "-Isrc")) return EXIT_FAILURE;
    build_rebuild_self(argc, argv, __FILE__, &self);
    arr_free(&self);

    mem_ = arena_allocator(&arena_);
    const char *cc = getenv("CC");
    opt.cc = cc != NULL && cc[0] != '\0' ? cc : "cc";

    const ArgOpt opts[] = {
        { "out", 'o', ARG_STR, &opt.out, "DIR", "where output goes (.build)" },
        { "profile", 'p', ARG_STR, &opt.profile, "NAME",
          "debug, nosan or release, for debug, run, test and examples" },
        { "check", 0, ARG_FLAG, &opt.check, NULL,
          "with format: report unformatted files, change nothing" },
        { "jobs", 'j', ARG_U64, &opt.jobs, "N",
          "compile N at once (the CPU count)" },
        { "scale", 0, ARG_STR, &opt.scale, "MIBS",
          "sizes for bench-scale (10,100,1000)" },
    };
    Args a = { .usage = "build [command] [options] [names...]\n\n"
                        "Bootstrap once with: cc -Isrc -o build build.c\n"
                        "The compiler is $CC, or cc.",
               .opts = opts,
               .opt_count = sizeof opts / sizeof opts[0],
               .cmds = commands,
               .cmd_count = sizeof commands / sizeof commands[0],
               .rest = { .alloc = mem_ } };
    if (!args_parse(&a, argc, argv, &err_)) {
        log_error("%s (see --help)", err_.msg);
        return 2;
    }
    if (a.help) {
        Str help = { .alloc = mem_ };
        oom(args_help(&help, &a));
        fputs(str_cstr(&help), stdout);
        return 0;
    }
    pool_.max = (size_t)opt.jobs;

    const char *cmd = a.cmd != NULL ? a.cmd : "debug";
    const Profile *p = profile(opt.profile);
    int status = 0;
    if (strcmp(cmd, "debug") == 0) {
        cmd_debug(p);
        cmd_compile_commands();
    } else if (strcmp(cmd, "release") == 0) {
        cmd_release();
    } else if (strcmp(cmd, "run") == 0) {
        cmd_run_main(p, &a.rest);
    } else if (strcmp(cmd, "test") == 0) {
        status = cmd_test(p, &a.rest);
    } else if (strcmp(cmd, "ci") == 0) {
        status = cmd_ci();
    } else if (strcmp(cmd, "dist") == 0) {
        dist();
    } else if (strcmp(cmd, "docs") == 0) {
        cmd_docs();
    } else if (strcmp(cmd, "examples") == 0) {
        cmd_examples(p);
    } else if (strcmp(cmd, "version") == 0) {
        puts(version());
    } else if (strncmp(cmd, "bench", 5) == 0) {
        cmd_bench(cmd);
    } else if (strcmp(cmd, "data") == 0) {
        book("2554");
    } else if (strcmp(cmd, "corpus") == 0) {
        cmd_corpus();
    } else if (strcmp(cmd, "format") == 0) {
        cmd_format();
    } else if (strcmp(cmd, "clean") == 0) {
        must(fs_remove_all(opt.out, &err_));
    }
    arena_free(&arena_);
    return status;
}
