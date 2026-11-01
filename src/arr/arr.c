#include "arr/arr.h"

#include <string.h>

/* `data_field` is the address of the array's `T *data`. Copying the pointer
 * through memcpy reads and writes it without aliasing it as a `void *`. */
bool arr_reserve_(void *data_field, size_t *capacity, size_t len, size_t add,
                  size_t elem, const Allocator *alloc) {
    size_t needed;
    if (ckd_add(&needed, len, add)) return false;
    if (needed <= *capacity) return true;

    size_t cap = *capacity != 0 ? *capacity : 8;
    while (cap < needed) {
        if (ckd_mul(&cap, cap, 2)) {
            cap = needed;
            break;
        }
    }
    size_t bytes;
    if (ckd_mul(&bytes, cap, elem)) return false;

    void *old;
    memcpy(&old, data_field, sizeof old);
    void *p = mem_realloc(alloc, old, *capacity * elem, bytes);
    if (p == nullptr) return false;
    memcpy(data_field, &p, sizeof p);
    *capacity = cap;
    return true;
}

void arr_free_(void *data_field, size_t bytes, const Allocator *alloc) {
    void *p;
    memcpy(&p, data_field, sizeof p);
    if (p != nullptr) (void)mem_realloc(alloc, p, bytes, 0);
    p = nullptr;
    memcpy(data_field, &p, sizeof p);
}
