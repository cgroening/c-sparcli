#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width */

#include <stdlib.h>
#include <string.h>


/** Render-time state for a single multi-line textarea. */
typedef struct Textarea {
    const char *prompt;
    char *buf;
    size_t len;
    size_t cap;

    /** Byte offset of the cursor. */
    size_t cursor;
    ScTextareaOpts opts;
} Textarea;

static const char *const DEFAULT_HINT =
    "ctrl-d submit \xc2\xb7 enter newline \xc2\xb7 esc cancel";


static bool init_state(Textarea *self, const char *prompt,
                       ScTextareaOpts opts, const char *content);
static ScRendered *ta_render(void *state);
    static ScRendered *render_inline(const Textarea *self);
    static ScRendered *render_boxed(const Textarea *self);
        static void append_content(const Textarea *self, ScText *text,
                                   int width);
        static const char *ta_hint(const Textarea *self);
static void ta_on_key(void *state, ScKey key, bool *done, bool *cancel);
static char *ta_edit_get(void *state);
static void ta_edit_set(void *state, const char *text);
    static void insert(Textarea *self, const char *bytes, size_t len);
        static bool ensure_cap(Textarea *self, size_t extra);
    static void del_range(Textarea *self, size_t from, size_t to);
    static void move_vertical(Textarea *self, int direction);
        static size_t col_of(const Textarea *self, size_t off);
        static size_t offset_at_col(const Textarea *self, size_t line_begin,
                                    size_t col);
    static size_t line_start(const Textarea *self, size_t off);
    static size_t line_end(const Textarea *self, size_t off);
static size_t prev_b(const char *buf, size_t off);
static size_t next_b(const char *buf, size_t len, size_t off);


ScInputStatus sc_textarea(const char *prompt, char **out, ScTextareaOpts opts) {
    if (!prompt || !out) {
        return SC_INPUT_ERROR;
    }
    sc_theme_apply_textarea(&opts);

    Textarea state;
    if (!init_state(&state, prompt, opts, opts.initial)) {
        return SC_INPUT_ERROR;
    }

    ScPromptVTable vtable = {
        .render = ta_render,
        .on_key = ta_on_key,
        .paste = SC_PASTE_MULTILINE,
        .edit_get = ta_edit_get,
        .edit_set = ta_edit_set,
    };
    ScPromptShortcuts sk = {
        opts.shortcuts, opts.n_shortcuts, opts.out_shortcut_id
    };
    ScPromptEditor ed = {
        opts.external_editor, opts.editor, opts.editor_key
    };
    ScInputStatus status = sc_prompt_run(
        &vtable, &state, opts.shortcuts ? &sk : NULL,
        opts.external_editor ? &ed : NULL);
    if (status != SC_INPUT_OK) {
        free(state.buf);
        return status;
    }

    *out = state.buf;   // hand ownership to the caller
    if (!opts.hide_summary) {
        // Count lines for a compact summary (don't echo the whole body).
        size_t lines = state.len ? 1 : 0;
        for (size_t i = 0; i < state.len; i++) {
            if (state.buf[i] == '\n') {
                lines++;
            }
        }
        char line[96];
        snprintf(line, sizeof line, "? %s (%zu line%s)", prompt, lines,
                 lines == 1 ? "" : "s");
        sc_println(line, opts.summary_style);
    }
    return SC_INPUT_OK;
}

ScRendered *sc_textarea_frame(const char *prompt, const char *content,
                              ScTextareaOpts opts) {
    Textarea state;
    if (!init_state(&state, prompt, opts, content)) {
        return NULL;
    }
    ScRendered *rendered = ta_render(&state);
    free(state.buf);
    return rendered;
}

/** Initializes `self` with `content` (cursor at end). */
static bool init_state(Textarea *self, const char *prompt,
                       ScTextareaOpts opts, const char *content) {
    self->prompt = prompt;
    self->opts = opts;
    size_t len = content ? strlen(content) : 0;
    self->cap = len + 32;
    self->buf = malloc(self->cap);
    if (!self->buf) {
        return false;
    }
    if (len) {
        memcpy(self->buf, content, len);
    }
    self->buf[len] = '\0';
    self->len = len;
    self->cursor = len;
    return true;
}

