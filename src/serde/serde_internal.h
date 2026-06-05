#pragma once

#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @file serde_internal.h
 * @brief Private declarations shared across the serde layer: the concrete
 *        `ScValue` representation and the growable output buffer used by the
 *        serializers. Not part of the public API.
 */


/* ── Growable byte buffer (backs every format writer) ──────────────────── */

/**
 * Append-only byte buffer with a sticky failure flag: callers append freely
 * and check the result once via `sc_serde_buf_finish`, instead of testing
 * every individual append.
 */
typedef struct ScSerdeBuf {
    char  *data;   /**< Owned storage; `NULL` until the first append. */
    size_t len;    /**< Bytes currently used (excluding the NUL). */
    size_t cap;    /**< Allocated capacity. */
    bool   failed; /**< Set once an allocation fails; never cleared. */
} ScSerdeBuf;

/** Appends `len` raw bytes; sets `failed` on allocation error. */
bool sc_serde_buf_append(ScSerdeBuf *buf, const char *bytes, size_t len);

/** Appends a NUL-terminated string (no-op for `NULL`). */
bool sc_serde_buf_append_str(ScSerdeBuf *buf, const char *text);

/** Appends a single byte. */
bool sc_serde_buf_append_char(ScSerdeBuf *buf, char byte);

/**
 * Finalizes the buffer into a heap, NUL-terminated string and hands over
 * ownership; the buffer is reset to empty. Returns `NULL` (and frees any
 * storage) if an earlier append failed.
 */
char *sc_serde_buf_finish(ScSerdeBuf *buf);

/** Releases the buffer's storage and resets it to empty. */
void sc_serde_buf_free(ScSerdeBuf *buf);

/** Encodes one Unicode code point as UTF-8 and appends it to `buf`. */
bool sc_serde_append_utf8(ScSerdeBuf *buf, uint32_t codepoint);


/* ── File I/O (backs the per-format `*_parse_file` / `*_write_file`) ────── */

/**
 * Reads a whole file into a heap, NUL-terminated buffer. On failure fills
 * `err` (when non-NULL) with a message and returns `NULL`.
 *
 * @param out_len  Receives the byte length (excluding the NUL); set to 0 on
 *                 failure.
 */
char *sc_serde_read_file(const char *path, size_t *out_len, ScParseError *err);

/** Writes a NUL-terminated string to `path`; returns false on any I/O error. */
bool sc_serde_write_file(const char *path, const char *data);


/* ── Concrete value representation ─────────────────────────────────────── */

/** Insertion-ordered array storage. */
typedef struct ScValueArray {
    ScValue **items;    /**< Owned child pointers. */
    size_t    count;    /**< Number of children. */
    size_t    capacity; /**< Allocated slots. */
} ScValueArray;

/** Insertion-ordered object storage (parallel key/value arrays). */
typedef struct ScValueObject {
    char    **keys;     /**< Owned member keys. */
    ScValue **values;   /**< Owned member values. */
    size_t    count;    /**< Number of members. */
    size_t    capacity; /**< Allocated slots. */
} ScValueObject;

/** The node behind the opaque public `ScValue`. */
struct ScValue {
    ScValueType type;
    union {
        bool          boolean;
        int64_t       integer;
        double        number;
        char         *string; /**< STRING and DATETIME; owned. */
        ScValueArray  array;
        ScValueObject object;
    } as;
};

/**
 * Allocates a string-backed value of the given type (`SC_VALUE_STRING` or
 * `SC_VALUE_DATETIME`), copying `text`. Internal: lets the format parsers
 * build datetimes, which have no public constructor.
 */
ScValue *sc_value_string_typed(ScValueType type, const char *text);
