#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** A single- or multi-choice selection prompt. */
struct ScSelect {
    char **items;
    bool *checked;          /**< Parallel to `items`; used in multi-select. */
    size_t count;
    size_t cap;
    ScSelectOpts opts;
    ScColor accent;
    int max_visible;
    size_t cursor;
    size_t top;            /**< First visible row (scroll offset). */
};

static const char *const HINT_SINGLE =
    "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 enter select \xc2\xb7 esc cancel";
static const char *const HINT_MULTI =
    "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 space toggle \xc2\xb7 "
    "enter confirm \xc2\xb7 esc cancel";


static void copy_opts_strings(ScSelect *self);
static void free_opts_strings(ScSelect *self);
static ScRendered *select_render(void *state);
    static int format_scroll_hint(char *buf, size_t size, size_t top,
                                  size_t end, size_t count);
    static int select_row_width(ScSelect *self, size_t i, bool on_cursor,
                                const char *cursor_marker, const char *marker,
                                const char *checkbox_on,
                                const char *checkbox_off);
static void select_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static void reframe(ScSelect *self);


ScSelect *sc_select_new(ScSelectOpts opts) {
    sc_theme_apply_select(&opts);
    ScSelect *self = calloc(1, sizeof *self);
    if (!self) {
        return NULL;
    }
    self->opts = opts;
    copy_opts_strings(self);
    self->accent = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN;
    self->max_visible = opts.max_visible > 0 ? opts.max_visible : 10;
    return self;
}

void sc_select_add(ScSelect *self, const char *label) {
    if (!self) {
        return;
    }
    if (self->count == self->cap) {
        size_t cap = self->cap ? self->cap * 2 : 8;
        char **items = realloc(self->items, cap * sizeof *items);
        bool *checked = realloc(self->checked, cap * sizeof *checked);
        if (!items || !checked) {
            free(items);
            free(checked);
            return;
        }
        self->items = items;
        self->checked = checked;
        self->cap = cap;
    }
    self->items[self->count] = sc_dup_str(label);
    self->checked[self->count] = false;
    self->count++;
}

void sc_select_free(ScSelect *self) {
    if (!self) {
        return;
    }
    for (size_t i = 0; i < self->count; i++) {
        free(self->items[i]);
    }
    free(self->items);
    free(self->checked);
    free_opts_strings(self);
    free(self);
}

void sc_select_set_cursor(ScSelect *self, size_t index) {
    if (!self || index >= self->count) {
        return;
    }
    self->cursor = index;
    reframe(self);
}

void sc_select_set_checked(ScSelect *self, size_t index, bool on) {
    if (self && index < self->count) {
        self->checked[index] = on;
    }
}

size_t sc_select_cursor(const ScSelect *self) {
    return self ? self->cursor : 0;
}

const char *sc_select_label(const ScSelect *self, size_t index) {
    if (!self || index >= self->count) {
        return NULL;
    }
    return self->items[index];
}

void sc_select_set_label(ScSelect *self, size_t index, const char *label) {
    if (!self || index >= self->count) {
        return;
    }
    char *copy = sc_dup_str(label);
    if (!copy) {
        return;   // keep the old label on allocation failure
    }
    free(self->items[index]);
    self->items[index] = copy;
}

void sc_select_remove(ScSelect *self, size_t index) {
    if (!self || index >= self->count) {
        return;
    }
    free(self->items[index]);
    size_t tail = self->count - index - 1;
    memmove(&self->items[index], &self->items[index + 1],
            tail * sizeof *self->items);
    memmove(&self->checked[index], &self->checked[index + 1],
            tail * sizeof *self->checked);
    self->count--;
    if (self->cursor >= self->count) {
        self->cursor = self->count ? self->count - 1 : 0;
    }
    reframe(self);   // keep the (possibly clamped) cursor within the viewport
}

