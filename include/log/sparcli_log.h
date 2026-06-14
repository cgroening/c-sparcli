#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_export.h"
#include "platform/sc_compat.h"

#include <stdbool.h>
#include <stdio.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_log.h
 * @brief Logging: leveled, colored terminal output + plain-text file sinks.
 *
 * Two flavors share one engine:
 *
 * - The **global logger** (`sc_log_*`): process-wide, ready to use - a
 *   colored stderr sink at `SC_LOG_INFO` plus optional file sinks.
 * - **Handle-based loggers** (`sc_logger_*`): independent instances with
 *   their own sinks and options, for libraries/subsystems that must not
 *   touch process-wide state.
 *
 * Every record is rendered once (timestamp, colored level tag, message,
 * source location) and written to each sink: terminal sinks receive the
 * colored bytes, file sinks the ANSI-stripped plain text. Sinks own their
 * `FILE *` targets directly - logging never touches the thread-local
 * `sc_output_stream()` used by the widgets, so it cannot disturb captures
 * or an active pager.
 *
 * **Thread safety:** `sc_log_*` calls may run concurrently from multiple
 * threads once configuration (`sc_log_set_level`, `sc_log_add_file`,
 * `sc_log_set_opts`) is done - configure the global logger before
 * spawning threads. Handle-based loggers are not internally synchronized:
 * use one per thread or add external locking.
 */


/** Log severity, ordered from most to least verbose. */
typedef enum ScLogLevel {
    SC_LOG_DEBUG = 0,  /**< Diagnostic detail (dim). */
    SC_LOG_INFO,       /**< Normal progress messages (cyan). */
    SC_LOG_WARN,       /**< Something unexpected but recoverable (yellow). */
    SC_LOG_ERROR,      /**< Operation failed (red). */
    SC_LOG_OFF,        /**< Sink/logger disabled; nothing is emitted. */
} ScLogLevel;


/** Opaque logger handle; create with `sc_logger_new`. */
typedef struct ScLogger ScLogger;

/**
 * Format options for a logger. Zero-init renders
 * `[HH:MM:SS] LEVEL message` with no source location and no markup
 * parsing, passing every level (`SC_LOG_DEBUG`).
 */
typedef struct ScLoggerOpts {
    /** Minimum level processed at all; zero-init = `SC_LOG_DEBUG`. */
    ScLogLevel level;

    /** Hide the dim `[HH:MM:SS]` timestamp prefix. */
    bool hide_timestamps;

    /** Render the dim `file:line` suffix passed by the log macros. */
    bool show_source;

    /** Parse `[tag]` markup in messages (after printf formatting). */
    bool markup;

    /** ANSI passthrough for message text; zero-init inherits the global. */
    ScAnsiMode ansi;
} ScLoggerOpts;


/* ── Handle-based loggers ───────────────────────────────────────────────── */

/**
 * Allocates a logger with no sinks (records go nowhere until a sink is
 * added).
 *
 * @param opts  Format options; zero-init for defaults.
 * @return      Heap-allocated logger; `NULL` on allocation failure.
 *              Free with `sc_logger_free`.
 *
 * @code
 * ScLogger *logger = sc_logger_new((ScLoggerOpts){ .show_source = true });
 * sc_logger_add_terminal(logger, stderr, SC_LOG_INFO);
 * sc_logger_add_file(logger, "app.log", SC_LOG_DEBUG);
 * sc_logger_info(logger, "started with %d workers", 4);
 * sc_logger_free(logger);
 * @endcode
 */
SPARCLI_EXPORT ScLogger *sc_logger_new(ScLoggerOpts opts);

/**
 * Adds a colored terminal sink writing to `stream`.
 *
 * @param logger     Target logger; no-op when `NULL`.
 * @param stream     Output stream (e.g. `stderr`); borrowed, not closed
 *                   by the logger. No-op when `NULL`.
 * @param min_level  Minimum level this sink shows.
 */
SPARCLI_EXPORT void sc_logger_add_terminal(
    ScLogger *logger, FILE *stream, ScLogLevel min_level
);

/**
 * Adds a plain-text file sink (ANSI codes stripped), appending to `path`.
 *
 * @param logger     Target logger; no-op when `NULL`.
 * @param path       Log file path; opened in append mode and closed by
 *                   `sc_logger_free`.
 * @param min_level  Minimum level this sink records.
 * @return           `true` when the file was opened.
 */
SPARCLI_EXPORT bool sc_logger_add_file(
    ScLogger *logger, const char *path, ScLogLevel min_level
);

