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

/* Edge markers for a horizontally-scrolled field. */
#define LE_MARK_LEFT  "\xe2\x80\xb9"  /* ‹ */
#define LE_MARK_RIGHT "\xe2\x80\xba"  /* › */

/** Appends one display cell `d` of the editor (a codepoint or mask glyph). */
static void append_cell(
    ScText *text, const ScLineEditor *e, const size_t *off, size_t ncp,
    size_t d, size_t cur_i, bool cursor_end, const char *mask,
    ScTextStyle value_style, ScTextStyle cursor_cell
) {
    bool is_cursor = cursor_end ? (d == ncp) : (d == cur_i);
    ScTextStyle st = is_cursor ? cursor_cell : value_style;
    if (d == ncp) { sc_text_append(text, " ", st); return; }  /* trailing cell */
    if (mask) {
        sc_text_append(text, mask, st);
    } else {
        char glyph[8];
        size_t n = off[d + 1] - off[d];
        memcpy(glyph, e->buf + off[d], n);
        glyph[n] = '\0';
        sc_text_append(text, glyph, st);
    }
}

void sc_le_render_into(
    const ScLineEditor *e, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style, int field_width
) {
    ScTextStyle cur = resolve_cursor_style(cursor_style);

    if (e->len == 0) {
        sc_text_append(text, " ", cur);  /* cursor */
        if (placeholder && placeholder[0]) {
            sc_text_append(text, placeholder, placeholder_style);
        }
        return;
    }
    if (mask && mask[0] == '\0') {  /* hidden mode: show only the cursor */
        sc_text_append(text, " ", cur);
        return;
    }

    /* Map codepoint boundaries so cells can be addressed by index. */
    size_t ncp = 0;
    for (size_t i = 0; i < e->len; i = next_boundary(e->buf, e->len, i)) { ncp++; }
    size_t *off = malloc((ncp + 1) * sizeof *off);
    if (!off) { return; }
    { size_t i = 0, k = 0;
      while (i < e->len) { off[k++] = i; i = next_boundary(e->buf, e->len, i); }
      off[ncp] = e->len; }

    size_t cur_i = 0;
    for (size_t k = 0; k < ncp; k++) { if (off[k] < e->cursor) { cur_i = k + 1; } }
    bool   cursor_end = (e->cursor == e->len);
    size_t L = ncp + (cursor_end ? 1 : 0);

    if (field_width <= 0 || L <= (size_t)field_width) {
        for (size_t d = 0; d < L; d++) {
            append_cell(text, e, off, ncp, d, cur_i, cursor_end, mask, value_style, cur);
        }
        free(off);
        return;
    }

    /* Horizontal scroll: pick a window [start, start+content) containing the
     * cursor, with edge markers in reserved columns (never over the cursor). */
    int W = field_width;
    bool lm = false, rm = false;
    size_t start = 0;
    for (int pass = 0; pass < 3; pass++) {
        int content = W - (lm ? 1 : 0) - (rm ? 1 : 0);
        if (content < 1) { content = 1; }
        if (cur_i < start) { start = cur_i; }
        else if (cur_i >= start + (size_t)content) { start = cur_i - (size_t)content + 1; }
        lm = (start > 0);
        rm = (start + (size_t)content < L);
    }
    int content = W - (lm ? 1 : 0) - (rm ? 1 : 0);
    if (content < 1) { content = 1; }

    ScTextStyle mark = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    if (lm) { sc_text_append(text, LE_MARK_LEFT, mark); }
    size_t end = start + (size_t)content;
    if (end > L) { end = L; }
    for (size_t d = start; d < end; d++) {
        append_cell(text, e, off, ncp, d, cur_i, cursor_end, mask, value_style, cur);
    }
    if (rm) { sc_text_append(text, LE_MARK_RIGHT, mark); }
    free(off);
}
