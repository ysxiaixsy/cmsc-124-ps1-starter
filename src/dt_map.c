/*
 * dt_map.c: Associative arrays for Unit 5, Section E.
 *
 * An array does not store its indices. This map stores its keys.
 * An array calculates a position with one subtraction.
 * The map calculates a hash and then compares keys in one bucket.
 *
 * Hashing turns the key into a bucket number. Compare all keys in that bucket
 * because two keys can select it. Use a linked list for each bucket. Start the
 * unsigned accumulator at 14695981039346656037ULL. For each unsigned byte,
 * exclusive-or the byte into it and multiply by 1099511628211ULL.
 *
 * A separate list stores insertion order for stable output. dt_map_key_at
 * reads this list. Updating a key preserves its position. Removing and
 * reinserting a key moves it to the end.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "dt.h"

// map has a fixed number of buckets, since i was playing minecraft, it will be arbitrarly 64.
#define DT_MAP_BUCKETS 64

static uint64_t dt_map_hash(const char *key)
{
    uint64_t hash = 14695981039346656037ULL;
    for (const unsigned char *p = (const unsigned char *)key; *p != '\0'; p++) {
        hash ^= (uint64_t)(*p);
        hash *= 1099511628211ULL;
    }
    return hash;
}

// map entry is a node in a bucket chain and an insertion-order list, remember 123
struct dt_map_entry {
    // int placeholder; /* TODO: Add the key, value, and next pointer. */
    char *key;
    dt_value value;
    struct dt_map_entry *next;
};

struct dt_map {
    struct dt_map_entry *   buckets[DT_MAP_BUCKETS];
    struct dt_map_entry *   *order;
    size_t                  count;
    size_t                  order_cap;
};

/*
 * dt_map_new builds an empty map. It returns NULL after an allocation failure.
 */
dt_map *dt_map_new(void)
{
    /* TODO: Return an allocated empty map. Return NULL after an allocation failure.
       dt_map_new()  -> a map whose dt_map_len is 0
       cases/normal/map_basics.case */
    
    // calloc > malloc because it initializes the memory to zero, which is what we want for an empty map.
    dt_map *m = calloc(1, sizeof(*m));
    /* count = 0, order_cap = 0, order = NULL, all 64 buckets = NULL */
    return m;
}

/*
 * dt_map_free releases each entry, copied key, order array, and map.
 * It accepts NULL. The environment owns the values.
 */
void dt_map_free(dt_map *m)
{
    /* TODO: Release each entry, copied key, order array, and map.
       Preserve the values. The environment owns them.
       a map holding a string value  -> the nodes and keys go, the string stays
       dt_map_free(NULL)             -> returns, having done nothing
       cases/cleanup/map_churn.case */
    
    // if the map is NULL, just return, nothing to free
    if (m == NULL) {
        return;
    }

    // free each bucket's linked list and the copied keys
    for (size_t i = 0; i < DT_MAP_BUCKETS; i++) {       
        struct dt_map_entry *entry = m->buckets[i];
        while (entry != NULL) {                         
            struct dt_map_entry *next = entry->next;    
            free(entry->key);                           // free the copied key
            free(entry);                                // free the entry itself
            entry = next;                               
        }
    }
    free(m->order);     // free the order array
    free(m);            // free the map itself
    
}

/*
 * dt_map_len returns the number of keys in constant time.
 */
size_t dt_map_len(const dt_map *m)
{
    /* TODO: Return the current key count.
       Replacing a value does not change this count.
       after put alpha, beta, gamma:  dt_map_len(m) -> 3
       after put beta again:          dt_map_len(m) -> 3, still
       after del alpha:               dt_map_len(m) -> 2
       cases/normal/map_basics.case */
    if (m == NULL) {
        return 0;
    }
    return m->count;
}

/*
 * dt_map_put binds v to key.
 * An existing key keeps its insertion position. A new key becomes the last key.
 * Copy each new key because the caller owns the source buffer.
 * Return DT_ERR_CAPACITY after an allocation failure.
 */
dt_status dt_map_put(dt_map *m, const char *key, dt_value v)
{
    /* TODO: Replace the value for an existing key.
       Add a new entry for a new key. Copy each new key.
       Hash the key. Select its bucket. Search the bucket chain.
       Add a new entry to the chain and insertion list.
       put "beta" -> 2 on an empty map    -> DT_OK, "beta" is last in order
       put "beta" -> 22 on that map       -> DT_OK, same position, new value
       an allocation failure              -> DT_ERR_CAPACITY, map unchanged
       cases/normal/map_basics.case */
    
    // if the map is NULL, return DT_ERR_CAPACITY
    if (m == NULL) {
        return DT_ERR_CAPACITY;
    }

    // find the bucket, remember 123
    size_t bucket_index = dt_map_hash(key) % DT_MAP_BUCKETS;
    struct dt_map_entry *entry = m->buckets[bucket_index];

    // walk the bucket chain to see if the key already exists
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            // key exists, replace the value
            entry->value = v;
            return DT_OK;
        }
        entry = entry->next;
    }

    // key not found, add a new entry
    struct dt_map_entry *new_entry = malloc(sizeof(*new_entry));
    if (new_entry == NULL) {
        return DT_ERR_CAPACITY; // allocation failure
    }
    new_entry->key = malloc(strlen(key) + 1);
    if (new_entry->key == NULL) {
        free(new_entry);
        return DT_ERR_CAPACITY; // allocation failure
    }
    strcpy(new_entry->key, key);
    new_entry->value = v;
    new_entry->next = NULL;

    // make room in the order array BEFORE touching the map,
    // so an allocation failure leaves the map unchanged
    if (m->count == m->order_cap) {
        // refuse a capacity whose byte size would overflow size_t
        if (m->order_cap > SIZE_MAX / 2 / sizeof(*m->order)) {
            free(new_entry->key);
            free(new_entry);
            return DT_ERR_CAPACITY;
        }
        size_t new_cap = (m->order_cap == 0) ? 4 : m->order_cap * 2;       // start w 4, double each time
        struct dt_map_entry **new_order = realloc(m->order, new_cap * sizeof(*new_order));
        if (new_order == NULL) {
            free(new_entry->key);
            free(new_entry);
            return DT_ERR_CAPACITY; // allocation failure, map unchanged
        }
        m->order = new_order;
        m->order_cap = new_cap;
    }

    // link at the head of the bucket chain; the old chain stays behind it
    new_entry->next = m->buckets[bucket_index];
    m->buckets[bucket_index] = new_entry;

    // record arrival order; a new key becomes the last key
    m->order[m->count] = new_entry;
    m->count++;

    return DT_OK;
}

