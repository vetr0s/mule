/* The index is open addressing with linear probing over a power-of-two
 * table of uint64_t slots. A slot holds the top 32 bits of its key's hash
 * above the entry's index plus one, so 0 marks an empty slot. A probe
 * compares the stored hash before touching the entry, and growth moves
 * slots without hashing a key again. Removal fills the hole in `data` with
 * the last entry, and backward-shift deletion closes the gap in the index
 * without tombstones. */
#include "map/map.h"

#include <string.h>

static uint32_t map_hash_(StrView key) {
    const char *p = key.data;
    size_t n = key.len;
    uint64_t h = 0x9e3779b97f4a7c15u ^ (uint64_t)n;
    for (; n >= 8; p += 8, n -= 8) {
        uint64_t w;
        memcpy(&w, p, sizeof w);
        h = (h ^ w) * 0xff51afd7ed558ccdu;
        h ^= h >> 32;
    }
    uint64_t w = 0;
    if (n > 0) memcpy(&w, p, n);
    h = (h ^ w) * 0xff51afd7ed558ccdu;
    h ^= h >> 33;
    h *= 0xc4ceb9fe1a85ec53u;
    h ^= h >> 33;
    return (uint32_t)(h >> 32);
}

/* Find the slot that holds `key`, or the empty slot where it would go.
 * `slot_count` must be non-zero, and the table must have an empty slot. */
static bool map_probe_(const char *data, size_t elem, const uint64_t *slots,
                       size_t slot_count, StrView key, uint32_t hash,
                       size_t *pos) {
    size_t mask = slot_count - 1;
    for (size_t p = hash & mask;; p = (p + 1) & mask) {
        uint64_t s = slots[p];
        if (s == 0) {
            *pos = p;
            return false;
        }
        if ((uint32_t)(s >> 32) != hash) continue;
        StrView k;
        memcpy(&k, data + ((uint32_t)s - 1) * elem, sizeof k);
        if (str_eq(k, key)) {
            *pos = p;
            return true;
        }
    }
}

bool map_find_(const void *data, size_t elem, const uint64_t *slots,
               size_t slot_count, StrView key, size_t *i) {
    size_t pos;
    if (slot_count == 0 ||
        !map_probe_(data, elem, slots, slot_count, key, map_hash_(key), &pos))
        return false;
    *i = (uint32_t)slots[pos] - 1;
    return true;
}

bool map_reserve_(void *data_field, size_t *capacity, size_t len, size_t add,
                  size_t elem, const Allocator *alloc, uint64_t **slots,
                  size_t *slot_count) {
    size_t needed;
    if (ckd_add(&needed, len, add)) return false;
    size_t count = *slot_count != 0 ? *slot_count : 8;
    while (needed > count / 4 * 3) {
        if (count >= (size_t)1 << 31) return false;
        count *= 2;
    }

    /* The new index comes first, so a failure leaves the entries as they
     * were. */
    uint64_t *fresh = NULL;
    size_t bytes;
    if (ckd_mul(&bytes, count, sizeof *fresh)) return false;
    if (count != *slot_count) {
        fresh = mem_realloc(alloc, NULL, 0, bytes);
        if (fresh == NULL) return false;
        memset(fresh, 0, bytes);
    }
    if (!arr_reserve_(data_field, capacity, len, add, elem, alloc)) {
        if (fresh != NULL) fresh = mem_realloc(alloc, fresh, bytes, 0);
        return false;
    }
    if (fresh == NULL) return true;

    size_t mask = count - 1;
    for (size_t j = 0; j < *slot_count; j++) {
        uint64_t s = (*slots)[j];
        if (s == 0) continue;
        size_t p = (uint32_t)(s >> 32) & mask;
        while (fresh[p] != 0) p = (p + 1) & mask;
        fresh[p] = s;
    }
    map_free_(slots, slot_count, alloc);
    *slots = fresh;
    *slot_count = count;
    return true;
}

bool map_put_(void *data_field, size_t *len, size_t *capacity,
              const Allocator *alloc, uint64_t **slots, size_t *slot_count,
              size_t elem, StrView key, size_t *i) {
    char *data;
    memcpy(&data, data_field, sizeof data);
    uint32_t hash = map_hash_(key);
    size_t pos = 0;
    if (*slot_count != 0 &&
        map_probe_(data, elem, *slots, *slot_count, key, hash, &pos)) {
        *i = (uint32_t)(*slots)[pos] - 1;
        return true;
    }

    size_t old_count = *slot_count;
    if (!map_reserve_(data_field, capacity, *len, 1, elem, alloc, slots,
                      slot_count))
        return false;
    memcpy(&data, data_field, sizeof data);
    if (*slot_count != old_count)
        (void)map_probe_(data, elem, *slots, *slot_count, key, hash, &pos);

    char *entry = data + *len * elem;
    memset(entry, 0, elem);
    memcpy(entry, &key, sizeof key);
    (*slots)[pos] = (uint64_t)hash << 32 | (uint64_t)(*len + 1);
    *i = (*len)++;
    return true;
}

void map_clear_(uint64_t *slots, size_t slot_count) {
    if (slot_count != 0) memset(slots, 0, slot_count * sizeof *slots);
}

void map_free_(uint64_t **slots, size_t *slot_count, const Allocator *alloc) {
    if (*slots != NULL)
        *slots = mem_realloc(alloc, *slots, *slot_count * sizeof **slots, 0);
    *slot_count = 0;
}

bool map_remove_(void *data, size_t *len, uint64_t *slots, size_t slot_count,
                 size_t elem, StrView key) {
    char *d = data;
    size_t hole;
    if (slot_count == 0 ||
        !map_probe_(d, elem, slots, slot_count, key, map_hash_(key), &hole))
        return false;

    /* Repoint the last entry's slot before its bytes move, so the probe
     * cannot stop at the hole's slot. */
    size_t at = (uint32_t)slots[hole] - 1, last = *len - 1;
    if (at != last) {
        StrView k;
        size_t p;
        memcpy(&k, d + last * elem, sizeof k);
        (void)map_probe_(d, elem, slots, slot_count, k, map_hash_(k), &p);
        slots[p] = (slots[p] & ~(uint64_t)UINT32_MAX) | (uint64_t)(at + 1);
        memcpy(d + at * elem, d + last * elem, elem);
    }

    /* Pull back each later slot in the run whose home is at or before the
     * hole, so no probe chain crosses an empty slot. */
    size_t mask = slot_count - 1;
    for (size_t j = (hole + 1) & mask; slots[j] != 0; j = (j + 1) & mask) {
        size_t home = (uint32_t)(slots[j] >> 32) & mask;
        if (((j - home) & mask) >= ((j - hole) & mask)) {
            slots[hole] = slots[j];
            hole = j;
        }
    }
    slots[hole] = 0;
    *len = last;
    return true;
}
