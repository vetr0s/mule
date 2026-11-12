/* log: levelled messages with their source line, filtered at run time. */
#define MULE_IMPLEMENTATION
#include "mule.h"

#include <stdlib.h>

int main(void) {
    log_info("starting with %d workers", 4);
    log_debug("hidden: the default level is LOG_INFO");

    log_set_level(LOG_WARN);
    log_info("hidden: below the level, and its arguments are not evaluated");
    log_warn("disk %d%% full", 91);
    log_error("could not open %s", "cache.db");
    return EXIT_SUCCESS;
}
