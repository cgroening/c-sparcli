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
 * Counts visible terminal columns in the first `byte_len` bytes of a UTF-8
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
 * Returns `true` when `color` should emit ANSI background escapes.
 *
 * Both `SC_ANSI_COLOR_NONE` (`index == -2`) and a zero-initialized
 * `ScColor` (`{0, 0, 0, 0}`, indistinguishable from
 * `SC_ANSI_COLOR_BLACK`) return `false`. Use
 * `sc_ansi_color_from_rgb(0, 0, 0)` to specify explicit black.
 */
static inline bool sc_color_is_active(ScColor color) {
    if (color.index == -2) { return false; }
    bool is_zero_init = color.index == 0
        && color.r == 0 && color.g == 0 && color.b == 0;
    return !is_zero_init;
}

/**
 * Returns `true` when `style` carries any formatting (non-zero attribute
 * or an active fg/bg color). Zero-initialized `ScTextStyle` returns
 * `false`, signalling that the caller can skip ANSI emission entirely.
 */
static inline bool sc_style_has_format(ScTextStyle style) {
    return style.attr != 0
        || style.fg.index != 0 || style.fg.r || style.fg.g || style.fg.b
        || style.bg.index != 0 || style.bg.r || style.bg.g || style.bg.b;
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
