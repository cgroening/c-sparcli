#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stdint.h>
#include <time.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_humanize.h
 * @brief Human-readable formatting of sizes, durations, numbers and time.
 *
 * Every function returns a freshly heap-allocated string (caller must
 * `free()` it) or `NULL` on allocation failure - the same convention as
 * @ref sc_truncate / @ref sc_strip_ansi. The output is pure data (no ANSI),
 * so it composes with any widget.
 */


/**
 * Unit system for @ref sc_humanize_bytes.
 */
typedef enum ScByteUnit {
    /** SI / decimal: 1000-based with `KB`, `MB`, `GB`, … (the default). */
    SC_BYTES_SI = 0,
    /** IEC / binary: 1024-based with `KiB`, `MiB`, `GiB`, … */
    SC_BYTES_IEC,
    /** IEC magnitudes (1024-based) with short labels `K`, `M`, `G`, … */
    SC_BYTES_IEC_SHORT,
} ScByteUnit;

/**
 * Shared formatting options. Zero-init selects sensible defaults
 * (period decimal separator, comma thousands separator, one fractional
 * digit where applicable, a space before size units).
 */
typedef struct ScHumanizeOpts {
    /** Fixed number of fractional digits; `0` = per-function default. */
    int  decimals;

    /** Decimal separator character; `0` = `'.'` (use `','` for de_DE). */
    char decimal_sep;

    /** Thousands grouping separator for @ref sc_humanize_number;
        `0` = `','` (use `'.'` for de_DE). */
    char group_sep;

    /** Suppress the space between number and unit
        (`"1.5KB"` instead of `"1.5 KB"`). */
    bool no_space;
} ScHumanizeOpts;


/**
 * Formats a byte count as a human-readable size, e.g. `1536` → `"1.5 KB"`
 * (SI) or `"1.5 KiB"` (IEC).
 *
 * Raw byte counts (below the first unit threshold) are printed without a
 * fractional part; larger magnitudes use one fractional digit by default
 * with trailing zeros trimmed.
 *
 * @param bytes  Number of bytes.
 * @param unit   Unit system (see @ref ScByteUnit).
 * @return       Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_bytes(uint64_t bytes, ScByteUnit unit);

/** Options-taking variant of @ref sc_humanize_bytes. */
SPARCLI_EXPORT char *sc_humanize_bytes_opts(
    uint64_t bytes, ScByteUnit unit, ScHumanizeOpts opts
);

/**
 * Formats a plain number with thousands grouping, e.g.
 * `1234567.5` → `"1,234,567.5"`.
 *
 * @param value  The value to format.
 * @param opts   Separators / decimals (zero-init = `1,234,567.5` style).
 * @return       Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_number(double value, ScHumanizeOpts opts);

/**
 * Formats a number in compact form, e.g. `12400` → `"12.4k"`,
 * `1200000` → `"1.2M"`. Suffixes: `k`, `M`, `B`, `T`.
 *
 * @param value  The value to format.
 * @param opts   Decimals / decimal separator (grouping unused).
 * @return       Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_compact(double value, ScHumanizeOpts opts);

/**
 * Formats a ratio as a percentage, e.g. `0.42` → `"42%"`.
 *
 * @param ratio  The ratio (1.0 = 100%).
 * @param opts   `decimals` controls precision (default 0).
 * @return       Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_percent(double ratio, ScHumanizeOpts opts);

/**
 * Formats a duration as the two most-significant units, e.g.
 * `93` → `"1m 33s"`, `8054` → `"2h 14m"`, `90061` → `"1d 1h"`.
 * Durations under a minute render as `"Ns"`.
 *
 * @param seconds  Duration in seconds (negative is treated as its magnitude).
 * @return         Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_duration(double seconds);

/**
 * Formats a duration as a clock value, `"MM:SS"` (or `"HH:MM:SS"` from one
 * hour on), e.g. `93` → `"01:33"`, `3725` → `"01:02:05"`.
 *
 * @param seconds  Duration in seconds (negative is treated as its magnitude).
 * @return         Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_duration_clock(double seconds);

/**
 * Formats `when` relative to `now`, e.g. `"3 hours ago"` / `"in 2 days"`;
 * within a few seconds it returns `"just now"`. English only.
 *
 * @param when  The timestamp to describe.
 * @param now   The reference time (typically `time(NULL)`).
 * @return      Heap string, caller frees; `NULL` on allocation failure.
 */
SPARCLI_EXPORT char *sc_humanize_relative(time_t when, time_t now);

SPARCLI_END_DECLS
