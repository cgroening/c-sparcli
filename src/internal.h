#pragma once

#include "sparcli.h"
#include "core/sanitize_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


void sc_apply_colors(ScColor fg, ScColor bg);
void sc_apply_style (ScTextAttribute style);

/**
 * Prints `text` with `style` WITHOUT sanitizing.
 *
 * Internal trusted variant of `sc_print` for rendering strings that
 * already crossed the trust boundary (stored widget strings, `ScText`
 * spans, library-generated buffers). The public `sc_print` sanitizes
 * per the global `sc_allow_ansi` setting and then delegates here.
 */
void sc_print_raw(const char *text, ScTextStyle style);


/* ── Shared capacity constants ──────────────────────────────────────────── */

/**
 * Default initial capacity for dynamic arrays. Picked as a sensible
 * middle ground between immediate-resize overhead (too small) and wasted
 * memory for small collections (too large).
 *
 * Modules with hot paths that benefit from a larger initial size define
 * their own constant locally - but the default for new code is to use
 * this value.
 */
#define SC_INITIAL_CAPACITY 8

/** Doubling factor used by every dynamic-array grower. */
#define SC_GROWTH_FACTOR 2


/* ── Shared render-time span / line types ───────────────────────────────── */

/**
 * Single styled run of UTF-8 text within an `ScRenderLine`.
 *
 * Internal-only type used by both the panel and table renderers when
 * parsing `ScText` content into per-line styled spans. The string is
 * owned by the surrounding `ScRenderLine`.
 */
typedef struct ScRenderSpan {
    /**
     * Heap-allocated UTF-8 string; owned by the surrounding `ScRenderLine`.
     * Non-`const` because the line takes ownership and is responsible for
     * freeing it.
     */
    char *text;

    /** Style applied at render time. */
    ScTextStyle style;
} ScRenderSpan;

/** One rendered line of content, composed of one or more styled spans. */
typedef struct ScRenderLine {
    /** Array of styled spans making up this line. */
    ScRenderSpan *spans;

    /** Number of spans in `spans`. */
    size_t count;

    /** Total visible column width of this line. */
    size_t visible_width;
} ScRenderLine;

static inline int    min_i (int a,    int b)    { return a < b ? a : b; }
static inline int    max_i (int a,    int b)    { return a > b ? a : b; }
static inline size_t min_sz(size_t a, size_t b) { return a < b ? a : b; }
static inline size_t max_sz(size_t a, size_t b) { return a > b ? a : b; }

static inline int min_i_n(const int *arr, size_t n) {
    int r = arr[0];
    for (size_t i = 1; i < n; i++) { r = min_i(r, arr[i]); }
    return r;
}

static inline int max_i_n(const int *arr, size_t n) {
    int r = arr[0];
    for (size_t i = 1; i < n; i++) { r = max_i(r, arr[i]); }
    return r;
}

static inline size_t min_sz_n(const size_t *arr, size_t n) {
    size_t r = arr[0];
    for (size_t i = 1; i < n; i++) { r = min_sz(r, arr[i]); }
    return r;
}

static inline size_t max_sz_n(const size_t *arr, size_t n) {
    size_t r = arr[0];
    for (size_t i = 1; i < n; i++) { r = max_sz(r, arr[i]); }
    return r;
}


/**
 * Name of the environment variable that forces "no terminal attached"
 * behavior throughout the library (see `sc_no_tty_override`).
 */
#define SC_NO_TTY_ENV "SPARCLI_NO_TTY"

/**
 * Returns `true` when the `SPARCLI_NO_TTY` environment override is set to a
 * non-empty value other than `"0"`, forcing the library to behave as if no
 * terminal were attached.
 *
 * Honored by every terminal-presence check on both sides of the
 * output/input boundary: the input widgets (`sc_input_available`,
 * `sc_tty_begin`) and the output-stream checks (pager, live display).
 * Test suites set it so widgets never grab a developer's real terminal -
 * test runners like `cargo test` or `pytest` only redirect stdin/stdout,
 * the controlling terminal stays reachable via `/dev/tty` and `isatty()`.
 * Re-read on every call so tests can toggle it at runtime.
 */
