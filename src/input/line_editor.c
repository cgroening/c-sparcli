#include "input_internal.h"

#include <stdlib.h>
#include <string.h>


/* Edge markers shown when a long value is horizontally scrolled. */
#define LE_MARK_LEFT  "\xe2\x80\xb9"   /* ‹ */
#define LE_MARK_RIGHT "\xe2\x80\xba"   /* › */


/** Invariant render state for one `sc_le_render_into` call (parameter group). */
typedef struct CellView {
    const ScLineEditor *editor;
    const size_t *offsets;     /**< Codepoint boundary offsets, size cp_count+1. */
    size_t cp_count;           /**< Number of codepoints in the buffer. */
    size_t cursor_cp;          /**< Codepoint index of the cursor. */
    bool cursor_at_end;        /**< Cursor sits past the last codepoint. */
    const char *mask;          /**< Mask glyph, or NULL to show raw text. */
    ScTextStyle value_style;
    ScTextStyle cursor_cell;   /**< Resolved style of the cursor cell. */
} CellView;

/** Result of fitting the value window around the cursor at a field width. */
typedef struct ScrollWindow {
    size_t start;              /**< First visible codepoint index. */
    int content;               /**< Visible codepoint columns (markers excluded). */
    bool left_marker;          /**< Show the `‹` marker. */
    bool right_marker;         /**< Show the `›` marker. */
} ScrollWindow;


static bool ensure_cap(ScLineEditor *self, size_t extra);
static void insert_bytes(ScLineEditor *self, const char *bytes, size_t len);
static void delete_range(ScLineEditor *self, size_t from, size_t to);
static size_t word_start(const char *buf, size_t cursor);
static ScTextStyle resolve_cursor_style(ScTextStyle override);
static size_t *build_offsets(const ScLineEditor *self, size_t *out_count);
static void append_cell(ScText *text, const CellView *view, size_t cell);
static ScrollWindow compute_scroll_window(int width, size_t display_len,
                                          size_t cursor_cp);
static size_t prev_boundary(const char *buf, size_t off);
static size_t next_boundary(const char *buf, size_t len, size_t off);


void sc_le_init(ScLineEditor *self, const char *initial) {
    size_t len = initial ? strlen(initial) : 0;
    self->cap = len + 16;
    self->buf = malloc(self->cap);
    if (!self->buf) {
        self->cap = 0;
        self->len = 0;
        self->cursor = 0;
        return;
    }
    if (len) {
        memcpy(self->buf, initial, len);
    }
    self->buf[len] = '\0';
    self->len = len;
    self->cursor = len;   // start at end
}

void sc_le_free(ScLineEditor *self) {
    free(self->buf);
    self->buf = NULL;
    self->len = 0;
    self->cap = 0;
    self->cursor = 0;
}

bool sc_le_handle(ScLineEditor *self, ScKey key) {
    switch (key.type) {
        case SC_KEY_CHAR:
            insert_bytes(self, key.bytes, strlen(key.bytes));
            return true;
        case SC_KEY_BACKSPACE:
            if (self->cursor > 0) {
                delete_range(self, prev_boundary(self->buf, self->cursor),
                             self->cursor);
            }
            return true;
        case SC_KEY_DELETE:
            if (self->cursor < self->len) {
                delete_range(self, self->cursor,
                             next_boundary(self->buf, self->len, self->cursor));
            }
            return true;
        case SC_KEY_LEFT:
            self->cursor = prev_boundary(self->buf, self->cursor);
            return true;
        case SC_KEY_RIGHT:
            self->cursor = next_boundary(self->buf, self->len, self->cursor);
            return true;
        case SC_KEY_HOME:
        case SC_KEY_CTRL_A:
            self->cursor = 0;
            return true;
        case SC_KEY_END:
        case SC_KEY_CTRL_E:
            self->cursor = self->len;
            return true;
        case SC_KEY_CTRL_U:
            delete_range(self, 0, self->cursor);
            return true;
        case SC_KEY_CTRL_K:
            delete_range(self, self->cursor, self->len);
            return true;
        case SC_KEY_CTRL_W:
            delete_range(self, word_start(self->buf, self->cursor), self->cursor);
            return true;
        default:
            return false;   // Enter, Up/Down, … left to the widget
    }
}

