#pragma once

#include "sparcli.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>


void sc_apply_colors(ScColor fg, ScColor bg);
void sc_apply_style (ScTextAttribute style);


/* ── Shared capacity constants ──────────────────────────────────────────── */

/**
 * Default initial capacity for dynamic arrays. Picked as a sensible
 * middle ground between immediate-resize overhead (too small) and wasted
 * memory for small collections (too large).
 *
 * Modules with hot paths that benefit from a larger initial size define
 * their own constant locally — but the default for new code is to use
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
 * Counts visible terminal columns in the first `byte_length` bytes of a UTF-8
 * string.
 *
 * Must be used instead of `strlen()` whenever a display width for
 * alignment or truncation is needed. `strlen()` returns the byte count,
 * which is wrong for multi-byte UTF-8 sequences (`€` is 3 bytes but only
 * 1 terminal column). Walks the byte sequence, skips continuation bytes
 * and counts one column per codepoint. Assumes all code points occupy
 * 1 column (correct for non-CJK).
 */
static inline size_t sc_utf8_string_length(
    const char *string, size_t byte_length
) {
    size_t columns = 0;
    const unsigned char *cursor = (const unsigned char *)string;
    const unsigned char *end = cursor + byte_length;
    while (cursor < end) {
        unsigned char current_byte = *cursor;
        if      ((current_byte & 0x80) == 0x00) { cursor += 1; }
        else if ((current_byte & 0xE0) == 0xC0) { cursor += 2; }
        else if ((current_byte & 0xF0) == 0xE0) { cursor += 3; }
        else                                    { cursor += 4; }
        columns++;
    }
    return columns;
}

/**
 * Returns the number of bytes of `string` that fit within `max_columns`
 * visible terminal columns.
 *
 * Stops only at codepoint boundaries, so the returned byte count is
 * always safe to pass to `fwrite()` or `memcpy()`.
 */
static inline size_t sc_utf8_trim_to_cols(
    const char *string, int max_columns
) {
    const unsigned char *cursor = (const unsigned char *)string;
    int columns = 0;
    while (*cursor && columns < max_columns) {
        unsigned char current_byte = *cursor;
        if      ((current_byte & 0x80) == 0x00) { cursor += 1; }
        else if ((current_byte & 0xE0) == 0xC0) { cursor += 2; }
        else if ((current_byte & 0xF0) == 0xE0) { cursor += 3; }
        else                                    { cursor += 4; }
        columns++;
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
    void *tmp = realloc(array, new_capacity * element_size);
    if (!tmp) { return NULL; }
    *capacity_in_out = new_capacity;
    return tmp;
}