/**
 * Sets the logger's overall minimum level (gates every sink).
 *
 * @param logger  Target logger; no-op when `NULL`.
 * @param level   New minimum level.
 */
SPARCLI_EXPORT void sc_logger_set_level(ScLogger *logger, ScLogLevel level);

/**
 * Emits one record. Use the `sc_logger_debug/info/warn/error` macros,
 * which fill in `file` and `line` automatically.
 *
 * The formatted message is sanitized (ANSI-injection protection) per the
 * logger's `ansi` option before rendering.
 *
 * @param logger  Target logger; no-op when `NULL`.
 * @param level   Record severity.
 * @param file    Source file (`__FILE__`); `NULL` = no source location.
 * @param line    Source line (`__LINE__`).
 * @param format  printf-style format string (developer-controlled; never
 *                pass user input as the format).
 */
SPARCLI_EXPORT void sc_logger_log(
    ScLogger *logger, ScLogLevel level, const char *file, int line,
    const char *format, ...
)
    SC_ATTR_FORMAT(5, 6);

/**
 * Frees a logger: flushes and closes its file sinks (terminal streams
 * stay open - they are borrowed).
 *
 * @param logger  Logger to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_logger_free(ScLogger *logger);

/** Emits a `SC_LOG_DEBUG` record on `logger` with source location. */
#define sc_logger_debug(logger, ...)                                          \
    sc_logger_log((logger), SC_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_INFO` record on `logger` with source location. */
#define sc_logger_info(logger, ...)                                           \
    sc_logger_log((logger), SC_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_WARN` record on `logger` with source location. */
#define sc_logger_warn(logger, ...)                                           \
    sc_logger_log((logger), SC_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_ERROR` record on `logger` with source location. */
#define sc_logger_error(logger, ...)                                          \
    sc_logger_log((logger), SC_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)


/* ── Global logger ──────────────────────────────────────────────────────── */

/**
 * Sets the level of the global logger's built-in stderr sink (and the
 * overall gate). Default: `SC_LOG_INFO`.
 *
 * The stderr sink emits colors only when stderr is a terminal.
 *
 * @param level  New minimum level; `SC_LOG_OFF` silences the terminal.
 */
SPARCLI_EXPORT void sc_log_set_level(ScLogLevel level);

/** Returns the global logger's terminal level. */
SPARCLI_EXPORT ScLogLevel sc_log_level(void);

/**
 * Sets the global logger's format options (`level` inside `opts` is
 * ignored - use `sc_log_set_level`).
 *
 * Configuration is **not** thread-safe: call before spawning threads.
 *
 * @param opts  New format options.
 */
SPARCLI_EXPORT void sc_log_set_opts(ScLoggerOpts opts);

/**
 * Adds a plain-text file sink to the global logger, appending to `path`.
 *
 * Configuration is **not** thread-safe: call before spawning threads.
 *
 * @param path       Log file path (e.g. from `sc_path_file`).
 * @param min_level  Minimum level this file records.
 * @return           `true` when the file was opened.
 */
SPARCLI_EXPORT bool sc_log_add_file(const char *path, ScLogLevel min_level);

/**
 * Resets the global logger: closes file sinks and restores the default
 * configuration (stderr sink at `SC_LOG_INFO`). Mainly for tests.
 */
SPARCLI_EXPORT void sc_log_reset(void);

/**
 * Emits one record on the global logger. Use the `sc_log_debug/info/
 * warn/error` macros, which fill in `file` and `line` automatically.
 *
 * Thread-safe once configuration is done.
 *
 * @param level   Record severity.
 * @param file    Source file (`__FILE__`); `NULL` = no source location.
 * @param line    Source line (`__LINE__`).
 * @param format  printf-style format string (developer-controlled).
 */
SPARCLI_EXPORT void sc_log_log(
    ScLogLevel level, const char *file, int line, const char *format, ...
)
    SC_ATTR_FORMAT(4, 5);

/** Emits a `SC_LOG_DEBUG` record on the global logger. */
#define sc_log_debug(...)                                                     \
    sc_log_log(SC_LOG_DEBUG, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_INFO` record on the global logger. */
#define sc_log_info(...)                                                      \
    sc_log_log(SC_LOG_INFO, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_WARN` record on the global logger. */
#define sc_log_warn(...)                                                      \
    sc_log_log(SC_LOG_WARN, __FILE__, __LINE__, __VA_ARGS__)

/** Emits a `SC_LOG_ERROR` record on the global logger. */
#define sc_log_error(...)                                                     \
    sc_log_log(SC_LOG_ERROR, __FILE__, __LINE__, __VA_ARGS__)

SPARCLI_END_DECLS
