#pragma once

#include "core/sparcli_export.h"
#include "serde/sparcli_value.h"
#include "serde/sparcli_parse_error.h"

#include <stdbool.h>
#include <stdint.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_config.h
 * @brief Layered application configuration: defaults < file < env < flags.
 *
 * The "klammer" around the serde data model and the rest of the framework.
 * A `ScConfig` holds one `ScValue` object tree that is built up from layers
 * applied in increasing precedence:
 *
 *   1. `sc_config_set_defaults`  - built-in defaults (lowest priority)
 *   2. `sc_config_load_file`     - a config file (JSON/TOML/YAML by extension)
 *   3. `sc_config_load_env`      - environment variables under a prefix
 *   4. `sc_config_merge` / `_set` - explicit overrides, e.g. parsed CLI flags
 *
 * Each later layer deep-merges over the earlier ones (objects merge
 * recursively, scalars/arrays replace), so a value set in a higher layer
 * wins. Read back with the dotted-path typed getters.
 *
 * @note Because this depends on the serde layer (`ScValue`), it is **not**
 * part of the `sparcli.h` / `app/sparcli_app.h` umbrella - include this
 * header explicitly, like `<serde/sparcli_serde.h>`.
 */


/** Result of @ref sc_config_load_file. */
typedef enum ScConfigStatus {
    SC_CONFIG_OK = 0,   /**< File found, parsed and merged. */
    SC_CONFIG_MISSING,  /**< File does not exist (not an error for optional
                             config; nothing was merged). */
    SC_CONFIG_ERROR,    /**< File exists but could not be read/parsed (`err`
                             is filled), or its format is unknown. */
} ScConfigStatus;


/** Opaque layered configuration handle. */
typedef struct ScConfig ScConfig;


/**
 * Creates an empty configuration (an empty object tree).
 *
 * @return  Heap handle (free with @ref sc_config_free), or `NULL` on OOM.
 */
SPARCLI_EXPORT ScConfig *sc_config_new(void);

/** Frees the configuration and its value tree. `NULL`-safe. */
SPARCLI_EXPORT void sc_config_free(ScConfig *cfg);


/**
 * Merges a defaults object as the base layer.
 *
 * @param cfg       Configuration handle.
 * @param defaults  Object whose members are deep-copied in (borrowed,
 *                  unchanged).
 * @return          `true` on success; `false` when an argument is invalid
 *                  or `defaults` is not an object.
 */
SPARCLI_EXPORT bool sc_config_set_defaults(
    ScConfig *cfg, const ScValue *defaults
);

/**
 * Loads a config file and merges it over the current values. The format is
 * chosen from the path extension: `.json`, `.toml`, `.yaml`/`.yml`.
 *
 * A missing file is reported as @ref SC_CONFIG_MISSING (not an error), so the
 * same call works whether or not the user has a config file yet.
 *
 * @param cfg   Configuration handle.
 * @param path  File path.
 * @param err   Optional; filled on a parse error (when the result is
 *              @ref SC_CONFIG_ERROR).
 * @return      Load status (see @ref ScConfigStatus).
 */
SPARCLI_EXPORT ScConfigStatus sc_config_load_file(
    ScConfig *cfg, const char *path, ScParseError *err
);

/**
 * Merges environment variables under `prefix` over the current values.
 *
 * For each variable whose name starts with `prefix`, the remainder is
 * lowercased to form a key path: a double underscore `__` denotes nesting,
 * single underscores are kept literally. So with `prefix = "MYAPP_"`,
 * `MYAPP_SERVER__MAX_CONN=10` sets `server.max_conn = 10`. Values are
 * coerced: `true`/`false` → bool, integer / float literals → numbers,
 * everything else stays a string.
 *
 * @param cfg     Configuration handle.
 * @param prefix  Variable-name prefix (e.g. `"MYAPP_"`); `NULL`/empty is a
 *                no-op.
 */
SPARCLI_EXPORT void sc_config_load_env(ScConfig *cfg, const char *prefix);

/**
 * Deep-merges an explicit overlay object over the current values (highest
 * precedence layer; use it for parsed CLI flags).
 *
 * @param cfg      Configuration handle.
 * @param overlay  Object to merge in (borrowed, deep-copied, unchanged).
 * @return         `true` on success; `false` when `overlay` is not an object.
 */
SPARCLI_EXPORT bool sc_config_merge(ScConfig *cfg, const ScValue *overlay);

/**
 * Sets a single value at a dotted path, creating intermediate objects, e.g.
 * `sc_config_set(cfg, "server.port", sc_value_int(8080))`.
 *
 * @param cfg    Configuration handle.
 * @param path   Dotted key path.
 * @param value  Value to store; **ownership is transferred** (freed here on
 *               both success and failure).
 * @return       `true` on success; `false` on invalid arguments / OOM.
 */
SPARCLI_EXPORT bool sc_config_set(
    ScConfig *cfg, const char *path, ScValue *value
);


/** Returns the merged value tree (borrowed; owned by `cfg`). */
SPARCLI_EXPORT const ScValue *sc_config_root(const ScConfig *cfg);

/** Resolves a dotted path; borrowed node or `NULL`. */
SPARCLI_EXPORT const ScValue *sc_config_get(
    const ScConfig *cfg, const char *path
);

/** Boolean at `path`, or `fallback` when absent / wrong type. */
SPARCLI_EXPORT bool sc_config_get_bool(
    const ScConfig *cfg, const char *path, bool fallback
);

/** Integer at `path`, or `fallback` when absent / wrong type. */
SPARCLI_EXPORT int64_t sc_config_get_int(
    const ScConfig *cfg, const char *path, int64_t fallback
);

/** Number at `path` as a double, or `fallback` when absent / not numeric. */
SPARCLI_EXPORT double sc_config_get_float(
    const ScConfig *cfg, const char *path, double fallback
);

/** String at `path`, or `fallback` when absent / not a string (borrowed). */
SPARCLI_EXPORT const char *sc_config_get_string(
    const ScConfig *cfg, const char *path, const char *fallback
);

SPARCLI_END_DECLS
