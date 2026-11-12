#!/bin/sh
# Print dist/mule.h, the single-header distribution, built from src/. The
# arguments are the module names in dependency order. The test module is
# always appended behind MULE_TEST.
set -eu

# Each file without its include guard pragma or its includes of other modules.
emit() {
    for f; do
        printf '\n/* --- %s --- */\n\n' "$f"
        grep -v -e '^#pragma once' -e '^#include "' "$f"
    done
}

cat <<'EOF'
/* MULE: Multi-Use Library Essentials, as one header.
 *
 * Generated from src/ by `./build dist`. Edit src/ and regenerate.
 *
 * Include it anywhere for the declarations. In exactly one .c file, define
 * MULE_IMPLEMENTATION first to compile the library into that file:
 *
 *     #define MULE_IMPLEMENTATION
 *     #include "mule.h"
 *
 * Include it before any system header. It sets _POSIX_C_SOURCE, which only
 * takes effect before the first one.
 *
 * Define MULE_TEST as well to get the test harness. test_init installs its
 * counting, failing Allocator as the heap, so its leak counting and allocation
 * failure cover the whole library.
 */
#ifndef MULE_H
#define MULE_H
EOF
for m; do emit "src/$m/$m.h"; done
printf '\n#endif /* MULE_H */\n'

printf '\n#if defined(MULE_TEST) && !defined(MULE_TEST_DONE)\n'
printf '#define MULE_TEST_DONE\n'
emit src/test/test.h
printf '\n#endif /* MULE_TEST */\n'

printf '\n#if defined(MULE_IMPLEMENTATION) && !defined(MULE_IMPLEMENTATION_DONE)\n'
printf '#define MULE_IMPLEMENTATION_DONE\n'
for m; do
    if [ -f "src/$m/$m.c" ]; then emit "src/$m/$m.c"; fi
done
printf '\n#endif /* MULE_IMPLEMENTATION */\n'
