#include "input_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


struct ScSelect {
    char       **items;
    bool        *checked;   /* parallel to items; used when multi */
    size_t       count;
    size_t       cap;
    ScSelectOpts opts;
    ScColor      accent;
    int          max_visible;
    size_t       cursor;
    size_t       top;       /* first visible row (scroll offset) */
};


static char *dup_str(const char *s) {
    size_t n = strlen(s ? s : "");
    char  *p = malloc(n + 1);
    if (p) { memcpy(p, s ? s : "", n + 1); }
    return p;
}

ScSelect *sc_select_new(ScSelectOpts opts) {
    sc_theme_apply_select(&opts);
    ScSelect *s = calloc(1, sizeof *s);
    if (!s) { return NULL; }
    s->opts        = opts;
    s->accent      = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN;
    s->max_visible = opts.max_visible > 0 ? opts.max_visible : 10;
    return s;
}

void sc_select_add(ScSelect *s, const char *label) {
    if (!s) { return; }
    if (s->count == s->cap) {
        size_t cap = s->cap ? s->cap * 2 : 8;
        char **it  = realloc(s->items, cap * sizeof *it);
        bool  *ck  = realloc(s->checked, cap * sizeof *ck);
        if (!it || !ck) { free(it); free(ck); return; }
        s->items = it; s->checked = ck; s->cap = cap;
    }
    s->items[s->count]   = dup_str(label);
    s->checked[s->count] = false;
    s->count++;
}

void sc_select_free(ScSelect *s) {
    if (!s) { return; }
    for (size_t i = 0; i < s->count; i++) { free(s->items[i]); }
    free(s->items);
    free(s->checked);
    free(s);
}

/** Keeps the cursor row within the visible window. */
static void reframe(ScSelect *s) {
    size_t visible = (size_t)s->max_visible;
    if (s->cursor < s->top)                 { s->top = s->cursor; }
    else if (s->cursor >= s->top + visible)  { s->top = s->cursor - visible + 1; }
}

/**
 * Appends a dim "↑ first–last/total ↓" line when the list scrolls beyond the
 * viewport, so a long list never looks silently truncated.
 */
static void append_scroll_hint(ScText *t, size_t top, size_t end, size_t count) {
    if (top == 0 && end >= count) { return; }
    char buf[80];
    snprintf(buf, sizeof buf, "  %s %zu\xe2\x80\x93%zu/%zu %s",
             top > 0      ? "\xe2\x86\x91" : " ",          /* ↑ */
             top + 1, end, count,
             end < count  ? "\xe2\x86\x93" : " ");         /* ↓ */
    sc_text_append(t, "\n", (ScTextStyle){ 0 });
    sc_text_append(t, buf,
        (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
}

static ScRendered *select_render(void *state) {
    ScSelect *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    if (s->opts.prompt) {
        sc_text_append(t, s->opts.prompt, s->opts.prompt_style);
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
    }

    ScTextStyle sel = sc_style_set(s->opts.selected_style)
        ? s->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, s->accent, SC_ANSI_COLOR_NONE };
    const char *cur_mk = s->opts.cursor_marker ? s->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *mk     = s->opts.marker        ? s->opts.marker        : "  ";
    const char *cb_on  = s->opts.checkbox_on   ? s->opts.checkbox_on   : "[x] ";
    const char *cb_off = s->opts.checkbox_off  ? s->opts.checkbox_off  : "[ ] ";

    size_t visible = (size_t)s->max_visible;
    size_t end = s->top + visible;
    if (end > s->count) { end = s->count; }

    for (size_t i = s->top; i < end; i++) {
        bool on_cursor = (i == s->cursor);
        ScTextStyle row = on_cursor ? sel : (ScTextStyle){ 0 };

        sc_text_append(t, on_cursor ? cur_mk : mk, row);
        if (s->opts.multi) {
            sc_text_append(t, s->checked[i] ? cb_on : cb_off, row);
        }
        sc_text_append(t, s->items[i], row);
        if (i + 1 < end) { sc_text_append(t, "\n", (ScTextStyle){ 0 }); }
    }

    append_scroll_hint(t, s->top, end, s->count);

    const char *hint = s->opts.hint ? s->opts.hint : (s->opts.multi
        ? "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 space toggle \xc2\xb7 enter confirm \xc2\xb7 esc cancel"
        : "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 enter select \xc2\xb7 esc cancel");
    sc_append_hint(t, hint, s->opts.hide_hint, s->opts.hint_style);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static void select_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ScSelect *s = state;
    switch (key.type) {
        case SC_KEY_UP:
            if (s->cursor > 0) { s->cursor--; }
            break;
        case SC_KEY_DOWN:
            if (s->cursor + 1 < s->count) { s->cursor++; }
            break;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            if (key.bytes[0] == 'k' && s->cursor > 0) { s->cursor--; }
            else if (key.bytes[0] == 'j' && s->cursor + 1 < s->count) { s->cursor++; }
            else if (key.bytes[0] == ' ' && s->opts.multi) {
                s->checked[s->cursor] = !s->checked[s->cursor];
            }
            break;
        default:
            return;
    }
    reframe(s);
}

ScInputStatus sc_select_run(ScSelect *s, size_t *indices, size_t *count_io) {
    if (!s || !indices || !count_io || s->count == 0) { return SC_INPUT_ERROR; }

    ScPromptVTable vt = { .render = select_render, .on_key = select_on_key };
    ScInputStatus status = sc_prompt_run(&vt, s);
    if (status != SC_INPUT_OK) { return status; }

    size_t cap = *count_io;
    size_t written = 0;
    const char *prompt = s->opts.prompt ? s->opts.prompt : "selection";
    char summary[256];
    if (s->opts.multi) {
        for (size_t i = 0; i < s->count && written < cap; i++) {
            if (s->checked[i]) { indices[written++] = i; }
        }
        snprintf(summary, sizeof summary, "? %s — %zu selected", prompt, written);
    } else {
        if (cap >= 1) { indices[0] = s->cursor; written = 1; }
        snprintf(summary, sizeof summary, "? %s %s", prompt, s->items[s->cursor]);
    }
    if (!s->opts.hide_summary) { sc_println(summary, s->opts.summary_style); }
    *count_io = written;
    return SC_INPUT_OK;
}

ScRendered *sc_select_frame(ScSelect *s) {
    return select_render(s);
}

void sc_select_set_cursor(ScSelect *s, size_t index) {
    if (!s || index >= s->count) { return; }
    s->cursor = index;
    reframe(s);
}

void sc_select_set_checked(ScSelect *s, size_t index, bool on) {
    if (s && index < s->count) { s->checked[index] = on; }
}
