#pragma once
/**
 * @defgroup mod_fs Fs
 * @brief Files, directories and paths.
 *
 * Every function that reaches the operating system takes its paths as
 * `const char *`, because the system calls need NUL-terminated strings.
 * The path functions only cut a path into pieces, so they take and return
 * StrViews and never allocate. See [Views or C strings](@ref cstrings).
 *
 *   - A function that can fail takes an `Err *` last. Its message names
 *     the path where the failure happened.
 *   - A failed call may already have changed the disk. Only
 *     fs_write_atomic() is all or nothing. Each other function says under
 *     *Returns* what a failure can leave behind.
 *   - A Str argument, as in fs_read(), is unchanged when the call fails.
 *   - fs_walk() visits each directory's entries sorted by name, so its order
 *     is the same on every system.
 *
 * @snippet{trimleft} test_fs.c fs
 * @{
 */

#include "base/base.h"
#include "err/err.h"
#include "str/str.h"

/** @name Information
 *  What is at a path.
 *  @{ */

/** What kind of thing a path names. */
typedef enum FsType {
    FS_MISSING = 0, /**< Nothing is there. A zeroed FsStat holds this. */
    FS_FILE,        /**< A regular file. */
    FS_DIR,         /**< A directory. */
    FS_LINK,        /**< A symbolic link, from fs_lstat() or fs_walk(). */
    FS_OTHER,       /**< A device, pipe or socket. */
} FsType;

/** What fs_stat() and fs_lstat() report. */
typedef struct FsStat {
    FsType type;      /**< What the path names. */
    uint64_t size;    /**< Size in bytes. */
    int64_t mtime_ns; /**< Last modification, in nanoseconds since 1970. */
    uint32_t mode;    /**< Permission bits, such as 0644. */
} FsStat;

/** Describe what @p path names, following symbolic links.
 *  @return False when @p path does not exist (#ERR_NOT_FOUND) or cannot be
 *  read. @p st is then unchanged. */
MULE_NODISCARD bool fs_stat(const char *path, FsStat *st, Err *err);
/** Like fs_stat(), but a symbolic link is described as #FS_LINK rather
 *  than followed.
 *  @return False as for fs_stat(). */
MULE_NODISCARD bool fs_lstat(const char *path, FsStat *st, Err *err);
/** True when @p path names something, following symbolic links. A link to
 *  nothing does not exist. */
MULE_NODISCARD bool fs_exists(const char *path);

/** @} */

/** @name Reading and writing
 *  Whole files at once.
 *  @{ */

/** Append the whole file at @p path to @p out.
 *  @return False when the file cannot be opened or read, or memory runs
 *  out. @p out is then unchanged. */
MULE_NODISCARD bool fs_read(Str *out, const char *path, Err *err);
/** Write @p data to @p path, creating the file or replacing its contents.
 *  @return False when the file cannot be opened or written. A file that
 *  existed may then hold part of @p data. */
MULE_NODISCARD bool fs_write(const char *path, StrView data, Err *err);
/** Write @p data to @p path so that a reader sees the old file or the new
 *  one, never a mixture. It writes a temporary file beside @p path, flushes
 *  it to disk and renames it into place. The file keeps its permissions,
 *  and a new file gets 0644.
 *  @return False on any failure. @p path is then as it was, and the
 *  temporary file is gone. */
MULE_NODISCARD bool fs_write_atomic(const char *path, StrView data, Err *err);

/** @} */

/** @name Changing the tree
 *  Make, remove, move and copy.
 *  @{ */

/** Make the directory @p path and any missing parents. A directory that
 *  already exists is success.
 *  @return False when a file is in the way (#ERR_EXISTS for @p path itself,
 *  #ERR_NOT_DIR for a parent), or a directory cannot be made. The parents
 *  made before the failure stay. */
MULE_NODISCARD bool fs_mkdir_p(const char *path, Err *err);
/** Remove a file, a symbolic link or an empty directory.
 *  @return False when @p path does not exist (#ERR_NOT_FOUND), is a
 *  directory with entries, or cannot be removed. */
MULE_NODISCARD bool fs_remove(const char *path, Err *err);
/** Remove @p path and everything under it. Symbolic links are removed, not
 *  followed. A missing @p path is success.
 *  @return False when something cannot be removed. Whatever was removed
 *  before the failure stays removed. */
MULE_NODISCARD bool fs_remove_all(const char *path, Err *err);
/** Move @p from to @p to, replacing a file already at @p to. Both must be
 *  on the same file system.
 *  @return False when the move fails. Nothing has changed then. */
MULE_NODISCARD bool fs_rename(const char *from, const char *to, Err *err);
/** Copy the file @p from to @p to with its permissions, replacing a file
 *  already at @p to.
 *  @return False when either file cannot be opened, read or written. @p to
 *  may then hold part of the copy. */
MULE_NODISCARD bool fs_copy(const char *from, const char *to, Err *err);
/** Copy the tree at @p from to @p to. Directories are made, files are
 *  copied with fs_copy(), and symbolic links are recreated as links.
 *  @return False on the first failure. What was copied before it stays. */
MULE_NODISCARD bool fs_copy_tree(const char *from, const char *to, Err *err);

/** @} */

/** @name Walking
 *  Visit a tree.
 *  @{ */

/** What fs_walk() does after a visit. */
typedef enum FsWalkAct {
    FS_WALK_NEXT, /**< Go on, into this directory if it is one. */
    FS_WALK_SKIP, /**< Go on, but not into this directory. */
    FS_WALK_STOP, /**< End the walk. fs_walk() returns true. */
} FsWalkAct;

/** Called once for each path fs_walk() visits. @p path is valid only
 *  during the call. */
typedef FsWalkAct FsWalkFn(void *ctx, const char *path, FsType type);

/** Visit @p root and everything under it, depth first, with each
 *  directory's entries in byte order. Paths start with @p root. Symbolic
 *  links are visited as #FS_LINK and not followed.
 *  @param root  Where to start.
 *  @param fn  Called for each path.
 *  @param ctx  Passed to @p fn unchanged.
 *  @param err  The error to fill, or null.
 *  @return False when @p root is missing or a directory cannot be read. */
MULE_NODISCARD bool fs_walk(const char *root, FsWalkFn *fn, void *ctx,
                            Err *err);

/** @} */

/** @name Paths
 *  Cut a path into pieces, and build one. Only fs_join() allocates.
 *  @{ */

/** Append the absolute path of the running program to @p out. Unlike
 *  `argv[0]`, it is right when the program was found on `PATH`.
 *  @return False when the system cannot say, or memory runs out. @p out is
 *  then unchanged. */
MULE_NODISCARD bool fs_exe_path(Str *out, Err *err);
/** The last component of @p path, ignoring trailing slashes: `b.c` for
 *  `a/b.c`. A path of only slashes gives `/`. */
MULE_NODISCARD StrView fs_basename(StrView path);
/** Everything before the last component, ignoring trailing slashes: `a` for
 *  `a/b.c`. A path with no slash gives `.`, and one directly under the root
 *  gives `/`. */
MULE_NODISCARD StrView fs_dirname(StrView path);
/** The extension of the last component, from its last dot: `.c` for
 *  `a/b.c`. Empty when there is no dot, or the only dot starts the name, as
 *  in `.build`. */
MULE_NODISCARD StrView fs_extension(StrView path);
/** Append @p part to @p path, with one `/` between them. An empty @p part
 *  adds nothing.
 *  @return False when memory runs out. @p path is then unchanged. */
MULE_NODISCARD bool fs_join(Str *path, StrView part);

/** @} */

/** @} */
