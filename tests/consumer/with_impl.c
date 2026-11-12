/* A project that uses dist/mule.h, in two files linked together.
 *
 * This file compiles the library with MULE_IMPLEMENTATION, and header_only.c
 * only includes the header, the way a project does. `./build consumer` builds
 * the pair under C11, C17 and C23 and with LOG_LEVEL_MIN raised, with -Werror.
 * A header that defines a symbol twice, or only compiles under one standard,
 * fails here, where a single-file test cannot see it.
 */
#define MULE_IMPLEMENTATION
#include "mule.h"

int consumer_header_only(void);

int main(void) {
    return consumer_header_only();
}
