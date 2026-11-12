#pragma once
/**
 * @defgroup mod_base Base
 * @brief What every module needs, included before anything else.
 *
 * Every header includes base.h first, so a program that includes any
 * module gets all of this:
 *   - `_POSIX_C_SOURCE` set to 200809L, unless the program set it first.
 *     It only takes effect before the first system header, so include MULE
 *     before any of them.
 *   - `<stdbool.h>`, `<stddef.h>` and `<stdint.h>`.
 *   - `ckd_add`, `ckd_sub` and `ckd_mul` for overflow-checked arithmetic.
 *     They come from C23's `<stdckdint.h>` when the compiler has it, and
 *     from the gcc and clang builtins otherwise.
 *   - The `MULE_NODISCARD`, `MULE_UNUSED`, `MULE_NORETURN` and
 *     `MULE_PRINTF` attribute macros. They are left out of the signatures
 *     in this reference.
 *   - `MULE_ASAN`, which is 1 when AddressSanitizer is on and 0 otherwise.
 *
 * ## Layout rules
 *
 * These are for changing the library, not using it.
 *   - One folder per module under src/, holding `name.h` and `name.c`. A
 *     module of only macros and static functions is `name.h` alone.
 *   - Every header includes `base/base.h` first, and include paths are
 *     relative to src/ (`#include "str/str.h"`).
 *   - A `.c` file includes headers only, never another `.c`. Each program's
 *     unity file includes every `.c` it needs. Any order works, because
 *     each `.c` includes the headers it uses, so clang-format may sort them.
 *   - The whole program is one translation unit, so `static` does not keep
 *     names apart. Static helpers carry the module prefix and a trailing `_`.
 *   - Something belongs here only once two modules use it.
 *
 * Targets are Unix with clang 17+ or gcc 13+. The floor is C11, written as
 * C99. Attributes go through the MULE_ macros below, so a compiler without
 * GNU attributes still builds the library.
 * @{
 */

/** @cond */
/* Feature macros only take effect before the first system header. */
#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* gcc ignores a (void) cast on warn_unused_result, so discard by assigning. */
#ifdef __GNUC__
#define MULE_NODISCARD __attribute__((warn_unused_result))
#define MULE_UNUSED __attribute__((unused))
#define MULE_NORETURN __attribute__((noreturn))
#define MULE_PRINTF(fmt, args) __attribute__((format(printf, fmt, args)))
#else
#define MULE_NODISCARD
#define MULE_UNUSED
#define MULE_NORETURN _Noreturn
#define MULE_PRINTF(fmt, args)
#endif

/* gcc says so with __SANITIZE_ADDRESS__, and clang with __has_feature. */
#if defined(__SANITIZE_ADDRESS__)
#define MULE_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define MULE_ASAN 1
#endif
#endif
#ifndef MULE_ASAN
#define MULE_ASAN 0
#endif

#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
/* gcc 13 has the builtins but not the C23 header. */
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif
/** @endcond */

/** @name Version
 *  The library's version, which follows Semantic Versioning. See
 *  CHANGELOG.md for what changed in each one.
 *  @{ */
/** Bumped for breaking changes, once the version reaches 1.0.0. */
#define MULE_VERSION_MAJOR 0
/** Bumped for new features. Before 1.0.0, also for breaking changes. */
#define MULE_VERSION_MINOR 3
/** Bumped for fixes that change no behaviour a program relies on. */
#define MULE_VERSION_PATCH 0
/** All three numbers as a string, such as "0.0.1". */
#define MULE_VERSION "0.3.0"
/** @} */

/** @} */
