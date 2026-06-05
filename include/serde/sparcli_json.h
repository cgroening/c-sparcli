#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_json.h
 * @brief Standard JSON reader and writer over the shared `ScValue` model.
 *
 * The reader is a strict, depth-limited recursive-descent parser (RFC 8259:
 * no trailing commas, no comments, no trailing data). The writer emits
 * round-trippable JSON, compact by default or indented via `ScJsonWriteOpts`.
 */


/** Options controlling `sc_json_write`. Zero-init = compact, single line. */
typedef struct ScJsonWriteOpts {
    /** Spaces of indentation per nesting level; `0` = compact (no newlines). */
    int indent;

    /** When `true`, object keys are emitted in ascending byte order. */
    bool sort_keys;

    /** When `true`, a final newline is appended to the output. */
    bool trailing_newline;
} ScJsonWriteOpts;


/**
 * Parses a JSON document into an `ScValue` tree.
 *
 * @param src  Source bytes (need not be NUL-terminated).
 * @param len  Length of `src` in bytes.
 * @param err  Optional; on failure it is filled with the location and
 *             reason. May be `NULL` if the caller does not need detail.
 * @return     Heap value tree (free with `sc_value_free`), or `NULL` on a
 *             parse or allocation error.
 *
 * @code
 * ScParseError err = { 0 };
 * ScValue *root = sc_json_parse(text, strlen(text), &err);
 * if (!root) {
 *     sc_die(sc_parse_error_to_error(&err));
 * }
 * @endcode
 */
SPARCLI_EXPORT ScValue *sc_json_parse(
    const char *src, size_t len, ScParseError *err
);

/**
 * Serializes an `ScValue` tree to a JSON string.
 *
 * @param value  Tree to serialize; `NULL` is written as `null`.
 * @param opts   Formatting options.
 * @return       Heap-allocated, NUL-terminated JSON (caller `free`s it), or
 *               `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_json_write(const ScValue *value, ScJsonWriteOpts opts);

/**
 * Reads and parses a JSON file.
 *
 * @param path  File path.
 * @param err   Optional; filled on a read or parse error.
 * @return      Heap value tree (free with `sc_value_free`), or `NULL`.
 */
SPARCLI_EXPORT ScValue *sc_json_parse_file(const char *path, ScParseError *err);

/**
 * Serializes `value` and writes it to `path` (overwriting it).
 *
 * @return  `true` on success, `false` on a serialization or I/O error.
 */
SPARCLI_EXPORT bool sc_json_write_file(
    const ScValue *value, const char *path, ScJsonWriteOpts opts
);

SPARCLI_END_DECLS
