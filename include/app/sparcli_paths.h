#pragma once

#include "core/sparcli_export.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_paths.h
 * @brief XDG base-directory helpers for CLI applications.
 *
 * Resolves the standard per-user directories (config, data, cache, state)
 * according to the XDG Base Directory Specification: the `$XDG_*`
 * environment variable wins when it holds an absolute path, otherwise the
 * conventional `$HOME` fallback is used. Directories are created on first
 * use, so callers can write into the returned path immediately.
 */


/**
 * The four XDG base-directory kinds.
 *
 * Each kind maps to an environment variable and a `$HOME` fallback:
 *
 * | Kind             | Environment variable | Fallback           |
 * |------------------|-----------------------|--------------------|
 * | `SC_PATH_CONFIG` | `$XDG_CONFIG_HOME`    | `~/.config`        |
 * | `SC_PATH_DATA`   | `$XDG_DATA_HOME`      | `~/.local/share`   |
 * | `SC_PATH_CACHE`  | `$XDG_CACHE_HOME`     | `~/.cache`         |
 * | `SC_PATH_STATE`  | `$XDG_STATE_HOME`     | `~/.local/state`   |
 */
typedef enum ScPathKind {
    SC_PATH_CONFIG = 0,  /**< Configuration files (`~/.config`). */
    SC_PATH_DATA,        /**< Persistent application data (`~/.local/share`). */
    SC_PATH_CACHE,       /**< Re-creatable cache files (`~/.cache`). */
    SC_PATH_STATE,       /**< Logs, history, runtime state (`~/.local/state`). */
} ScPathKind;


/**
 * Returns the per-application base directory for `kind`, creating it
 * (mode 0700, including parents) when it does not exist.
 *
 * The result is `<base>/<appname>`, e.g. `/home/user/.config/myapp` for
 * `SC_PATH_CONFIG`. `appname` must be a single path component: names
 * containing `/`, `..` or control characters are rejected.
 *
 * @param kind     Which base directory to resolve.
 * @param appname  Application name (one path component); rejected when
 *                 invalid.
 * @return         Heap-allocated absolute path; caller must `free()` it.
 *                 `NULL` when `appname` is invalid, `$HOME` cannot be
 *                 resolved, or directory creation fails.
 */
SPARCLI_EXPORT char *sc_path(ScPathKind kind, const char *appname);

/** Convenience wrapper: `sc_path(SC_PATH_CONFIG, appname)`. */
SPARCLI_EXPORT char *sc_path_config(const char *appname);

/** Convenience wrapper: `sc_path(SC_PATH_DATA, appname)`. */
SPARCLI_EXPORT char *sc_path_data(const char *appname);

/** Convenience wrapper: `sc_path(SC_PATH_CACHE, appname)`. */
SPARCLI_EXPORT char *sc_path_cache(const char *appname);

/** Convenience wrapper: `sc_path(SC_PATH_STATE, appname)`. */
SPARCLI_EXPORT char *sc_path_state(const char *appname);

/**
 * Returns the absolute path of a file inside an application directory,
 * creating all parent directories (mode 0700).
 *
 * The result is `<base>/<appname>/<relative>`, e.g.
 * `~/.local/state/myapp/logs/app.log` for
 * `sc_path_file(SC_PATH_STATE, "myapp", "logs/app.log")`. The file itself
 * is **not** created, only its parent directories.
 *
 * `relative` may contain subdirectories but must stay inside the
 * application directory: absolute paths, `..` segments and control
 * characters are rejected.
 *
 * @param kind      Which base directory to resolve.
 * @param appname   Application name (one path component).
 * @param relative  File path relative to the application directory.
 * @return          Heap-allocated absolute path; caller must `free()` it.
 *                  `NULL` when any argument is invalid or directory
 *                  creation fails.
 */
SPARCLI_EXPORT char *sc_path_file(
    ScPathKind kind, const char *appname, const char *relative
);

SPARCLI_END_DECLS