ScInputStatus sc_select_run(ScSelect *self, size_t *indices, size_t *count_io) {
    if (!self || !indices || !count_io || self->count == 0) {
        return SC_INPUT_ERROR;
    }

    ScPromptVTable vtable = {
        .render = select_render,
        .on_key = select_on_key,
    };
    ScPromptShortcuts sk = {
        self->opts.shortcuts, self->opts.n_shortcuts, self->opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, self, self->opts.shortcuts ? &sk : NULL, NULL);
    if (status != SC_INPUT_OK) {
        return status;
    }

    size_t cap = *count_io;
    size_t written = 0;
    const char *prompt = self->opts.prompt ? self->opts.prompt : "selection";
    char summary[256];
    if (self->opts.multi) {
        for (size_t i = 0; i < self->count && written < cap; i++) {
            if (self->checked[i]) {
                indices[written++] = i;
            }
        }
        snprintf(
            summary, sizeof summary, "? %s - %zu selected", prompt, written
        );
    } else if (self->count > 0) {
        if (cap >= 1) {
            indices[0] = self->cursor;
            written = 1;
        }
        snprintf(summary, sizeof summary, "? %s %s", prompt,
                 self->items[self->cursor]);
    } else {
        // All items were removed via a shortcut callback: nothing to select.
        snprintf(summary, sizeof summary, "? %s (none)", prompt);
    }
    if (!self->opts.hide_summary) {
        sc_println(summary, self->opts.summary_style);
    }
    *count_io = written;
    return SC_INPUT_OK;
}

ScRendered *sc_select_frame(ScSelect *self) {
    return select_render(self);
}

/**
 * Replaces the borrowed string fields of `self->opts` with heap copies, so
 * the prompt honors the "opts are copied internally" contract: the caller's
 * strings only need to live until `sc_select_new` returns. A failed copy
 * (OOM) leaves the field NULL, which falls back to the built-in default.
 * `shortcuts` and `prompt_text` stay borrowed (documented per-field).
 */
static void copy_opts_strings(ScSelect *self) {
    ScSelectOpts *opts = &self->opts;
    opts->prompt = sc_dup_opt_str(opts->prompt);
    opts->cursor_marker = sc_dup_opt_str(opts->cursor_marker);
    opts->marker = sc_dup_opt_str(opts->marker);
    opts->checkbox_on = sc_dup_opt_str(opts->checkbox_on);
    opts->checkbox_off = sc_dup_opt_str(opts->checkbox_off);
    opts->hint = sc_dup_opt_str(opts->hint);
}

/** Releases the opts strings duplicated by `copy_opts_strings`. */
static void free_opts_strings(ScSelect *self) {
    ScSelectOpts *opts = &self->opts;
    free((char *)opts->prompt);
    free((char *)opts->cursor_marker);
    free((char *)opts->marker);
    free((char *)opts->checkbox_on);
    free((char *)opts->checkbox_off);
    free((char *)opts->hint);
}

/** Visible width of one row: marker + optional checkbox + label. */
static int select_row_width(ScSelect *self, size_t i, bool on_cursor,
                            const char *cursor_marker, const char *marker,
                            const char *checkbox_on, const char *checkbox_off) {
    const char *mk = on_cursor ? cursor_marker : marker;
    int width = (int)sc_utf8_string_length(mk, strlen(mk));
    if (self->opts.multi) {
        const char *box = self->checked[i] ? checkbox_on : checkbox_off;
        width += (int)sc_utf8_string_length(box, strlen(box));
    }
    width += (int)sc_utf8_string_length(self->items[i], strlen(self->items[i]));
    return width;
}