static inline bool sc_no_tty_override(void) {
    const char *value = getenv(SC_NO_TTY_ENV);
    return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

/**
 * Returns the current terminal width in columns, or 80 when it cannot
 * be determined (terminal not attached, ioctl failure, etc.).
 */
static inline int sc_terminal_width(void) {
    struct winsize window_size;
    int ioctl_return_code = ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
    if (ioctl_return_code == 0 && window_size.ws_col > 0) {
        return (int)window_size.ws_col;
    }
    return 80;
}

/**
 * Returns the current terminal height in rows, or 24 when it cannot
 * be determined (terminal not attached, ioctl failure, etc.).
 */
static inline int sc_terminal_height(void) {
    struct winsize window_size;
    int ioctl_return_code = ioctl(STDOUT_FILENO, TIOCGWINSZ, &window_size);
    if (ioctl_return_code == 0 && window_size.ws_row > 0) {
        return (int)window_size.ws_row;
    }
    return 24;
}

/**
 * Decodes one UTF-8 codepoint from `[cursor, end)` into `*cp` and returns the
 * number of bytes consumed (1..4). Bounds- and malformation-safe: a truncated
 * sequence at the end of the buffer, a stray continuation byte, or an invalid
 * lead byte decode as the single raw byte (length 1, or the bytes available),
 * so the cursor always advances and never reads past `end`.
 */
static inline size_t sc_utf8_decode(
    const unsigned char *cursor, const unsigned char *end, uint32_t *cp
) {
    unsigned char lead = cursor[0];
    if ((lead & 0x80) == 0x00) { *cp = lead; return 1; }   /* ASCII fast path */
    size_t len;
    uint32_t value;
    if      ((lead & 0xE0) == 0xC0) { len = 2; value = lead & 0x1Fu; }
    else if ((lead & 0xF0) == 0xE0) { len = 3; value = lead & 0x0Fu; }
    else if ((lead & 0xF8) == 0xF0) { len = 4; value = lead & 0x07u; }
    else { *cp = lead; return 1; }   /* stray continuation / invalid lead */
    size_t avail = (size_t)(end - cursor);
    if (avail < len) { *cp = lead; return avail ? avail : 1; }   /* truncated */
    for (size_t i = 1; i < len; i++) {
        if ((cursor[i] & 0xC0) != 0x80) { *cp = lead; return 1; }  /* bad cont */
        value = (value << 6) | (cursor[i] & 0x3Fu);
    }
    *cp = value;
    return len;
}

/**
 * Display width of a Unicode codepoint in terminal columns: 0 for combining /
 * zero-width marks, 2 for East-Asian Wide / Fullwidth characters and width-2
 * emoji, 1 otherwise. Conservative on purpose — ambiguous-width symbols below
 * U+2E80 (box drawing, arrows, `✔`/`✖`, bullets, …) stay 1 column, matching how
 * sparcli renders its own glyphs and the surrounding terminals.
 */
static inline int sc_codepoint_width(uint32_t cp) {
    if ((cp >= 0x0300 && cp <= 0x036F) || (cp >= 0x0483 && cp <= 0x0489) ||
        (cp >= 0x0591 && cp <= 0x05BD) || (cp >= 0x0610 && cp <= 0x061A) ||
        (cp >= 0x064B && cp <= 0x065F) || (cp >= 0x0E31 && cp <= 0x0E3A) ||
        (cp >= 0x1AB0 && cp <= 0x1AFF) || (cp >= 0x1DC0 && cp <= 0x1DFF) ||
        (cp >= 0x20D0 && cp <= 0x20FF) || (cp >= 0xFE20 && cp <= 0xFE2F) ||
        (cp >= 0xFE00 && cp <= 0xFE0F) || (cp >= 0x1160 && cp <= 0x11FF) ||
        cp == 0x200B || cp == 0x200D || (cp >= 0xE0100 && cp <= 0xE01EF)) {
        return 0;   /* combining / zero-width */
    }
    if ((cp >= 0x1100 && cp <= 0x115F) || cp == 0x2329 || cp == 0x232A ||
        (cp >= 0x2E80 && cp <= 0x303E) || (cp >= 0x3041 && cp <= 0x33FF) ||
        (cp >= 0x3400 && cp <= 0x4DBF) || (cp >= 0x4E00 && cp <= 0x9FFF) ||
        (cp >= 0xA000 && cp <= 0xA4CF) || (cp >= 0xAC00 && cp <= 0xD7A3) ||
        (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFE10 && cp <= 0xFE19) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || (cp >= 0x1F300 && cp <= 0x1FAFF) ||
        (cp >= 0x1F900 && cp <= 0x1F9FF) || (cp >= 0x20000 && cp <= 0x3FFFD)) {
        return 2;   /* East-Asian Wide / Fullwidth / emoji */
    }
    return 1;
}

/**
 * Counts visible terminal columns in the first `byte_length` bytes of a UTF-8
 * string.
 *
 * Must be used instead of `strlen()` whenever a display width for alignment or
 * truncation is needed. Per-codepoint width comes from `sc_codepoint_width`, so
 * East-Asian Wide / Fullwidth characters and emoji count as 2 columns and
 * combining marks as 0.
 *
 * ANSI-aware: well-formed escape sequences (CSI/OSC/…) and the control
 * bytes removed by the sanitizer (everything below 0x20 except `\n`/`\t`,
 * plus DEL) count as zero columns. A string therefore measures the same
 * before and after sanitization, and strings carrying ANSI codes
 * (allow_ansi mode) measure their visible width correctly.
 */
static inline size_t sc_utf8_string_length(
    const char *string, size_t byte_length
) {
    size_t columns = 0;
    const unsigned char *cursor = (const unsigned char *)string;
    const unsigned char *end = cursor + byte_length;
    while (cursor < end) {
        unsigned char current_byte = *cursor;
        if (current_byte == 0x1B) {
            /* Escape sequences occupy no columns; a lone ESC is skipped. */
            const char *seq_end = sc_ansi_skip_seq((const char *)cursor);
            cursor = seq_end != (const char *)cursor
                ? (const unsigned char *)seq_end : cursor + 1;
            continue;
        }
        if ((current_byte < 0x20 && current_byte != '\t'
             && current_byte != '\n') || current_byte == 0x7F) {
            cursor++;   /* sanitizer-dropped control bytes: no columns */
            continue;
        }
        uint32_t cp;
        size_t n = sc_utf8_decode(cursor, end, &cp);   /* bounds-safe */
        columns += (size_t)sc_codepoint_width(cp);
        cursor += n;
    }
    return columns;
}

/**
 * Returns the number of bytes of `string` that fit within `max_columns`
 * visible terminal columns.
 *
 * Stops only at codepoint boundaries, so the returned byte count is always safe
 * to pass to `fwrite()` or `memcpy()`. Display-width-aware (`sc_codepoint_width`):
 * a 2-column glyph that would cross `max_columns` is left out entirely (the trim
 * never splits a wide glyph), so the result can be one column short of
 * `max_columns` — callers that need an exact width pad the difference. ANSI
 * escape sequences are included in the byte count but occupy no columns, so a
 * trim never cuts inside an escape sequence.
 */
static inline size_t sc_utf8_trim_to_cols(
    const char *string, int max_columns
) {
    const unsigned char *cursor = (const unsigned char *)string;
    const unsigned char *end = cursor + strlen(string);
    int columns = 0;
    while (cursor < end && columns < max_columns) {
        unsigned char current_byte = *cursor;
        if (current_byte == 0x1B) {
            const char *seq_end = sc_ansi_skip_seq((const char *)cursor);
            cursor = seq_end != (const char *)cursor
                ? (const unsigned char *)seq_end : cursor + 1;
            continue;
        }
        if ((current_byte < 0x20 && current_byte != '\t'
             && current_byte != '\n') || current_byte == 0x7F) {
            cursor++;   /* sanitizer-dropped control bytes: no columns */
            continue;
        }
        uint32_t cp;
        size_t n = sc_utf8_decode(cursor, end, &cp);   /* bounds-safe */
        int w = sc_codepoint_width(cp);
        if (columns + w > max_columns) { break; }   /* don't split a wide glyph */
        columns += w;
        cursor += n;
    }
    return (size_t)(cursor - (const unsigned char *)string);
}


/* ── Shared output helpers ───────────────────────────────────────────────── */

/**
 * Prints `count` space characters to the current output stream; no-op
 * when `count <= 0`.
 */
static inline void sc_print_spaces(int count) {
    FILE *out = sc_output_stream();
    for (int i = 0; i < count; i++) { fputc(' ', out); }
}

/**
 * Prints `count` newline characters to the current output stream; no-op
 * when `count <= 0`.
 */
static inline void sc_print_newlines(int count) {
    FILE *out = sc_output_stream();
    for (int i = 0; i < count; i++) { fputc('\n', out); }
}

/**
 * Returns `true` when `color` should emit ANSI escapes.
 *
 * Zero-initialized `ScColor` equals `SC_ANSI_COLOR_NONE` (`index == 0`)
 * and returns `false`. All named colors (`index >= 1`) and RGB mode
 * (`index == -1`) return `true`.
 */
static inline bool sc_color_is_active(ScColor color) {
    return color.index != 0;
}

/**
 * Returns `true` when `style` carries any formatting (non-zero attribute
 * or an active fg/bg color). Zero-initialized `ScTextStyle` returns
 * `false`, signalling that the caller can skip ANSI emission entirely.
 */
static inline bool sc_style_has_format(ScTextStyle style) {
    return style.attr != 0
        || sc_color_is_active(style.fg)
        || sc_color_is_active(style.bg);
}

/**
 * Prints `str` wrapped in ANSI foreground escapes; emits a reset
 * afterwards. When `color` is inactive (see `sc_color_is_active`),
 * `str` is written without any escape codes.
 */
static inline void sc_print_colored_string(const char *str, ScColor color) {
    FILE *out = sc_output_stream();
    if (!sc_color_is_active(color)) {
        fputs(str, out);
        return;
    }
    sc_apply_colors(color, SC_ANSI_COLOR_NONE);
    fputs(str, out);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, out);
}


/* ── Dynamic-array growth ────────────────────────────────────────────────── */

/**
 * Grows a dynamic array by doubling its capacity (or initializing it to
 * `initial_capacity` when zero) and reallocates the storage.
 *
 * Returns the reallocated array, or `NULL` on allocation failure. On
 * failure the previous storage is left untouched and `*capacity_in_out`
 * is not modified, so the caller can either retry or propagate the error.
 *
 * Unlike a typical `realloc` wrapper, the new capacity is written back
 * via `*capacity_in_out` on success.
 */
static inline void *sc_dynarray_grow(
    void *array, size_t *capacity_in_out,
    size_t element_size, size_t initial_capacity
) {
    size_t new_capacity = *capacity_in_out == 0
        ? initial_capacity : *capacity_in_out * 2;
    // Guard the size multiplication: realloc cannot check it the way
    // calloc does, and an overflowed (tiny) allocation would corrupt
    // memory on the next element write.
    if (element_size != 0 && new_capacity > SIZE_MAX / element_size) {
        return NULL;
    }
    void *tmp = realloc(array, new_capacity * element_size);
    if (!tmp) { return NULL; }
    *capacity_in_out = new_capacity;
    return tmp;
}
