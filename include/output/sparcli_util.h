#pragma once

#include "core/sparcli_export.h"


SPARCLI_BEGIN_DECLS



/**
 * Returns a heap-allocated copy of `str` with all ANSI escape sequences
 * removed.
 *
 * Strips CSI sequences (`ESC [` … final byte `0x40`-`0x7E`), OSC sequences
 * (`ESC ]` … `BEL` or `ESC \`), DCS/SOS/PM/APC string sequences
 * (`ESC P/X/^/_` … `ESC \`), two-character escape sequences (e.g.
 * `ESC c`), and lone/malformed `ESC` bytes. All other bytes (including
 * control bytes like `\t`) are copied verbatim.
 *
 * @param str  Source string; `NULL` returns an empty heap string.
 * @return     Heap-allocated stripped string; caller must `free()` the result.
 *             Returns `NULL` only on allocation failure.
 */
char *sc_strip_ansi(const char *str);

/**
 * Returns a heap-allocated copy of `str` truncated to `max_cols` visible
 * columns, appending `ellipsis` when truncation happens.
 *
 * If the visible width of `str` already fits within `max_cols`, returns a
 * plain `strdup` copy. Otherwise `ellipsis` is appended after as many full
 * UTF-8 codepoints of `str` as fit into `max_cols - visible_width(ellipsis)`.
 *
 * @param str       Source string; `NULL` returns an empty heap string.
 * @param max_cols  Maximum visible width of the result, in columns.
 * @param ellipsis  Trailing marker; may be `NULL` for no marker.
 * @return          Heap-allocated truncated string; caller must `free()` it.
 *                  Returns `NULL` only on allocation failure.
 */
char *sc_truncate(const char *str, int max_cols, const char *ellipsis);

/**
 * Overwrites the current terminal line with spaces and returns the cursor
 * to column 0, then flushes stdout.
 */
SPARCLI_EXPORT void sc_clear_line(void);

SPARCLI_END_DECLS
