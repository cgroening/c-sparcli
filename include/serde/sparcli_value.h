#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_value.h
 * @brief `ScValue` - the shared in-memory data model for the structured
 *        formats (JSON, TOML, YAML).
 *
 * A recursive tree of scalars, arrays and objects. All three tree formats
 * parse into it and serialize from it, so converting between them is a
 * parse-then-write round trip with no format-specific intermediate model.
 *
 * Ownership is single-rooted: a container owns its children, and one
 * `sc_value_free` on the root releases the whole tree. The mutators
 * `sc_value_push` / `sc_value_set` take ownership of the child they are
 * handed (no deep copy).
 */


/** Discriminator for the value held by an `ScValue`. */
typedef enum ScValueType {
    SC_VALUE_NULL,     /**< JSON `null` / absent value. */
    SC_VALUE_BOOL,     /**< Boolean. */
    SC_VALUE_INT,      /**< 64-bit signed integer. */
    SC_VALUE_FLOAT,    /**< Double-precision floating point. */
    SC_VALUE_STRING,   /**< UTF-8 string (stored raw, not sanitized). */
    SC_VALUE_ARRAY,    /**< Ordered list of child values. */
    SC_VALUE_OBJECT,   /**< Insertion-ordered key/value map. */
    SC_VALUE_DATETIME, /**< Date/time held as its source string (TOML/YAML). */
} ScValueType;

/** Opaque value node; build with the `sc_value_*` constructors. */
typedef struct ScValue ScValue;


/* ── Constructors (each returns a heap node, or NULL on allocation failure) ─ */

/** Allocates a `null` value. */
SPARCLI_EXPORT ScValue *sc_value_null(void);

/** Allocates a boolean value. */
SPARCLI_EXPORT ScValue *sc_value_bool(bool value);

/** Allocates a 64-bit integer value. */
SPARCLI_EXPORT ScValue *sc_value_int(int64_t value);

/** Allocates a floating-point value. */
SPARCLI_EXPORT ScValue *sc_value_float(double value);

/**
 * Allocates a string value; the bytes are copied.
 *
 * @param value  UTF-8 string; copied. `NULL` is treated as `""`.
 * @return       Heap node, or `NULL` on allocation failure.
 */
SPARCLI_EXPORT ScValue *sc_value_string(const char *value);

/** Allocates an empty array. */
SPARCLI_EXPORT ScValue *sc_value_array(void);

/** Allocates an empty object. */
SPARCLI_EXPORT ScValue *sc_value_object(void);


/* ── Inspection ────────────────────────────────────────────────────────── */

/** Returns the discriminator of `value` (`SC_VALUE_NULL` when `value` is
 *  `NULL`). */
SPARCLI_EXPORT ScValueType sc_value_type(const ScValue *value);

/**
 * Returns the element count of an array or object.
 *
 * @param value  Value to inspect.
 * @return       Number of children for arrays/objects; `0` otherwise.
 */
SPARCLI_EXPORT size_t sc_value_len(const ScValue *value);

/**
 * Reads a boolean value.
 *
 * @param value  Value to read.
 * @param out    Receives the boolean when the type matches.
 * @return       `true` when `value` is a boolean (and `out` is written),
 *               `false` otherwise.
 */
SPARCLI_EXPORT bool sc_value_as_bool(const ScValue *value, bool *out);

/**
 * Reads an integer value.
 *
 * @param value  Value to read.
 * @param out    Receives the integer when the type matches.
 * @return       `true` when `value` is an integer, `false` otherwise.
 */
SPARCLI_EXPORT bool sc_value_as_int(const ScValue *value, int64_t *out);

/**
 * Reads a numeric value as a double; integers are promoted.
 *
 * @param value  Value to read.
 * @param out    Receives the number when `value` is an int or float.
 * @return       `true` when `value` is numeric, `false` otherwise.
 */
SPARCLI_EXPORT bool sc_value_as_float(const ScValue *value, double *out);

/**
 * Returns the bytes of a string or datetime value.
 *
 * @param value  Value to read.
 * @return       Borrowed, NUL-terminated bytes for `SC_VALUE_STRING` /
 *               `SC_VALUE_DATETIME`; `NULL` for any other type.
 */
SPARCLI_EXPORT const char *sc_value_as_string(const ScValue *value);

/**
 * Returns the array element at `index`.
 *
 * @param value  Array value.
 * @param index  Zero-based element index.
 * @return       Borrowed child, or `NULL` when `value` is not an array or
 *               `index` is out of range.
 */
SPARCLI_EXPORT ScValue *sc_value_at(const ScValue *value, size_t index);

/**
 * Returns the object member stored under `key`.
 *
 * @param value  Object value.
 * @param key    Member key.
 * @return       Borrowed child, or `NULL` when `value` is not an object or
 *               the key is absent.
 */
SPARCLI_EXPORT ScValue *sc_value_get(const ScValue *value, const char *key);

/**
 * Returns the key of the object member at insertion position `index`.
 *
 * @param value  Object value.
 * @param index  Zero-based member position.
 * @return       Borrowed key, or `NULL` when out of range / not an object.
 */
SPARCLI_EXPORT const char *sc_value_key_at(
    const ScValue *value, size_t index
);


/* ── Mutation (ownership of `child` transfers to the container) ─────────── */

/**
 * Appends `child` to an array.
 *
 * @param array  Array value.
 * @param child  Child to append; owned by `array` on success.
 * @return       `true` on success; `false` (ownership stays with the caller)
 *               when `array` is not an array, `child` is `NULL`, or
 *               allocation fails.
 */
SPARCLI_EXPORT bool sc_value_push(ScValue *array, ScValue *child);

/**
 * Sets object member `key` to `child`; an existing member with the same key
 * is replaced (its previous value is freed) and keeps its position.
 *
 * @param object  Object value.
 * @param key     Member key; copied.
 * @param child   Child to store; owned by `object` on success.
 * @return        `true` on success; `false` (ownership stays with the caller)
 *                when `object` is not an object, an argument is `NULL`, or
 *                allocation fails.
 */
SPARCLI_EXPORT bool sc_value_set(
    ScValue *object, const char *key, ScValue *child
);


/**
 * Recursively frees `value` and every child it owns.
 *
 * @param value  Root to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_value_free(ScValue *value);

SPARCLI_END_DECLS
