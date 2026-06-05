#include "serde_internal.h"

#include <stdlib.h>
#include <string.h>

/**
 * @file value.c
 * @brief The `ScValue` data model: constructors, ownership-transferring
 *        mutators, accessors and the recursive destructor.
 */


// Forward declarations indented to reflect the call hierarchy.
static ScValue *value_alloc(ScValueType type);
static char *dup_string(const char *text);
static bool ensure_array_capacity(ScValueArray *array, size_t needed);
static bool ensure_object_capacity(ScValueObject *object, size_t needed);
static size_t object_index_of(const ScValueObject *object, const char *key);


/** Starting slot count for the first array/object insertion. */
#define VALUE_MIN_CAPACITY 4


/* ── Constructors ──────────────────────────────────────────────────────── */

ScValue *sc_value_null(void) {
    return value_alloc(SC_VALUE_NULL);
}

ScValue *sc_value_bool(bool value) {
    ScValue *node = value_alloc(SC_VALUE_BOOL);
    if (node) {
        node->as.boolean = value;
    }
    return node;
}

ScValue *sc_value_int(int64_t value) {
    ScValue *node = value_alloc(SC_VALUE_INT);
    if (node) {
        node->as.integer = value;
    }
    return node;
}

ScValue *sc_value_float(double value) {
    ScValue *node = value_alloc(SC_VALUE_FLOAT);
    if (node) {
        node->as.number = value;
    }
    return node;
}

ScValue *sc_value_string(const char *value) {
    return sc_value_string_typed(SC_VALUE_STRING, value);
}

ScValue *sc_value_string_typed(ScValueType type, const char *text) {
    ScValue *node = value_alloc(type);
    if (!node) {
        return NULL;
    }
    node->as.string = dup_string(text);
    if (!node->as.string) {
        free(node);
        return NULL;
    }
    return node;
}

ScValue *sc_value_array(void) {
    return value_alloc(SC_VALUE_ARRAY);
}

ScValue *sc_value_object(void) {
    return value_alloc(SC_VALUE_OBJECT);
}


/* ── Inspection ────────────────────────────────────────────────────────── */

ScValueType sc_value_type(const ScValue *value) {
    return value ? value->type : SC_VALUE_NULL;
}

size_t sc_value_len(const ScValue *value) {
    if (!value) {
        return 0;
    }
    if (value->type == SC_VALUE_ARRAY) {
        return value->as.array.count;
    }
    if (value->type == SC_VALUE_OBJECT) {
        return value->as.object.count;
    }
    return 0;
}

bool sc_value_as_bool(const ScValue *value, bool *out) {
    if (!value || value->type != SC_VALUE_BOOL || !out) {
        return false;
    }
    *out = value->as.boolean;
    return true;
}

bool sc_value_as_int(const ScValue *value, int64_t *out) {
    if (!value || value->type != SC_VALUE_INT || !out) {
        return false;
    }
    *out = value->as.integer;
    return true;
}

bool sc_value_as_float(const ScValue *value, double *out) {
    if (!value || !out) {
        return false;
    }
    if (value->type == SC_VALUE_FLOAT) {
        *out = value->as.number;
        return true;
    }
    if (value->type == SC_VALUE_INT) {
        *out = (double)value->as.integer;
        return true;
    }
    return false;
}

const char *sc_value_as_string(const ScValue *value) {
    if (!value) {
        return NULL;
    }
    if (value->type == SC_VALUE_STRING || value->type == SC_VALUE_DATETIME) {
        return value->as.string;
    }
    return NULL;
}

ScValue *sc_value_at(const ScValue *value, size_t index) {
    if (!value || value->type != SC_VALUE_ARRAY) {
        return NULL;
    }
    if (index >= value->as.array.count) {
        return NULL;
    }
    return value->as.array.items[index];
}

ScValue *sc_value_get(const ScValue *value, const char *key) {
    if (!value || value->type != SC_VALUE_OBJECT || !key) {
        return NULL;
    }
    size_t index = object_index_of(&value->as.object, key);
    if (index == value->as.object.count) {
        return NULL;
    }
    return value->as.object.values[index];
}

const char *sc_value_key_at(const ScValue *value, size_t index) {
    if (!value || value->type != SC_VALUE_OBJECT) {
        return NULL;
    }
    if (index >= value->as.object.count) {
        return NULL;
    }
    return value->as.object.keys[index];
}