void sc_le_render_into(
    const ScLineEditor *self, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style, int field_width
) {
    ScTextStyle cursor_cell = resolve_cursor_style(cursor_style);

    if (self->len == 0) {
        sc_text_append(text, " ", cursor_cell);
        if (placeholder && placeholder[0]) {
            sc_text_append(text, placeholder, placeholder_style);
        }
        return;
    }
    if (mask && mask[0] == '\0') {   // hidden mode: show only the cursor
        sc_text_append(text, " ", cursor_cell);
        return;
    }

    size_t cp_count = 0;
    size_t *offsets = build_offsets(self, &cp_count);
    if (!offsets) {
        return;
    }

    size_t cursor_cp = 0;
    for (size_t k = 0; k < cp_count; k++) {
        if (offsets[k] < self->cursor) {
            cursor_cp = k + 1;
        }
    }
    bool cursor_at_end = (self->cursor == self->len);
    size_t display_len = cp_count + (cursor_at_end ? 1 : 0);

    CellView view = {
        .editor = self,
        .offsets = offsets,
        .cp_count = cp_count,
        .cursor_cp = cursor_cp,
        .cursor_at_end = cursor_at_end,
        .mask = mask,
        .value_style = value_style,
        .cursor_cell = cursor_cell,
    };

    if (field_width <= 0 || display_len <= (size_t)field_width) {
        for (size_t cell = 0; cell < display_len; cell++) {
            append_cell(text, &view, cell);
        }
        free(offsets);
        return;
    }

    // Horizontal scroll: window around the cursor, markers in reserved columns.
    ScrollWindow window = compute_scroll_window(field_width, display_len, cursor_cp);
    ScTextStyle mark = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    if (window.left_marker) {
        sc_text_append(text, LE_MARK_LEFT, mark);
    }
    size_t end = window.start + (size_t)window.content;
    if (end > display_len) {
        end = display_len;
    }
    for (size_t cell = window.start; cell < end; cell++) {
        append_cell(text, &view, cell);
    }
    if (window.right_marker) {
        sc_text_append(text, LE_MARK_RIGHT, mark);
    }
    free(offsets);
}


/* ── Editing helpers ────────────────────────────────────────────────────── */

/** Inserts `len` bytes at the cursor and advances it. */
static void insert_bytes(ScLineEditor *self, const char *bytes, size_t len) {
    if (len == 0 || !ensure_cap(self, len)) {
        return;
    }
    memmove(self->buf + self->cursor + len, self->buf + self->cursor,
            self->len - self->cursor);
    memcpy(self->buf + self->cursor, bytes, len);
    self->len += len;
    self->cursor += len;
    self->buf[self->len] = '\0';
}

/** Grows the buffer to hold `extra` more bytes plus the NUL. */
static bool ensure_cap(ScLineEditor *self, size_t extra) {
    if (self->len + extra + 1 <= self->cap) {
        return true;
    }
    size_t cap = self->cap ? self->cap : 16;
    while (cap < self->len + extra + 1) {
        cap *= 2;
    }
    char *grown = realloc(self->buf, cap);
    if (!grown) {
        return false;
    }
    self->buf = grown;
    self->cap = cap;
    return true;
}

/** Deletes bytes `[from, to)` and moves the cursor to `from`. */
static void delete_range(ScLineEditor *self, size_t from, size_t to) {
    if (to <= from) {
        return;
    }
    memmove(self->buf + from, self->buf + to, self->len - to);
    self->len -= (to - from);
    self->cursor = from;
    self->buf[self->len] = '\0';
}

