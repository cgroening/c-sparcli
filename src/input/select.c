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

static ScRendered *select_render(void *state) {
    ScSelect *s = state;
    ScText *t = sc_text_new();
    if (!t) { return NULL; }

    if (s->opts.prompt) {
        sc_text_append(t, s->opts.prompt, s->opts.prompt_style);
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
    }

    size_t visible = (size_t)s->max_visible;
    size_t end = s->top + visible;
    if (end > s->count) { end = s->count; }

    for (size_t i = s->top; i < end; i++) {
        bool on_cursor = (i == s->cursor);
        ScTextStyle row = on_cursor
            ? (ScTextStyle){ SC_TEXT_ATTR_BOLD, s->accent, SC_ANSI_COLOR_NONE }
            : (ScTextStyle){ 0 };

        sc_text_append(t, on_cursor ? "\xe2\x80\xa3 " : "  ", row);  /* ‣ */
        if (s->opts.multi) {
            sc_text_append(t, s->checked[i] ? "[x] " : "[ ] ", row);
        }
        sc_text_append(t, s->items[i], row);
        if (i + 1 < end) { sc_text_append(t, "\n", (ScTextStyle){ 0 }); }
    }

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
    if (s->opts.multi) {
        for (size_t i = 0; i < s->count && written < cap; i++) {
            if (s->checked[i]) { indices[written++] = i; }
        }
        printf("? %s — %zu selected\n",
               s->opts.prompt ? s->opts.prompt : "selection", written);
    } else {
        if (cap >= 1) { indices[0] = s->cursor; written = 1; }
        printf("? %s %s\n",
               s->opts.prompt ? s->opts.prompt : "selection",
               s->items[s->cursor]);
    }
    *count_io = written;
    return SC_INPUT_OK;
}
