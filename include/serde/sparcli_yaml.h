#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stddef.h>

SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_yaml.h
 * @brief A documented YAML subset reader and writer over `ScValue`.
 *
 * Supported: block mappings and block sequences (including the inline
 * `- key: value` mapping-item form), flow collections (`[a, b]` / `{k: v}`),
 * plain / single-quoted / double-quoted scalars, `|` and `>` block scalars
 * (basic), `#` comments, a leading `---` / trailing `...` document marker, and
 * scalar type inference (`null`/`~`, `true`/`false`, integers, floats).
 *
 * **Not** supported (by design): anchors/aliases (`&`/`*`), tags (`!`),
 * multiple documents in one stream, and complex/explicit keys (`?`). These
 * make YAML 1.2 enormous; this reader targets the config / front-matter 95%.
 */


/** Options controlling `sc_yaml_write`. Zero-init = 2-space indent. */
typedef struct ScYamlWriteOpts {
    /** Spaces of indentation per nesting level; `0` selects the default `2`. */
    int indent;

    /** When `true`, mapping keys are emitted in ascending order. */
    bool sort_keys;
} ScYamlWriteOpts;


/**
 * Parses a YAML document (the supported subset) into an `ScValue` tree.
 *
 * @param src  Source bytes (need not be NUL-terminated).
 * @param len  Length of `src` in bytes.
 * @param err  Optional; filled with the location and reason on failure.
 * @return     Heap value tree (free with `sc_value_free`), or `NULL` on a
 *             parse or allocation error.
 */
SPARCLI_EXPORT ScValue *sc_yaml_parse(
    const char *src, size_t len, ScParseError *err
);

/**
 * Serializes an `ScValue` tree to block-style YAML.
 *
 * @param value  Tree to serialize.
 * @param opts   Formatting options.
 * @return       Heap-allocated, NUL-terminated YAML (caller `free`s it), or
 *               `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_yaml_write(const ScValue *value, ScYamlWriteOpts opts);

/** Reads and parses a YAML file; `NULL` on a read or parse error (`err`
 *  filled). Free the result with `sc_value_free`. */
SPARCLI_EXPORT ScValue *sc_yaml_parse_file(const char *path, ScParseError *err);

/** Serializes `value` and writes it to `path`; `false` on a write error. */
SPARCLI_EXPORT bool sc_yaml_write_file(
    const ScValue *value, const char *path, ScYamlWriteOpts opts
);

SPARCLI_END_DECLS
