#pragma once
/**
 * @defgroup mod_map Map
 * @brief A hash map from StrView keys to entries of any struct type.
 *
 * Map(E) is an Arr(E) with a hash index beside it. The entries sit in
 * `data` in the order they were added, so a plain loop visits them, and the
 * index finds one by its key in constant time.
 *
 * @snippet{trimleft} test_map.c map_type
 * @snippet{trimleft} test_map.c map
 *
 *   - E is a struct whose first member is `StrView key`. Its other members
 *     are the value, with whatever fields the caller needs. A struct with
 *     only a key makes a set. Map(E) fails to compile when E does not start
 *     with the key.
 *   - Name each map type once with a typedef, as for Arr.
 *   - A zero-initialised map (`WordMap m = { 0 };`) is valid, empty, and
 *     owns no memory. `alloc` picks where both the entries and the index
 *     live, and null means the libc heap.
 *   - map_put() and map_find() give the index of an entry in `data`. An
 *     index stays valid as the map grows. A pointer into `data` does not.
 *   - Change any field of an entry except `key`. Changing a key or
 *     reordering `data`, as qsort does, breaks every later lookup, so sort
 *     only after the last one.
 *   - Calls that may allocate return `bool`. On false the map is unchanged.
 *
 * ## Who owns the keys
 *
 * The map stores each key as a StrView and never copies its bytes. Like any
 * view, a key is valid only while the bytes it points at stay alive and in
 * place, and the map needs every key valid for as long as it is used.
 *
 * Keys cut from one buffer that outlives the map cost nothing, as when the
 * words of a file read into a Str are counted. When the bytes do not live
 * long enough, as with a line buffer that str_read_line() reuses, copy each
 * new key into an arena before adding it, and free the arena with the map.
 *
 * @snippet{trimleft} test_map.c own_keys
 *
 * The hash is not seeded, so keys chosen by an attacker to collide can slow
 * every lookup to a linear scan.
 *
 * @warning The macros evaluate the map argument more than once. Pass a
 * variable or a plain pointer, never an expression with side effects.
 * @{
 */

#include "base/base.h"

#include "arr/arr.h"
#include "mem/mem.h"
#include "str/str.h"

/** A hash map whose entries are @p E, a struct that starts with
 *  `StrView key`. Declare it with a typedef before use. */
#define Map(E)                                                                 \
    struct {                                                                   \
        E *data;                                                               \
        size_t len;                                                            \
        size_t capacity;                                                       \
        const Allocator *alloc;                                                \
        uint64_t *slots;                                                       \
        size_t slot_count;                                                     \
        _Static_assert(offsetof(E, key) == 0 &&                                \
                           _Generic(((E *)0)->key, StrView: 1, default: 0),    \
                       "the first member of a map entry must be StrView key"); \
    }

/** The most entries a map holds: 2^31 index slots, at most 3/4 of them
 *  full. */
#define MAP_MAX ((size_t)3 << 29)

/** @name Lookup
 *  Find an entry by its key.
 *  @{ */

/** Find @p key and store its index in `*i`.
 *  @return True when @p key is in the map. False leaves `*i` as it was. */
#define map_find(m, key, i)                                                    \
    map_find_((m)->data, sizeof *(m)->data, (m)->slots, (m)->slot_count,       \
              (key), (i))

/** Find @p key, or add it at the end of `data` when it is missing, and store
 *  its index in `*i`. A new entry is all zero except for its key, so a
 *  count starts at 0. An existing entry is left as it was.
 *  @return False when out of memory, or when the map already holds
 *  #MAP_MAX entries. The map is then unchanged.
 *  @warning It may move `data`, but not when @p key is already there. */
#define map_put(m, key, i)                                                     \
    map_put_(&(m)->data, &(m)->len, &(m)->capacity, (m)->alloc, &(m)->slots,   \
             &(m)->slot_count, sizeof *(m)->data, (key), (i))

/** @} */

/** @name Capacity
 *  Make room ahead of time, empty the map, and free it.
 *  @{ */

/** Make room for @p n more entries, so the next @p n calls to map_put()
 *  do not allocate.
 *  @return False when out of memory or the size overflows. The map is then
 *  unchanged. */
#define map_reserve(m, n)                                                      \
    map_reserve_(&(m)->data, &(m)->capacity, (m)->len, (n), sizeof *(m)->data, \
                 (m)->alloc, &(m)->slots, &(m)->slot_count)

/** Remove every entry. Keeps the memory for reuse. */
#define map_clear(m)                                                           \
    (map_clear_((m)->slots, (m)->slot_count), (void)((m)->len = 0))

/** Free the entries and the index and reset to empty. Keeps `alloc`, and is
 *  safe to call twice. */
#define map_free(m)                                                            \
    (arr_free_(&(m)->data, (m)->capacity * sizeof *(m)->data, (m)->alloc),     \
     map_free_(&(m)->slots, &(m)->slot_count, (m)->alloc), (m)->len = 0,       \
     (void)((m)->capacity = 0))

/** @} */

/** @cond */
MULE_NODISCARD bool map_find_(const void *data, size_t elem,
                              const uint64_t *slots, size_t slot_count,
                              StrView key, size_t *i);
MULE_NODISCARD bool map_put_(void *data_field, size_t *len, size_t *capacity,
                             const Allocator *alloc, uint64_t **slots,
                             size_t *slot_count, size_t elem, StrView key,
                             size_t *i);
MULE_NODISCARD bool map_reserve_(void *data_field, size_t *capacity, size_t len,
                                 size_t add, size_t elem,
                                 const Allocator *alloc, uint64_t **slots,
                                 size_t *slot_count);
void map_clear_(uint64_t *slots, size_t slot_count);
void map_free_(uint64_t **slots, size_t *slot_count, const Allocator *alloc);
/** @endcond */

/** @} */
