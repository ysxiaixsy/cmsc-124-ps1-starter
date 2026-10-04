/*
 * dt_record.c: Records for Unit 5, Section F.
 *
 * A record selects fields by name. A compiled language can replace a field
 * access with a fixed offset. That access requires no run-time search.
 *
 * This implementation keeps a field-name array.
 * A lookup searches the array and returns an index.
 *
 * An undeclared field returns DT_ERR_FIELD. Records cannot add fields after
 * construction. An associative array can add keys.
 */

#include "dt.h"

#include <stdlib.h>
#include <string.h>

struct dt_record {
    char    *names[DT_RECORD_MAX_FIELDS];
    dt_value values[DT_RECORD_MAX_FIELDS];
    size_t   count;
};

/*
 * dt_record_new builds a record with the specified fields in declaration order.
 * It sets each field to nil and copies each field name.
 * It returns NULL for too many fields or an allocation failure.
 */
dt_record *dt_record_new(const char **field_names, size_t field_count)
{
    /* TODO: Return NULL for more than DT_RECORD_MAX_FIELDS.
       Copy each field name. Set each field to dt_value_nil().
       fields {"name", "age"}       -> a record with two nil fields, in that order
       eight fields                 -> fine, DT_RECORD_MAX_FIELDS is 8
       nine fields                  -> NULL, and the driver reports DT_ERR_CAPACITY
       cases/normal/record_basics.case, cases/capacity/record_max_fields.case,
       cases/capacity/record_over_fields.case */

    // check if field_count exceeds DT_RECORD_MAX_FIELDS
    if (field_count > DT_RECORD_MAX_FIELDS) {
        return NULL; // too many fields
    }

    // allocate memory for the record
    dt_record *r = malloc(sizeof(*r));

    // if allocation fails, return NULL
    if (r == NULL) {
        return NULL; // allocation failure
    }

    // copy each field name and initialize each value to dt_value_nil()
    size_t i;

    for (i = 0; i < field_count; i++) {
        // allocate memory for the field name
        r->names[i] = malloc(strlen(field_names[i]) + 1); // +1 for /0

        // if allocation fails, free previously allocated names and the record
        if (r->names[i] == NULL) {
            for (size_t j = 0; j < i; j++) {
                free(r->names[j]);
            }
            free(r);
            return NULL; // allocation failure
        }

        // otherwise, copy the field name
        strcpy(r->names[i], field_names[i]);

        // initialize the value to dt_value_nil()
        r->values[i] = dt_value_nil();
    }

    // store the field count
    r->count = field_count;

    return r;
}

/*
 * dt_record_free releases the copied field names and the record.
 * It accepts NULL. The environment owns the field values.
 */
void dt_record_free(dt_record *r)
{
    /* TODO: Release the copied field names. Then release the record.
       a record holding a string value  -> the names go, the string stays
       dt_record_free(NULL)             -> returns, having done nothing */

    // if r is NULL, do nothing
    if (r == NULL) {
        return;
    }

    // for each field name, free the allocated memory
    for (size_t i = 0; i < r->count; i++) {
        free(r->names[i]);
    }
    // free the record itself
    free(r);
}

/*
 * dt_record_field_count returns the stored field count in constant time.
 */
size_t dt_record_field_count(const dt_record *r)
{
    /* TODO: Return the field count that the constructor stored.
       The count does not change after construction.
       after `rec new person name age`:  dt_record_field_count(person) -> 2
       cases/normal/record_basics.case */
    
    // if r is not NULL, return the stored count
    if (r != NULL) {
        return r->count;
    }
    
    return 0;
}

/*
 * dt_record_field_name writes the field name at declaration position index to *out.
 * It returns DT_ERR_RANGE and does not change *out for an invalid index.
 * The record prints fields in declaration order.
 */
dt_status dt_record_field_name(const dt_record *r, size_t index, const char **out)
{
    /* TODO: Return field names in declaration order.
       Return DT_ERR_RANGE for an invalid position. Preserve *out after this error.
       after `rec new person name age`:
         dt_record_field_name(person, 0, &out)  -> DT_OK, *out = "name"
         dt_record_field_name(person, 2, &out)  -> DT_ERR_RANGE, *out untouched
       cases/normal/record_basics.case */
    
    // check index is out of bounds
    // not required to check for NULL but it is good practice to do so
    if (r == NULL || index >= r->count) {
        return DT_ERR_RANGE; // invalid index
    }

    // write the field name at index to *out
    *out = r->names[index];

    return DT_OK;
}

/*
 * dt_record_get writes the value of field to *out.
 * It returns DT_ERR_FIELD and does not change *out when the field is absent.
 */
dt_status dt_record_get(const dt_record *r, const char *field, dt_value *out)
{
    /* TODO: Find the index for field. Return DT_ERR_FIELD when it is absent.
       after `rec set person age 36`:
         dt_record_get(person, "age", &out)      -> DT_OK, *out is the integer 36
         dt_record_get(person, "salary", &out)   -> DT_ERR_FIELD, *out untouched
       cases/normal/record_basics.case, cases/boundary/record_unknown_field.case */

    size_t i;

    // search for the field name in the record's names array
    for (i = 0; i < r->count; i++) {
        // compare current field name with the passed field name
        if (strcmp(r->names[i], field) == 0) {
            // if found, point *out to the corresponding value 
            *out = r->values[i];
            return DT_OK;
        }
    }
    // if not found, return DT_ERR_FIELD
    return DT_ERR_FIELD;
}

/*
 * dt_record_set replaces the value of field with v.
 * It returns DT_ERR_FIELD and changes nothing when the field is absent.
 * A record cannot gain fields after construction.
 */
dt_status dt_record_set(dt_record *r, const char *field, dt_value v)
{
    /* TODO: Use the same lookup and then write the value. Never add a field.
       after `rec new person name age`:
         dt_record_set(person, "age", dt_value_int(36))     -> DT_OK
         dt_record_set(person, "salary", dt_value_int(1))   -> DT_ERR_FIELD
         the record still has only the fields "name" and "age"
       cases/normal/record_basics.case, cases/boundary/record_unknown_field.case */
    (void)r;
    (void)field;
    (void)v;
    return DT_ERR_FIELD;
}
