#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_toml.h
 * @brief TOML reader and writer over the shared `ScValue` model.
 *
 * Covers the common TOML 1.0 surface: bare/quoted/dotted keys; basic, literal
 * and multi-line strings; integers (decimal with underscores and sign, plus
 * `0x`/`0o`/`0b`); floats (fraction/exponent, `inf`/`nan`); booleans; arrays
 * (nested, multi-line, trailing comma); inline tables; standard tables
 * (`[a.b]`) and arrays of tables (`[[a]]`); and `#` comments. Date/time values
 * are preserved verbatim as `SC_VALUE_DATETIME` strings (not interpreted).
 */


/** Options controlling `sc_toml_write`. Zero-init = insertion order. */
typedef struct ScTomlWriteOpts {
    /** When `true`, keys within each table are emitted in ascending order. */
    bool sort_keys;
} ScTomlWriteOpts;


/**
 * Parses a TOML document into an `ScValue` object tree.
 *
 * @param src  Source bytes (need not be NUL-terminated).
 * @param len  Length of `src` in bytes.
 * @param err  Optional; filled with the location and reason on failure.
 * @return     Heap object tree (free with `sc_value_free`), or `NULL` on a
 *             parse or allocation error.
 */
SPARCLI_EXPORT ScValue *sc_toml_parse(
    const char *src, size_t len, ScParseError *err
);

/**
 * Serializes an `ScValue` object tree to a TOML string.
 *
 * @param value  Tree to serialize; must be an object (`NULL` yields `""`).
 * @param opts   Formatting options.
 * @return       Heap-allocated, NUL-terminated TOML (caller `free`s it), or
 *               `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_toml_write(const ScValue *value, ScTomlWriteOpts opts);

/** Reads and parses a TOML file; `NULL` on a read or parse error (`err`
 *  filled). Free the result with `sc_value_free`. */
SPARCLI_EXPORT ScValue *sc_toml_parse_file(const char *path, ScParseError *err);

/** Serializes `value` and writes it to `path`; `false` on a write error. */
SPARCLI_EXPORT bool sc_toml_write_file(
    const ScValue *value, const char *path, ScTomlWriteOpts opts
);

SPARCLI_END_DECLS
