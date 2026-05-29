#include "input_internal.h"

#include <stdlib.h>
#include <string.h>


typedef struct {
    const char    *prompt;
    char          *buf;
    size_t         len;
    size_t         cap;
    size_t         cursor;   /* byte offset */
    ScTextareaOpts opts;
} Textarea;


/* ── UTF-8 boundaries ───────────────────────────────────────────────────── */

static size_t prev_b(const char *b, size_t off) {
    if (off == 0) { return 0; }
    size_t i = off - 1;
    while (i > 0 && ((unsigned char)b[i] & 0xC0) == 0x80) { i--; }
    return i;
}
static size_t next_b(const char *b, size_t len, size_t off) {
    if (off >= len) { return len; }
    size_t i = off + 1;
    while (i < len && ((unsigned char)b[i] & 0xC0) == 0x80) { i++; }
    return i;
}

/* ── Buffer editing ─────────────────────────────────────────────────────── */

static bool ensure(Textarea *a, size_t extra) {
    if (a->len + extra + 1 <= a->cap) { return true; }
    size_t cap = a->cap ? a->cap : 32;
    while (cap < a->len + extra + 1) { cap *= 2; }
    char *p = realloc(a->buf, cap);
    if (!p) { return false; }
    a->buf = p; a->cap = cap;
    return true;
}
static void insert(Textarea *a, const char *bytes, size_t n) {
    if (n == 0 || !ensure(a, n)) { return; }
    memmove(a->buf + a->cursor + n, a->buf + a->cursor, a->len - a->cursor);
    memcpy(a->buf + a->cursor, bytes, n);
    a->len += n; a->cursor += n; a->buf[a->len] = '\0';
}
static void del_range(Textarea *a, size_t from, size_t to) {
    if (to <= from) { return; }
    memmove(a->buf + from, a->buf + to, a->len - to);
    a->len -= (to - from); a->cursor = from; a->buf[a->len] = '\0';
}

/* ── Line geometry ──────────────────────────────────────────────────────── */

static size_t line_start(const Textarea *a, size_t off) {
    while (off > 0 && a->buf[off - 1] != '\n') { off--; }
    return off;
}
static size_t line_end(const Textarea *a, size_t off) {
    while (off < a->len && a->buf[off] != '\n') { off++; }
    return off;
}
/** Codepoint column of `off` within its line. */
static size_t col_of(const Textarea *a, size_t off) {
    size_t ls = line_start(a, off), col = 0;
    for (size_t i = ls; i < off; i = next_b(a->buf, a->len, i)) { col++; }
    return col;
}
/** Byte offset of column `col` on the line starting at `ls`. */
static size_t offset_at_col(const Textarea *a, size_t ls, size_t col) {
    size_t i = ls, c = 0, le = line_end(a, ls);
    while (c < col && i < le) { i = next_b(a->buf, a->len, i); c++; }
    return i;
}

static void move_vertical(Textarea *a, int dir) {
    size_t col = col_of(a, a->cursor);
    size_t ls  = line_start(a, a->cursor);
    if (dir < 0) {
        if (ls == 0) { return; }
        size_t prev_ls = line_start(a, ls - 1);
        a->cursor = offset_at_col(a, prev_ls, col);
    } else {
        size_t le = line_end(a, a->cursor);
        if (le >= a->len) { return; }
        a->cursor = offset_at_col(a, le + 1, col);
    }
}

/* ── Render ─────────────────────────────────────────────────────────────── */

static ScRendered *ta_render(void *state) {
    Textarea *a = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    if (a->prompt) {
        sc_text_append(t, a->prompt, a->opts.prompt_style);
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
    }

    ScTextStyle cur = sc_style_set(a->opts.cursor_style) ? a->opts.cursor_style
        : (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_WHITE };

    if (a->len == 0 && a->opts.placeholder && a->opts.placeholder[0]) {
        sc_text_append(t, " ", cur);
        sc_text_append(t, a->opts.placeholder,
            (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    } else {
        /* Walk the buffer, marking the cursor cell and preserving newlines. */
        size_t i = 0;
        while (i < a->len) {
            if (a->buf[i] == '\n') {
                if (i == a->cursor) { sc_text_append(t, " ", cur); }
                sc_text_append(t, "\n", (ScTextStyle){ 0 });
                i++;
                continue;
            }
            size_t e = next_b(a->buf, a->len, i);
            char g[8]; memcpy(g, a->buf + i, e - i); g[e - i] = '\0';
            sc_text_append(t, g, (i == a->cursor) ? cur : a->opts.value_style);
            i = e;
        }
        if (a->cursor == a->len) { sc_text_append(t, " ", cur); }  /* trailing */
    }

    sc_append_hint(t, a->opts.hint ? a->opts.hint
                   : "ctrl-d submit \xc2\xb7 enter newline \xc2\xb7 esc cancel",
                   a->opts.hide_hint, a->opts.hint_style);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void ta_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    Textarea *a = state;
    switch (key.type) {
        case SC_KEY_CHAR:      insert(a, key.bytes, strlen(key.bytes)); break;
        case SC_KEY_ENTER:     insert(a, "\n", 1); break;
        case SC_KEY_CTRL_D:    *done = true; break;
        case SC_KEY_BACKSPACE:
            if (a->cursor > 0) { del_range(a, prev_b(a->buf, a->cursor), a->cursor); }
            break;
        case SC_KEY_DELETE:
            if (a->cursor < a->len) { del_range(a, a->cursor, next_b(a->buf, a->len, a->cursor)); }
            break;
        case SC_KEY_LEFT:      a->cursor = prev_b(a->buf, a->cursor); break;
        case SC_KEY_RIGHT:     a->cursor = next_b(a->buf, a->len, a->cursor); break;
        case SC_KEY_UP:        move_vertical(a, -1); break;
        case SC_KEY_DOWN:      move_vertical(a, +1); break;
        case SC_KEY_HOME:      a->cursor = line_start(a, a->cursor); break;
        case SC_KEY_END:       a->cursor = line_end(a, a->cursor); break;
        default: break;
    }
}

ScInputStatus sc_textarea(const char *prompt, char **out, ScTextareaOpts opts) {
    if (!prompt || !out) { return SC_INPUT_ERROR; }
    sc_theme_apply_textarea(&opts);

    Textarea a = { .prompt = prompt, .opts = opts };
    size_t n = opts.initial ? strlen(opts.initial) : 0;
    a.cap = n + 32;
    a.buf = malloc(a.cap);
    if (!a.buf) { return SC_INPUT_ERROR; }
    if (n) { memcpy(a.buf, opts.initial, n); }
    a.buf[n] = '\0'; a.len = n; a.cursor = n;

    ScPromptVTable vt = { .render = ta_render, .on_key = ta_on_key };
    ScInputStatus status = sc_prompt_run(&vt, &a);

    if (status == SC_INPUT_OK) {
        *out = a.buf;  /* hand ownership to the caller */
        if (!opts.hide_summary) {
            /* Count lines for a compact summary (don't echo the whole body). */
            size_t lines = a.len ? 1 : 0;
            for (size_t i = 0; i < a.len; i++) { if (a.buf[i] == '\n') { lines++; } }
            char line[96];
            snprintf(line, sizeof line, "? %s (%zu line%s)",
                     prompt, lines, lines == 1 ? "" : "s");
            sc_println(line, opts.summary_style);
        }
        return SC_INPUT_OK;
    }
    free(a.buf);
    return status;
}