/** Start offset of the word ending at `cursor` (for Ctrl-W). */
static size_t word_start(const char *buf, size_t cursor) {
    size_t i = cursor;
    while (i > 0 && buf[i - 1] == ' ') {
        i = prev_boundary(buf, i);
    }
    while (i > 0 && buf[i - 1] != ' ') {
        i = prev_boundary(buf, i);
    }
    return i;
}


/* ── Rendering helpers ──────────────────────────────────────────────────── */

/**
 * Resolves the cursor-cell style: the caller-supplied `override` when it
 * carries formatting, otherwise the default inverse-ish black-on-white.
 */
static ScTextStyle resolve_cursor_style(ScTextStyle override) {
    if (sc_style_set(override)) {
        return override;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,
                          SC_ANSI_COLOR_WHITE };
}

/**
 * Allocates and fills the codepoint boundary table (size `*out_count + 1`,
 * last entry = buffer length). Returns NULL on allocation failure.
 */
static size_t *build_offsets(const ScLineEditor *self, size_t *out_count) {
    size_t cp_count = 0;
    for (size_t i = 0; i < self->len;
         i = next_boundary(self->buf, self->len, i)) {
        cp_count++;
    }
    size_t *offsets = malloc((cp_count + 1) * sizeof *offsets);
    if (!offsets) {
        *out_count = 0;
        return NULL;
    }
    size_t i = 0, k = 0;
    while (i < self->len) {
        offsets[k++] = i;
        i = next_boundary(self->buf, self->len, i);
    }
    offsets[cp_count] = self->len;
    *out_count = cp_count;
    return offsets;
}

/** Appends one display cell (a codepoint, mask glyph, or trailing cursor). */
static void append_cell(ScText *text, const CellView *view, size_t cell) {
    bool is_cursor = view->cursor_at_end ? (cell == view->cp_count)
                                         : (cell == view->cursor_cp);
    ScTextStyle style = is_cursor ? view->cursor_cell : view->value_style;
    if (cell == view->cp_count) {   // trailing cursor cell at end of buffer
        sc_text_append(text, " ", style);
        return;
    }
    if (view->mask) {
        sc_text_append(text, view->mask, style);
        return;
    }
    char glyph[8];
    size_t glyph_len = view->offsets[cell + 1] - view->offsets[cell];
    memcpy(glyph, view->editor->buf + view->offsets[cell], glyph_len);
    glyph[glyph_len] = '\0';
    sc_text_append(text, glyph, style);
}

/**
 * Fits a `width`-column window over `display_len` cells that keeps `cursor_cp`
 * visible, reserving columns for edge markers (so the cursor is never hidden
 * behind one). Iterates a few times because the markers change the budget.
 */
static ScrollWindow compute_scroll_window(int width, size_t display_len,
                                          size_t cursor_cp) {
    bool left = false, right = false;
    size_t start = 0;
    for (int pass = 0; pass < 3; pass++) {
        int content = width - (left ? 1 : 0) - (right ? 1 : 0);
        if (content < 1) {
            content = 1;
        }
        if (cursor_cp < start) {
            start = cursor_cp;
        } else if (cursor_cp >= start + (size_t)content) {
            start = cursor_cp - (size_t)content + 1;
        }
        left = (start > 0);
        right = (start + (size_t)content < display_len);
    }
    int content = width - (left ? 1 : 0) - (right ? 1 : 0);
    if (content < 1) {
        content = 1;
    }
    return (ScrollWindow){ .start = start, .content = content,
                           .left_marker = left, .right_marker = right };
}

/** Byte offset of the codepoint boundary before `off`. */
static size_t prev_boundary(const char *buf, size_t off) {
    if (off == 0) {
        return 0;
    }
    size_t i = off - 1;
    while (i > 0 && ((unsigned char)buf[i] & 0xC0) == 0x80) {
        i--;
    }
    return i;
}

/** Byte offset of the codepoint boundary after `off`. */
static size_t next_boundary(const char *buf, size_t len, size_t off) {
    if (off >= len) {
        return len;
    }
    size_t i = off + 1;
    while (i < len && ((unsigned char)buf[i] & 0xC0) == 0x80) {
        i++;
    }
    return i;
}
