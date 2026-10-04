/*
 * dt_array.c: Array descriptors for Unit 5, Section D.
 *
 * Built-in C arrays use offsets that start at zero. Ada, Fortran, and Pascal
 * can use bounds such as 1..10 or -5..5. The index and offset then differ.
 *
 *     offset = index - lower_bound
 *
 * The run-time descriptor stores the lower bound for this subtraction.
 * The mathematical difference can exceed long long.
 * Confirm that the result is representable before you subtract signed values.
 *
 * Check both bounds. An index below the lower bound can access memory before
 * the allocation. That access has undefined behavior.
 */

#include "dt.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>

struct dt_array {
    dt_value *elements;
    size_t    length;
    long long lower_bound;
};

/*
 * dt_array_new builds an array of length nil elements.
 * The first index is lower_bound. A zero length creates a valid empty array.
 * It returns NULL for an invalid size, invalid index range, or allocation failure.
 */
dt_array *dt_array_new(size_t length, long long lower_bound)
{
    /* TODO: Allocate the descriptor and length elements.
       Set each element to dt_value_nil(). Store the lower bound.
       Return a valid array for a zero length.
       Reject a nonempty range with an unrepresentable final index.
       Reject an element block size that exceeds SIZE_MAX.
       dt_array_new(3, 0)   -> three nil elements, indices 0, 1, 2
       dt_array_new(3, -1)  -> three nil elements, indices -1, 0, 1
       dt_array_new(0, 0)   -> an empty array
       cases/normal/array_basics.case, cases/boundary/array_empty.case,
       cases/boundary/array_negative_lower_bound.case */

    if (length > 0) {
        
        // example: idx: -1, 0, 1    offset: 0, 1, 2
        // final index = lower_bound + last_offset
        // make sure last_offset can be represented as long long
        size_t last_offset = length - 1;

        // check if last_offset can represent a long long
        if (last_offset > (size_t)LLONG_MAX) {
            return NULL; // final index is unrepresentable
        }

        // check if lower_bound + last_offset > LLONG_MAX
        // => lower_bound > LLONG_MAX - last_offset
        if (lower_bound > LLONG_MAX - (long long)(last_offset)) {
            return NULL; // final index is unrepresentable
        }
    }

    // total_bytes = length * sizeof(dt_value)
    // SIZE_MAX is the largest value that size_t can represent.
    // To prevent multiplication overflow: 
    //      total_bytes <= SIZE_MAX
    //      length * sizeof(dt_value) <= SIZE_MAX
    //      Hence, length <= SIZE_MAX / sizeof(dt_value)
    if (length > SIZE_MAX / sizeof(dt_value)) {
        return NULL;
    }

    // allocate the array descriptor
    dt_array *a = malloc(sizeof(*a));

    // if allocation fails, return NULL
    if (a == NULL) {
        return NULL;
    }

    // allocate and initialize the elements if the array is non-empty
    if (length > 0) {
        // allocate the elements
        a->elements = malloc(length * sizeof(dt_value));

        // if allocation fails, free the descriptor and return NULL
        if (a->elements == NULL) {
            free(a);
            return NULL;
        }

        // initialize each element to dt_value_nil()
        for (size_t i = 0; i < length; i++) {
            a->elements[i] = dt_value_nil();
        }
    }  else {
        // for an empty array, set elements to NULL
        a->elements = NULL;
    }

    // store the length and lower bound
    a->length = length;
    a->lower_bound = lower_bound;

    return a;
}

/*
 * dt_array_free releases the element block and descriptor. It accepts NULL.
 * The environment owns the runtime objects referenced by the dt_value elements.
 */
void dt_array_free(dt_array *a)
{
    /* TODO: Release the elements. Then release the descriptor.
       Preserve the referenced values. The driver environment owns them.
       an array holding a string  -> the element block goes, the string stays
       dt_array_free(NULL)        -> returns, having done nothing */
    (void)a;
}

/*
 * dt_array_len returns the stored element count in constant time.
 */
size_t dt_array_len(const dt_array *a)
{
    /* TODO: Return the stored length. The lower bound does not affect it.
       after `arr new a 3 0`:   dt_array_len(a) -> 3
       after `arr new a 3 -1`:  dt_array_len(a) -> 3, the same three elements
       after `arr new a 0 0`:   dt_array_len(a) -> 0
       cases/normal/array_basics.case, cases/boundary/array_empty.case */
    (void)a;
    return 0;
}

/*
 * dt_array_lower_bound returns the first array index. With lower bound 1,
 * index 1 uses storage offset 0.
 */
long long dt_array_lower_bound(const dt_array *a)
{
    /* TODO: Return the lower bound that the constructor stored.
       dt_array_get uses this value to calculate an element offset.
       after `arr new a 3 -1`:  dt_array_lower_bound(a) -> -1
       after `arr new a 3 1`:   dt_array_lower_bound(a) -> 1
       cases/boundary/array_negative_lower_bound.case,
       cases/boundary/array_lower_bound_one.case */
    (void)a;
    return 0;
}

/*
 * dt_array_get writes the element at index to *out.
 * It returns DT_ERR_RANGE and does not change *out for an invalid index.
 */
dt_status dt_array_get(const dt_array *a, long long index, dt_value *out)
{
    /* TODO: Reject an index below the lower bound.
       Calculate the nonnegative distance without signed overflow.
       Reject a distance that is at least the length.
       Convert the checked distance to size_t for the element offset.
       an array over -1..1:
         dt_array_get(a, -1, &out)  -> DT_OK, offset 0
         dt_array_get(a,  1, &out)  -> DT_OK, offset 2
         dt_array_get(a,  2, &out)  -> DT_ERR_RANGE, *out untouched
         dt_array_get(a, -2, &out)  -> DT_ERR_RANGE, below the lower bound
       cases/boundary/array_index_above_upper.case,
       cases/boundary/array_index_below_lower.case,
       cases/boundary/array_full_range_index.case */
    (void)a;
    (void)index;
    (void)out;
    return DT_ERR_RANGE;
}

/*
 * dt_array_set replaces the element at index with v.
 * It returns DT_ERR_RANGE and changes nothing for an invalid index.
 * The environment keeps ownership of the old value.
 */
dt_status dt_array_set(dt_array *a, long long index, dt_value v)
{
    /* TODO: Use the same bounds check as dt_array_get. Then write the value.
       Put the shared check in one helper.
       an array over -1..1:
         dt_array_set(a, -1, dt_value_int(10))  -> DT_OK, offset 0 holds 10
         dt_array_set(a,  2, dt_value_int(10))  -> DT_ERR_RANGE, nothing changes
       cases/normal/array_basics.case, cases/boundary/array_negative_lower_bound.case */
    (void)a;
    (void)index;
    (void)v;
    return DT_ERR_RANGE;
}