/*
 * dt_map_get writes the value for key to *out.
 * It returns DT_ERR_KEY and does not change *out when the key is absent.
 * An absent key differs from a nil value.
 */
dt_status dt_map_get(const dt_map *m, const char *key, dt_value *out)
{
    /* TODO: Return DT_ERR_KEY when the key is absent.
       Preserve *out after this error. A nil value can be present.
       after put "beta" -> 22:
         dt_map_get(m, "beta", &out)   -> DT_OK, *out is the integer 22
         dt_map_get(m, "ghost", &out)  -> DT_ERR_KEY, *out untouched
       cases/normal/map_basics.case, cases/boundary/map_missing_key.case */
    
    if (m == NULL || key == NULL || out == NULL) {
        return DT_ERR_KEY; 
    }

    // find the bucket, remember 123
    size_t bucket_index = dt_map_hash(key) % DT_MAP_BUCKETS;
    struct dt_map_entry *entry = m->buckets[bucket_index];

    // walk the bucket chain to find the key
    while (entry != NULL) {
        if (strcmp(entry->key, key) == 0) {
            *out = entry->value; // key found, write the value to *out
            return DT_OK;
        } else {
            entry = entry->next; // move to the next entry in the chain
        }
    }
    return DT_ERR_KEY; // key not found
}

/*
 * dt_map_remove removes key from its bucket and insertion position.
 * It releases the copied key. It returns DT_ERR_KEY when the key is absent.
 */
dt_status dt_map_remove(dt_map *m, const char *key)
{
    /* TODO: Remove the entry from its bucket and insertion position.
       Release the copied key. Return DT_ERR_KEY when the key is absent.
       a map holding alpha, beta, gamma:
         dt_map_remove(m, "alpha")  -> DT_OK, order is now beta, gamma
         dt_map_remove(m, "ghost")  -> DT_ERR_KEY, nothing changes
       reinserting "alpha" appends it after "gamma"
       cases/normal/map_basics.case, cases/boundary/map_remove_missing_key.case */
    
    if (m == NULL || key == NULL) {
        return DT_ERR_KEY; // map or key is NULL, treat as absent
    }

    // FIND the entry in its bucket, remembering the entry before it
    // nothing is changed yet, so a missing key leaves the map untouched
    size_t bucket_index = dt_map_hash(key) % DT_MAP_BUCKETS;
    struct dt_map_entry *entry = m->buckets[bucket_index];
    struct dt_map_entry *prev = NULL;
    while (entry != NULL && strcmp(entry->key, key) != 0) {
        prev = entry;
        entry = entry->next;
    }
    if (entry == NULL) {
        return DT_ERR_KEY; // key not found, nothing changed
    }

    // remove its address from the order array
    // compare addresses (no strcmp needed), then shift later keys one slot left
    for (size_t i = 0; i < m->count; i++) {
        if (m->order[i] == entry) {
            memmove(&m->order[i], &m->order[i + 1],
                    (m->count - i - 1) * sizeof(*m->order));
            break;
        }
    }
    m->count--;

    // unlink it from the bucket chain
    if (prev == NULL) {
        m->buckets[bucket_index] = entry->next; // entry was the chain head
    } else {
        prev->next = entry->next;               // bypass the entry
    }

    // release the copied key first, then the entry itself
    free(entry->key);
    free(entry);
    return DT_OK;
}

/*
 * dt_map_key_at writes the key at insertion position index to *out.
 * It returns DT_ERR_RANGE and does not change *out for an invalid index.
 */
dt_status dt_map_key_at(const dt_map *m, size_t index, const char **out)
{
    /* TODO: Write the key at the specified insertion position to *out.
       Return DT_ERR_RANGE for an invalid position. Preserve *out after this error.
       The printer uses this order.
       a map holding alpha, beta, gamma:
         dt_map_key_at(m, 0, &out)  -> DT_OK, *out = "alpha"
         dt_map_key_at(m, 3, &out)  -> DT_ERR_RANGE, *out untouched
       cases/normal/map_basics.case */
    if (m == NULL || index >= m->count) {     
        return DT_ERR_RANGE; // invalid index or map is NULL
    }
    *out = m->order[index]->key; // write the key at the specified index to *out
    return DT_OK;
}