/** External-editor hook: hands the editor a copy of the current text. */
static char *ta_edit_get(void *state) {
    Textarea *self = state;
    return strdup(self->buf ? self->buf : "");
}

/** External-editor hook: replaces the buffer with the editor result. */
static void ta_edit_set(void *state, const char *text) {
    Textarea *self = state;
    size_t len = text ? strlen(text) : 0;
    char *buf = malloc(len + 32);
    if (!buf) {
        return;   // keep the old buffer on allocation failure
    }
    if (len) {
        memcpy(buf, text, len);
    }
    buf[len] = '\0';
    free(self->buf);
    self->buf = buf;
    self->cap = len + 32;
    self->len = len;
    self->cursor = len;
}

static ScRendered *ta_render(void *state) {
    Textarea *self = state;
    return self->opts.box.enabled ? render_boxed(self) : render_inline(self);
}

/** Inline: prompt line, content, footer. */
static ScRendered *render_inline(const Textarea *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    if (self->prompt || self->opts.prompt_text) {
        sc_prompt_append(text, self->prompt, self->opts.prompt_style,
                         self->opts.prompt_markup, self->opts.prompt_text);
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
    }
    int width = self->opts.box.width > 0 ? self->opts.box.width
                                         : sc_terminal_width();
    append_content(self, text, width);
    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    return sc_compose_hint(body, ta_hint(self), self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/** Boxed: content inside a panel (prompt = top title), footer stacked below. */
static ScRendered *render_boxed(const Textarea *self) {
    ScText *inner = sc_text_new();
    if (!inner) {
        return NULL;
    }
    int panel_width = self->opts.box.width > 0 ? self->opts.box.width
                                               : sc_terminal_width() - 2;
    int field = panel_width - 4;   // interior minus borders + padding
    if (field < 1) {
        field = 1;
    }
    append_content(self, inner, field);

    ScText *title_text = sc_prompt_build(self->prompt, self->opts.prompt_style,
                                         self->opts.prompt_markup,
                                         self->opts.prompt_text);
    ScPanelOpts opts = {
        .border = self->opts.box.border,
        .bg = self->opts.box.bg,
        .title = { .text = self->prompt, .rich_text = title_text,
                   .style = self->opts.prompt_style,
                   .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding = sc_box_padding(self->opts.box.padding),
        .margin = self->opts.box.margin,
        .content_align = SC_ALIGN_LEFT,
    };
    if (opts.border.type == SC_BORDER_NONE) {
        opts.border.type = SC_BORDER_ROUNDED;
    }
    if (self->opts.box.width > 0) {
        opts.width = self->opts.box.width;
    } else {
        opts.full_width = true;
    }

    ScRendered *panel = sc_capture_panel_text(inner, opts);
    sc_text_free(inner);
    sc_text_free(title_text);
    if (!panel) {
        return NULL;
    }
    return sc_compose_hint(panel, ta_hint(self), self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/**
 * Appends the multi-line content (cursor cell marked, hard newlines preserved).
 * When `width > 0`, logical lines are soft-wrapped at that column so long lines
 * never overflow; the cursor is emitted inline, so it lands on the right
 * wrapped row automatically. (Navigation stays logical-line based.)
 */
static void append_content(const Textarea *self, ScText *text, int width) {
    ScTextStyle cursor_style = sc_style_set(self->opts.cursor_style)
        ? self->opts.cursor_style
        : (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,
                         SC_ANSI_COLOR_WHITE };

    if (self->len == 0 && self->opts.placeholder && self->opts.placeholder[0]) {
        sc_text_append(text, " ", cursor_style);
        sc_text_append(text, self->opts.placeholder, (ScTextStyle){
            SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        return;
    }
    int col = 0;
    size_t i = 0;
    while (i < self->len) {
        if (self->buf[i] == '\n') {
            if (i == self->cursor) {
                sc_text_append(text, " ", cursor_style);
            }
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
            i++;
            col = 0;
            continue;
        }
        if (width > 0 && col >= width) {   // soft wrap
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
            col = 0;
        }
        size_t end = next_b(self->buf, self->len, i);
        char glyph[8];
        memcpy(glyph, self->buf + i, end - i);
        glyph[end - i] = '\0';
        sc_text_append(
            text, glyph,
            (i == self->cursor) ? cursor_style : self->opts.value_style
        );
        i = end;
        col++;
    }
    if (self->cursor == self->len) {
        if (width > 0 && col >= width) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
        }
        sc_text_append(text, " ", cursor_style);   // trailing cursor
    }
}

static const char *ta_hint(const Textarea *self) {
    return self->opts.hint ? self->opts.hint : DEFAULT_HINT;
}

static void ta_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    Textarea *self = state;
    switch (key.type) {
        case SC_KEY_CHAR:
            insert(self, key.bytes, strlen(key.bytes));
            break;
        case SC_KEY_ENTER:
            insert(self, "\n", 1);
            break;
        case SC_KEY_CTRL_D:
            *done = true;
            break;
        case SC_KEY_BACKSPACE:
            if (self->cursor > 0) {
                del_range(self, prev_b(self->buf, self->cursor), self->cursor);
            }
            break;
        case SC_KEY_DELETE:
            if (self->cursor < self->len) {
                del_range(self, self->cursor,
                          next_b(self->buf, self->len, self->cursor));
            }
            break;
        case SC_KEY_LEFT:
            self->cursor = prev_b(self->buf, self->cursor);
            break;
        case SC_KEY_RIGHT:
            self->cursor = next_b(self->buf, self->len, self->cursor);
            break;
        case SC_KEY_UP:
            move_vertical(self, -1);
            break;
        case SC_KEY_DOWN:
            move_vertical(self, +1);
            break;
        case SC_KEY_HOME:
            self->cursor = line_start(self, self->cursor);
            break;
        case SC_KEY_END:
            self->cursor = line_end(self, self->cursor);
            break;
        default:
            break;
    }
}

/** Inserts `len` bytes at the cursor and advances it. */
static void insert(Textarea *self, const char *bytes, size_t len) {
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
static bool ensure_cap(Textarea *self, size_t extra) {
    if (self->len + extra + 1 <= self->cap) {
        return true;
    }
    size_t cap = self->cap ? self->cap : 32;
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
static void del_range(Textarea *self, size_t from, size_t to) {
    if (to <= from) {
        return;
    }
    memmove(self->buf + from, self->buf + to, self->len - to);
    self->len -= (to - from);
    self->cursor = from;
    self->buf[self->len] = '\0';
}

/** Moves the cursor one logical line up (`direction < 0`) or down. */
static void move_vertical(Textarea *self, int direction) {
    size_t col = col_of(self, self->cursor);
    size_t begin = line_start(self, self->cursor);
    if (direction < 0) {
        if (begin == 0) {
            return;
        }
        size_t prev_begin = line_start(self, begin - 1);
        self->cursor = offset_at_col(self, prev_begin, col);
    } else {
        size_t end = line_end(self, self->cursor);
        if (end >= self->len) {
            return;
        }
        self->cursor = offset_at_col(self, end + 1, col);
    }
}

/** Codepoint column of `off` within its line. */
static size_t col_of(const Textarea *self, size_t off) {
    size_t begin = line_start(self, off);
    size_t col = 0;
    for (size_t i = begin; i < off; i = next_b(self->buf, self->len, i)) {
        col++;
    }
    return col;
}

/** Byte offset of column `col` on the line starting at `line_begin`. */
static size_t offset_at_col(const Textarea *self, size_t line_begin,
                            size_t col) {
    size_t i = line_begin;
    size_t count = 0;
    size_t end = line_end(self, line_begin);
    while (count < col && i < end) {
        i = next_b(self->buf, self->len, i);
        count++;
    }
    return i;
}

/** Byte offset of the start of the line containing `off`. */
static size_t line_start(const Textarea *self, size_t off) {
    while (off > 0 && self->buf[off - 1] != '\n') {
        off--;
    }
    return off;
}

/** Byte offset of the newline (or end) terminating the line at `off`. */
static size_t line_end(const Textarea *self, size_t off) {
    while (off < self->len && self->buf[off] != '\n') {
        off++;
    }
    return off;
}

/** Byte offset of the codepoint boundary before `off`. */
static size_t prev_b(const char *buf, size_t off) {
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
static size_t next_b(const char *buf, size_t len, size_t off) {
    if (off >= len) {
        return len;
    }
    size_t i = off + 1;
    while (i < len && ((unsigned char)buf[i] & 0xC0) == 0x80) {
        i++;
    }
    return i;
}
