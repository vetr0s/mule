#pragma once
/**
 * @defgroup mod_arr Arr
 * @brief A growable array of any element type.
 *
 * Arr(T) is a struct with the same layout and rules as a Str, for any
 * element type. The operations are macros, because only a macro can work
 * for every `T`.
 *
 * @snippet{trimleft} test_arr.c arr
 *
 *   - Name each array type once with a typedef: `typedef Arr(int) IntArr;`.
 *     Every spelling of `Arr(...)` is a distinct type, so use the name.
 *   - A zero-initialised array (`IntArr xs = { 0 };`) is valid, empty, and
 *     owns no memory.
 *   - The fields are `data`, `len`, `capacity` and `alloc`, where a null
 *     `alloc` means the libc heap. Index `data` directly to read and write
 *     elements, and pass it to qsort or memcpy.
 *   - Calls that may allocate return `bool`. On false the array is
 *     unchanged.
 *   - Growing may move `data`, so pointers into it do not survive arr_push(),
 *     arr_insert or arr_reserve.
 *
 * @warning The macros evaluate the array argument more than once. Pass a
 * variable or a plain pointer, never an expression with side effects.
 * @{
 */

#include "base/base.h"
#include "mem/mem.h"

#include <assert.h>
#include <string.h>

/** A growable array of @p T. Declare it with a typedef before use. */
#define Arr(T)                                                                 \
    struct {                                                                   \
        T *data;                                                               \
        size_t len;                                                            \
        size_t capacity;                                                       \
        const Allocator *alloc;                                                \
    }

/** @name Growing
 *  Add elements. Each call may move `data`.
 *  @{ */

/** Make room for @p n more elements without another allocation.
 *  @return False when out of memory or the size overflows. The array is
 *  then unchanged. */
#define arr_reserve(a, n)                                                      \
    arr_reserve_(&(a)->data, &(a)->capacity, (a)->len, (n), sizeof *(a)->data, \
                 (a)->alloc)

/** Append @p v. @p v is evaluated after any growth, so it may read the array
 *  itself, as in `arr_push(&xs, xs.data[0])`.
 *  @return False when out of memory. The array is then unchanged.
 *  @warning @p v must not read `len`. The macro increments it in the same
 *  expression, so `arr_push(&xs, xs.len)` is undefined. */
#define arr_push(a, v)                                                         \
    (arr_reserve((a), 1) ? ((a)->data[(a)->len++] = (v), true) : false)

/** Insert @p v before index @p i, shifting the rest up. @p i may equal len.
 *  @return False when out of memory. The array is then unchanged.
 *  @pre `i <= len`. An assert checks it. */
#define arr_insert(a, i, v)                                                    \
    (assert((i) <= (a)->len),                                                  \
     arr_reserve((a), 1) ? (memmove((a)->data + (i) + 1, (a)->data + (i),      \
                                    ((a)->len - (i)) * sizeof *(a)->data),     \
                            (a)->data[(i)] = (v), (a)->len++, true)            \
                         : false)

/** @} */

/** @name Shrinking
 *  Remove elements. None of these allocate or move `data`.
 *  @{ */

/** Remove the element at index @p i, shifting the rest down.
 *  @pre `i < len`. An assert checks it. */
#define arr_remove(a, i)                                                       \
    (assert((i) < (a)->len),                                                   \
     memmove((a)->data + (i), (a)->data + (i) + 1,                             \
             ((a)->len - (i) - 1) * sizeof *(a)->data),                        \
     (void)(a)->len--)

/** Remove and return the last element.
 *  @pre The array is not empty. An assert checks it. */
#define arr_pop(a) (assert((a)->len > 0), (a)->data[--(a)->len])

/** Set the length to zero. Keeps the allocation for reuse. */
#define arr_clear(a) ((void)((a)->len = 0))

/** Free the elements' memory and reset to empty. Keeps `alloc`, and is safe
 *  to call twice. */
#define arr_free(a)                                                            \
    (arr_free_(&(a)->data, (a)->capacity * sizeof *(a)->data, (a)->alloc),     \
     (a)->len = 0, (void)((a)->capacity = 0))

/** @} */

/** @cond */
MULE_NODISCARD bool arr_reserve_(void *data_field, size_t *capacity, size_t len,
                                 size_t add, size_t elem,
                                 const Allocator *alloc);
void arr_free_(void *data_field, size_t bytes, const Allocator *alloc);
/** @endcond */

/** @} */
