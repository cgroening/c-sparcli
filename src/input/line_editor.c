#include "input_internal.h"

#include <stdlib.h>
#include <string.h>


/* ── UTF-8 cursor helpers ───────────────────────────────────────────────── */

/** Returns the byte offset of the codepoint boundary before `off`. */
static size_t prev_boundary(const char *buf, size_t off) {
    if (off == 0) { return 0; }
    size_t i = off - 1;
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) { i--; }
    return i;
}

/** Returns the byte offset of the codepoint boundary after `off`. */
static size_t next_boundary(const char *buf, size_t len, size_t off) {
    if (off >= len) { return len; }
    size_t i = off + 1;
    while (i < len && ((unsigned char)buf[i] & 0xC0) == 0x80) { i++; }
    return i;
}


/* ── Lifecycle ──────────────────────────────────────────────────────────── */

void sc_le_init(ScLineEditor *e, const char *initial) {
    size_t n = initial ? strlen(initial) : 0;
    e->cap = n + 16;
    e->buf = malloc(e->cap);
    if (!e->buf) { e->cap = 0; e->len = 0; e->cursor = 0; return; }
    if (n) { memcpy(e->buf, initial, n); }
    e->buf[n] = '\0';
    e->len    = n;
    e->cursor = n;  /* start at end */
}

void sc_le_free(ScLineEditor *e) {
    free(e->buf);
    e->buf = NULL;
    e->len = e->cap = e->cursor = 0;
}

/** Ensures room for `extra` more bytes plus the NUL. */
static bool ensure_cap(ScLineEditor *e, size_t extra) {
    if (e->len + extra + 1 <= e->cap) { return true; }
    size_t cap = e->cap ? e->cap : 16;
    while (cap < e->len + extra + 1) { cap *= 2; }
    char *p = realloc(e->buf, cap);
    if (!p) { return false; }
    e->buf = p;
    e->cap = cap;
    return true;
}


/* ── Editing ────────────────────────────────────────────────────────────── */

static void insert_bytes(ScLineEditor *e, const char *bytes, size_t n) {
    if (n == 0 || !ensure_cap(e, n)) { return; }
    memmove(e->buf + e->cursor + n, e->buf + e->cursor, e->len - e->cursor);
    memcpy(e->buf + e->cursor, bytes, n);
    e->len    += n;
    e->cursor += n;
    e->buf[e->len] = '\0';
}

static void delete_range(ScLineEditor *e, size_t from, size_t to) {
    if (to <= from) { return; }
    memmove(e->buf + from, e->buf + to, e->len - to);
    e->len   -= (to - from);
    e->cursor = from;
    e->buf[e->len] = '\0';
}

/** Start offset of the word ending at the cursor (for Ctrl-W). */
static size_t word_start(const char *buf, size_t cursor) {
    size_t i = cursor;
    while (i > 0 && buf[i - 1] == ' ') { i = prev_boundary(buf, i); }
    while (i > 0 && buf[i - 1] != ' ') { i = prev_boundary(buf, i); }
    return i;
}

bool sc_le_handle(ScLineEditor *e, ScKey key) {
    switch (key.type) {
        case SC_KEY_CHAR:
            insert_bytes(e, key.bytes, strlen(key.bytes));
            return true;
        case SC_KEY_BACKSPACE:
            if (e->cursor > 0) {
                delete_range(e, prev_boundary(e->buf, e->cursor), e->cursor);
            }
            return true;
        case SC_KEY_DELETE:
            if (e->cursor < e->len) {
                delete_range(e, e->cursor,
                             next_boundary(e->buf, e->len, e->cursor));
            }
            return true;
        case SC_KEY_LEFT:
            e->cursor = prev_boundary(e->buf, e->cursor);
            return true;
        case SC_KEY_RIGHT:
            e->cursor = next_boundary(e->buf, e->len, e->cursor);
            return true;
        case SC_KEY_HOME:
        case SC_KEY_CTRL_A:
            e->cursor = 0;
            return true;
        case SC_KEY_END:
        case SC_KEY_CTRL_E:
            e->cursor = e->len;
            return true;
        case SC_KEY_CTRL_U:
            delete_range(e, 0, e->cursor);
            return true;
        case SC_KEY_CTRL_K:
            delete_range(e, e->cursor, e->len);
            return true;
        case SC_KEY_CTRL_W:
            delete_range(e, word_start(e->buf, e->cursor), e->cursor);
            return true;
        default:
            return false;  /* Enter, Up/Down, … left to the widget */
    }
}


/* ── Rendering ──────────────────────────────────────────────────────────── */

/**
 * Resolves the cursor-cell style: the caller-supplied `override` when it
 * carries formatting, otherwise the default inverse-ish black-on-white.
 */
static ScTextStyle resolve_cursor_style(ScTextStyle override) {
    if (sc_style_set(override)) { return override; }
    return (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_WHITE };
}

/**
 * Appends one displayed segment covering bytes `[from,to)` of the editor,
 * applying the mask if requested.
 */
static void append_segment(
    ScText *text, const char *buf, size_t from, size_t to,
    const char *mask, ScTextStyle style
) {
    if (to <= from) { return; }
    if (mask && mask[0] == '\0') { return; }  /* hidden mode: show nothing */
    if (!mask) {
        char *seg = malloc(to - from + 1);
        if (!seg) { return; }
        memcpy(seg, buf + from, to - from);
        seg[to - from] = '\0';
        sc_text_append(text, seg, style);
        free(seg);
        return;
    }
    /* Masked: emit one mask glyph per codepoint in the range. */
    size_t i = from;
    while (i < to) {
        sc_text_append(text, mask, style);
        i = next_boundary(buf, to, i);
    }
}

void sc_le_render_into(
    const ScLineEditor *e, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style
) {
    ScTextStyle cur = resolve_cursor_style(cursor_style);

    if (e->len == 0) {
        sc_text_append(text, " ", cur);  /* cursor */
        if (placeholder && placeholder[0]) {
            sc_text_append(text, placeholder, placeholder_style);
        }
        return;
    }

    /* Left of cursor. */
    append_segment(text, e->buf, 0, e->cursor, mask, value_style);

    /* The character under the cursor (or a trailing block at end). */
    if (e->cursor < e->len) {
        size_t end = next_boundary(e->buf, e->len, e->cursor);
        if (mask && mask[0]) {
            sc_text_append(text, mask, cur);
        } else if (mask && mask[0] == '\0') {
            sc_text_append(text, " ", cur);
        } else {
            char glyph[8];
            size_t n = end - e->cursor;
            memcpy(glyph, e->buf + e->cursor, n);
            glyph[n] = '\0';
            sc_text_append(text, glyph, cur);
        }
        append_segment(text, e->buf, end, e->len, mask, value_style);
    } else {
        sc_text_append(text, " ", cur);  /* at end */
    }
}
