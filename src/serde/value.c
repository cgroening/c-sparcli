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
static ScValue *object_get_n(
    const ScValue *object, const char *key, size_t key_len
);
static bool parse_index(const char *text, size_t len, size_t *out);


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
    if (existing != members->count && members->values) {
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

bool sc_value_remove(ScValue *object, const char *key) {
    if (!object || object->type != SC_VALUE_OBJECT || !key) {
        return false;
    }
    ScValueObject *members = &object->as.object;
    size_t index = object_index_of(members, key);
    if (index == members->count) {
        return false;
    }

    free(members->keys[index]);
    sc_value_free(members->values[index]);
    size_t tail = members->count - index - 1;
    if (tail > 0) {
        memmove(&members->keys[index], &members->keys[index + 1],
                tail * sizeof *members->keys);
        memmove(&members->values[index], &members->values[index + 1],
                tail * sizeof *members->values);
    }
    members->count--;
    return true;
}

bool sc_value_remove_at(ScValue *array, size_t index) {
    if (!array || array->type != SC_VALUE_ARRAY
        || index >= array->as.array.count) {
        return false;
    }
    ScValueArray *items = &array->as.array;
    sc_value_free(items->items[index]);
    size_t tail = items->count - index - 1;
    if (tail > 0) {
        memmove(&items->items[index], &items->items[index + 1],
                tail * sizeof *items->items);
    }
    items->count--;
    return true;
}


/* ── Convenience accessors ─────────────────────────────────────────────── */

bool sc_value_get_bool_or(
    const ScValue *object, const char *key, bool fallback
) {
    bool out = false;
    if (sc_value_as_bool(sc_value_get(object, key), &out)) {
        return out;
    }
    return fallback;
}

int64_t sc_value_get_int_or(
    const ScValue *object, const char *key, int64_t fallback
) {
    int64_t out = 0;
    if (sc_value_as_int(sc_value_get(object, key), &out)) {
        return out;
    }
    return fallback;
}

double sc_value_get_float_or(
    const ScValue *object, const char *key, double fallback
) {
    double out = 0.0;
    if (sc_value_as_float(sc_value_get(object, key), &out)) {
        return out;
    }
    return fallback;
}

const char *sc_value_get_string_or(
    const ScValue *object, const char *key, const char *fallback
) {
    const char *out = sc_value_as_string(sc_value_get(object, key));
    return out ? out : fallback;
}

ScValue *sc_value_path(const ScValue *root, const char *path) {
    if (!root || !path) {
        return NULL;
    }

    ScValue *node = (ScValue *)root; // borrowed navigation, like sc_value_get
    const char *cursor = path;
    while (*cursor != '\0') {
        const char *dot = strchr(cursor, '.');
        size_t seg_len = dot ? (size_t)(dot - cursor) : strlen(cursor);
        if (seg_len == 0) {
            return NULL; // empty segment (e.g. "a..b")
        }

        if (sc_value_type(node) == SC_VALUE_OBJECT) {
            node = object_get_n(node, cursor, seg_len);
        } else if (sc_value_type(node) == SC_VALUE_ARRAY) {
            size_t index = 0;
            if (!parse_index(cursor, seg_len, &index)) {
                return NULL;
            }
            node = sc_value_at(node, index);
        } else {
            return NULL; // cannot descend into a scalar
        }
        if (!node) {
            return NULL;
        }
        cursor = dot ? dot + 1 : cursor + seg_len;
    }
    return node;
}


/* ── Copying / merging ─────────────────────────────────────────────────── */

ScValue *sc_value_clone(const ScValue *value) {
    if (!value) {
        return NULL;
    }
    switch (value->type) {
        case SC_VALUE_NULL:
            return sc_value_null();
        case SC_VALUE_BOOL:
            return sc_value_bool(value->as.boolean);
        case SC_VALUE_INT:
            return sc_value_int(value->as.integer);
        case SC_VALUE_FLOAT:
            return sc_value_float(value->as.number);
        case SC_VALUE_STRING:
            return sc_value_string(value->as.string);
        case SC_VALUE_DATETIME:
            return sc_value_string_typed(SC_VALUE_DATETIME, value->as.string);
        case SC_VALUE_ARRAY: {
            ScValue *copy = sc_value_array();
            if (!copy) {
                return NULL;
            }
            for (size_t i = 0; i < value->as.array.count; i++) {
                ScValue *child = sc_value_clone(value->as.array.items[i]);
                if (!child || !sc_value_push(copy, child)) {
                    sc_value_free(child);
                    sc_value_free(copy);
                    return NULL;
                }
            }
            return copy;
        }
        case SC_VALUE_OBJECT: {
            ScValue *copy = sc_value_object();
            if (!copy) {
                return NULL;
            }
            for (size_t i = 0; i < value->as.object.count; i++) {
                ScValue *child = sc_value_clone(value->as.object.values[i]);
                if (!child
                    || !sc_value_set(copy, value->as.object.keys[i], child)) {
                    sc_value_free(child);
                    sc_value_free(copy);
                    return NULL;
                }
            }
            return copy;
        }
    }
    return NULL;
}

bool sc_value_merge(ScValue *base, const ScValue *overlay) {
    if (!base || !overlay || base->type != SC_VALUE_OBJECT
        || overlay->type != SC_VALUE_OBJECT) {
        return false;
    }

    for (size_t i = 0; i < overlay->as.object.count; i++) {
        const char *key = overlay->as.object.keys[i];
        const ScValue *incoming = overlay->as.object.values[i];
        ScValue *existing = sc_value_get(base, key);

        if (existing && sc_value_type(existing) == SC_VALUE_OBJECT
            && sc_value_type(incoming) == SC_VALUE_OBJECT) {
            if (!sc_value_merge(existing, incoming)) {
                return false;
            }
            continue;
        }

        ScValue *copy = sc_value_clone(incoming);
        if (!copy || !sc_value_set(base, key, copy)) {
            sc_value_free(copy);
            return false;
        }
    }
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
    size_t old_cap = array->capacity;
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
    // Zero the freshly grown slots so they are defined (NULL) until written.
    memset(&grown[old_cap], 0, (new_cap - old_cap) * sizeof *grown);
    array->items = grown;
    array->capacity = new_cap;
    return true;
}

/** Grows an object's parallel key/value storage to hold `needed` members. */
static bool ensure_object_capacity(ScValueObject *object, size_t needed) {
    if (needed <= object->capacity) {
        return true;
    }
    size_t old_cap = object->capacity;
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
    memset(&keys[old_cap], 0, (new_cap - old_cap) * sizeof *keys);
    object->keys = keys;
    ScValue **values = realloc(
        object->values, new_cap * sizeof *object->values
    );
    if (!values) {
        return false;
    }
    // Zero the freshly grown slots so they are defined (NULL) until written.
    memset(&values[old_cap], 0, (new_cap - old_cap) * sizeof *values);
    object->values = values;
    object->capacity = new_cap;
    return true;
}

/** Returns the index of `key`, or `object->count` when absent. */
static size_t object_index_of(const ScValueObject *object, const char *key) {
    for (size_t i = 0; i < object->count; i++) {
        if (object->keys[i] && strcmp(object->keys[i], key) == 0) {
            return i;
        }
    }
    return object->count;
}

/** Returns the object member whose key matches the `key_len`-byte span, or
 *  `NULL`. Used by `sc_value_path` (the key is not NUL-terminated). */
static ScValue *object_get_n(
    const ScValue *object, const char *key, size_t key_len
) {
    const ScValueObject *members = &object->as.object;
    for (size_t i = 0; i < members->count; i++) {
        if (members->keys[i] && strlen(members->keys[i]) == key_len
            && memcmp(members->keys[i], key, key_len) == 0) {
            return members->values[i];
        }
    }
    return NULL;
}

/** Parses a `len`-byte decimal array index; rejects non-digits and overflow. */
static bool parse_index(const char *text, size_t len, size_t *out) {
    size_t value = 0;
    for (size_t i = 0; i < len; i++) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
        size_t digit = (size_t)(text[i] - '0');
        if (value > (SIZE_MAX - digit) / 10) {
            return false; // overflow
        }
        value = value * 10 + digit;
    }
    *out = value;
    return true;
}