/* ── Mutation ──────────────────────────────────────────────────────────── */

bool sc_value_push(ScValue *array, ScValue *child) {
    if (!array || array->type != SC_VALUE_ARRAY || !child) {
        return false;
    }
    if (!ensure_array_capacity(&array->as.array, array->as.array.count + 1)) {
        return false;
    }
    array->as.array.items[array->as.array.count] = child;
    array->as.array.count++;
    return true;
}

bool sc_value_set(ScValue *object, const char *key, ScValue *child) {
    if (!object || object->type != SC_VALUE_OBJECT || !key || !child) {
        return false;
    }

    ScValueObject *members = &object->as.object;
    size_t existing = object_index_of(members, key);
    if (existing != members->count) {
        sc_value_free(members->values[existing]);
        members->values[existing] = child;
        return true;
    }

    if (!ensure_object_capacity(members, members->count + 1)) {
        return false;
    }
    char *key_copy = dup_string(key);
    if (!key_copy) {
        return false;
    }
    members->keys[members->count] = key_copy;
    members->values[members->count] = child;
    members->count++;
    return true;
}


/* ── Destructor ────────────────────────────────────────────────────────── */

void sc_value_free(ScValue *value) {
    if (!value) {
        return;
    }

    switch (value->type) {
        case SC_VALUE_STRING:
        case SC_VALUE_DATETIME:
            free(value->as.string);
            break;
        case SC_VALUE_ARRAY:
            for (size_t i = 0; i < value->as.array.count; i++) {
                sc_value_free(value->as.array.items[i]);
            }
            free(value->as.array.items);
            break;
        case SC_VALUE_OBJECT:
            for (size_t i = 0; i < value->as.object.count; i++) {
                free(value->as.object.keys[i]);
                sc_value_free(value->as.object.values[i]);
            }
            free(value->as.object.keys);
            free(value->as.object.values);
            break;
        case SC_VALUE_NULL:
        case SC_VALUE_BOOL:
        case SC_VALUE_INT:
        case SC_VALUE_FLOAT:
            break;
    }
    free(value);
}


/* ── Internal helpers ──────────────────────────────────────────────────── */

/** Allocates a zeroed node of the given type. */
static ScValue *value_alloc(ScValueType type) {
    ScValue *node = calloc(1, sizeof *node);
    if (node) {
        node->type = type;
    }
    return node;
}

/** Copies a string; treats `NULL` as `""`. */
static char *dup_string(const char *text) {
    if (!text) {
        text = "";
    }
    size_t size = strlen(text) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, text, size);
    }
    return copy;
}

/** Grows an array's pointer storage to hold at least `needed` items. */
static bool ensure_array_capacity(ScValueArray *array, size_t needed) {
    if (needed <= array->capacity) {
        return true;
    }
    size_t new_cap = array->capacity ? array->capacity : VALUE_MIN_CAPACITY;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            return false;
        }
        new_cap *= 2;
    }
    if (new_cap > SIZE_MAX / sizeof *array->items) {
        return false;
    }
    ScValue **grown = realloc(array->items, new_cap * sizeof *array->items);
    if (!grown) {
        return false;
    }
    array->items = grown;
    array->capacity = new_cap;
    return true;
}

/** Grows an object's parallel key/value storage to hold `needed` members. */
static bool ensure_object_capacity(ScValueObject *object, size_t needed) {
    if (needed <= object->capacity) {
        return true;
    }
    size_t new_cap = object->capacity ? object->capacity : VALUE_MIN_CAPACITY;
    while (new_cap < needed) {
        if (new_cap > SIZE_MAX / 2) {
            return false;
        }
        new_cap *= 2;
    }
    if (new_cap > SIZE_MAX / sizeof *object->values) {
        return false;
    }
    char **keys = realloc(object->keys, new_cap * sizeof *object->keys);
    if (!keys) {
        return false;
    }
    object->keys = keys;
    ScValue **values = realloc(
        object->values, new_cap * sizeof *object->values
    );
    if (!values) {
        return false;
    }
    object->values = values;
    object->capacity = new_cap;
    return true;
}

/** Returns the index of `key`, or `object->count` when absent. */
static size_t object_index_of(const ScValueObject *object, const char *key) {
    for (size_t i = 0; i < object->count; i++) {
        if (strcmp(object->keys[i], key) == 0) {
            return i;
        }
    }
    return object->count;
}
