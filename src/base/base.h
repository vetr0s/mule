#pragma once
/**
 * @file
 * @brief Base: what every MULE module needs, included before anything else.
 *
 * Layout rules for the library:
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

#if __has_include(<stdckdint.h>)
#include <stdckdint.h>
#else
/* gcc 13 has the builtins but not the C23 header. */
#define ckd_add(r, a, b) __builtin_add_overflow((a), (b), (r))
#define ckd_sub(r, a, b) __builtin_sub_overflow((a), (b), (r))
#define ckd_mul(r, a, b) __builtin_mul_overflow((a), (b), (r))
#endif
/** @endcond */