static ScRendered *select_render(void *state) {
    ScSelect *self = state;
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    ScTextStyle selected_style = sc_style_set(self->opts.selected_style)
        ? self->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    const char *cursor_marker = self->opts.cursor_marker
        ? self->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *marker = self->opts.marker ? self->opts.marker : "  ";
    const char *checkbox_on = self->opts.checkbox_on
        ? self->opts.checkbox_on : "[x] ";
    const char *checkbox_off = self->opts.checkbox_off
        ? self->opts.checkbox_off : "[ ] ";

    size_t visible = (size_t)self->max_visible;
    size_t end = self->top + visible;
    if (end > self->count) {
        end = self->count;
    }

    bool have_prompt = self->opts.prompt || self->opts.prompt_text;
    int prompt_w = have_prompt
        ? sc_prompt_width(self->opts.prompt, self->opts.prompt_style,
                          self->opts.prompt_markup, self->opts.prompt_text)
        : 0;
    char scroll[80];
    int scroll_w = format_scroll_hint(scroll, sizeof scroll, self->top, end,
                                      self->count);

    /* Widest line drives the box width; the box maps it through its width
     * mode (default = content) to the interior width rows fill to. */
    int content_w = prompt_w > scroll_w ? prompt_w : scroll_w;
    for (size_t i = self->top; i < end; i++) {
        int w = select_row_width(self, i, i == self->cursor, cursor_marker,
                                 marker, checkbox_on, checkbox_off);
        if (w > content_w) { content_w = w; }
    }
    ScBoxLayout layout = sc_box_layout(self->opts.box, content_w,
                                       sc_terminal_width(), SC_WIDTH_CONTENT);
    bool fill = self->opts.box.bg_extent != SC_BG_EXTENT_TEXT;

    if (have_prompt) {
        sc_prompt_append(text, self->opts.prompt, self->opts.prompt_style,
                         self->opts.prompt_markup, self->opts.prompt_text);
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
    }

    for (size_t i = self->top; i < end; i++) {
        bool on_cursor = (i == self->cursor);
        ScTextStyle row = on_cursor ? selected_style : (ScTextStyle){ 0 };
        int used = select_row_width(self, i, on_cursor, cursor_marker, marker,
                                    checkbox_on, checkbox_off);

        sc_text_append(text, on_cursor ? cursor_marker : marker, row);
        if (self->opts.multi) {
            sc_text_append(text, self->checked[i] ? checkbox_on : checkbox_off,
                           row);
        }
        sc_text_append(text, self->items[i], row);
        /* Extend the cursor highlight into a full-width bar when it carries a
         * background and bg_extent fills the widget width. Other rows inherit
         * the widget background from the panel fill (when boxed). */
        if (on_cursor && fill && selected_style.bg.index != 0) {
            sc_pad_line_to(text, used, layout.interior_w, selected_style);
        }
        if (i + 1 < end) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
        }
    }

    if (scroll_w > 0) {
        sc_text_append(text, "\n", (ScTextStyle){ 0 });
        sc_text_append(text, scroll, (ScTextStyle){ SC_TEXT_ATTR_DIM,
                       SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    }

    const char *hint = self->opts.hint ? self->opts.hint
                     : (self->opts.multi ? HINT_MULTI : HINT_SINGLE);

    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    if (layout.active) {
        ScRendered *framed = sc_capture_panel_rendered(body, layout.panel);
        if (framed) {
            sc_rendered_free(body);
            body = framed;
        }
    }
    return sc_compose_hint(body, hint, self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/**
 * Formats the dim "↑ first-last/total ↓" scroll indicator into `buf` and
 * returns its visible width, or 0 when the list fits the viewport (no
 * indicator). Shown so a long list never looks silently truncated.
 */
static int format_scroll_hint(char *buf, size_t size, size_t top, size_t end,
                              size_t count) {
    if (top == 0 && end >= count) {
        return 0;
    }
    snprintf(buf, size, "  %s %zu-%zu/%zu %s",
             top > 0     ? "\xe2\x86\x91" : " ",   /* ↑ */
             top + 1, end, count,
             end < count ? "\xe2\x86\x93" : " ");  /* ↓ */
    return (int)sc_utf8_string_length(buf, strlen(buf));
}

/** Moves the cursor up one row, wrapping to the last row (unless no_wrap). */
static void cursor_up(ScSelect *self) {
    if (self->cursor > 0) {
        self->cursor--;
    } else if (!self->opts.no_wrap && self->count > 0) {
        self->cursor = self->count - 1;
    }
}

/** Moves the cursor down one row, wrapping to the first row (unless no_wrap). */
static void cursor_down(ScSelect *self) {
    if (self->cursor + 1 < self->count) {
        self->cursor++;
    } else if (!self->opts.no_wrap && self->count > 0) {
        self->cursor = 0;
    }
}

static void select_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ScSelect *self = state;
    switch (key.type) {
        case SC_KEY_UP:
            cursor_up(self);
            break;
        case SC_KEY_DOWN:
            cursor_down(self);
            break;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            if (key.mods != 0) { return; }   // Ctrl/Alt + char isn't j/k/space
            if (key.bytes[0] == 'k') {
                cursor_up(self);
            } else if (key.bytes[0] == 'j') {
                cursor_down(self);
            } else if (key.bytes[0] == ' ' && self->opts.multi) {
                self->checked[self->cursor] = !self->checked[self->cursor];
            }
            break;
        default:
            return;
    }
    reframe(self);
}

/** Keeps the cursor row within the visible window. */
static void reframe(ScSelect *self) {
    size_t visible = (size_t)self->max_visible;
    if (self->cursor < self->top) {
        self->top = self->cursor;
    } else if (self->cursor >= self->top + visible) {
        self->top = self->cursor - visible + 1;
    }
}
