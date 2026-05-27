#pragma once

#include "sparcli.h"
#include <stddef.h>
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
 * Return the current terminal width in columns or 80 if it cannot
 * be determined.
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
 * This function must be used instead of `strlen()` whenever a display width for
 * alignment or truncation is needed. `strlen()` returns the byte count, which
 * is wrong for multi-byte UTF-8 sequences: a single character like '€' is
 * 3 bytes but occupies only 1 terminal column. This function walks the byte
 * sequence, skips continuation bytes and counts one column per codepoint.
 *
 * Assumes all code points occupy 1 column (correct for non-CJK).
 */
static inline size_t sc_utf8_string_length(
    const char *string, size_t byte_length
) {
    size_t columns = 0;
    const unsigned char *cursor = (const unsigned char *)string;
    const unsigned char *end = cursor + byte_length;

    // Walk the byte sequence, skipping continuation bytes and counting one
    // column per code point
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
 * Use this instead of a byte-length limit when truncating UTF-8 text for
 * display. Cutting at a raw byte offset can split a multi-byte sequence in the
 * middle, producing garbage output. This function stops only at codepoint
 * boundaries, so the returned byte count is always safe to pass to e.g.
 * fwrite() or memcpy().
 */
static inline size_t sc_utf8_trim_to_cols(const char *string, int max_columns) {
    const unsigned char *cursor = (const unsigned char *)string;
    int columns = 0;
    while (*cursor && columns < max_columns) {
        unsigned char current_byte = *cursor;
        if ((current_byte & 0x80) == 0x00) {
            cursor += 1;
        } else if ((current_byte & 0xE0) == 0xC0) {
            cursor += 2;
        } else if ((current_byte & 0xF0) == 0xE0) {
            cursor += 3;
        } else {
            cursor += 4;
        }
        columns++;
    }
    return (size_t)(cursor - (const unsigned char *)string);
}
