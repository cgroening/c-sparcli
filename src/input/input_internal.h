#pragma once

#include "sparcli.h"
#include "tty/tty_internal.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>


/**
 * @file input_internal.h
 * @brief Shared engine + line editor behind every input widget.
 *
 * The widgets are thin: they own a small state struct and provide two
 * callbacks (render a frame, handle a key). `sc_prompt_run` drives the raw
 * mode, the in-place redraw loop, and the cleanup. Frames are built with
 * the ordinary output renderers (`ScText` + `sc_capture_text`), so the
 * input side reuses the entire output stack as its "view" layer.
 */


/**
 * Returns `true` when a style carries any formatting. Used by every widget
 * to decide whether a caller-supplied `*_style` overrides the built-in
 * default (zero-init `ScTextStyle` = "use default"). Self-contained so the
 * input layer needs no `internal.h` dependency.
 */
static inline bool sc_style_set(ScTextStyle style) {
    return style.attr != 0 || style.fg.index != 0 || style.bg.index != 0;
}

/**
 * Heap-duplicates a NUL-terminated string (`malloc` + `memcpy`); NULL input
 * duplicates the empty string. Returns NULL on allocation failure. Shared by
 * every widget that hands heap strings to the caller (text input, number
 * input, select labels, fuzzy rows, ...).
 */
static inline char *sc_dup_str(const char *str) {
    const char *src = str ? str : "";
    size_t size = strlen(src) + 1;
    char *copy = malloc(size);
    if (copy) {
        memcpy(copy, src, size);
    }
    return copy;
}

/**
 * Like `sc_dup_str`, but preserves NULL. Used when copying optional opts
 * strings where NULL means "use the built-in default" (prompt, markers,
 * hint, ...), so the distinction survives the copy.
 */
static inline char *sc_dup_opt_str(const char *str) {
    return str ? sc_dup_str(str) : NULL;
}

/** Resolves the zero-init `DEFAULT` layout to the built-in default (inline). */
static inline ScHintLayout sc_hint_resolved(ScHintLayout layout) {
    return layout == SC_HINT_LAYOUT_DEFAULT ? SC_HINT_INLINE : layout;
}

/** Resolves the zero-init `DEFAULT` position to the default (bottom). */
static inline ScHintPosition sc_hint_pos_resolved(ScHintPosition pos) {
    return pos == SC_HINT_POS_DEFAULT ? SC_HINT_POS_BOTTOM : pos;
}


/* ── Box framing (ScBoxStyle) ───────────────────────────────────────────── */

/**
 * Resolves a box's inner padding: an all-zero `ScEdges` selects the built-in
 * default of one column left and right (so a zero-init box keeps the historic
 * boxed look); any non-zero value is used verbatim.
 */
static inline ScEdges sc_box_padding(ScEdges padding) {
    if (padding.top == 0 && padding.right == 0 &&
        padding.bottom == 0 && padding.left == 0) {
        return (ScEdges){ .left = 1, .right = 1 };
    }
    return padding;
}

/** Returns true when `edges` has any non-zero side. */
static inline bool sc_edges_any(ScEdges edges) {
    return edges.top != 0 || edges.right != 0 ||
           edges.bottom != 0 || edges.left != 0;
}

/**
 * Resolved layout for a box: the `ScPanelOpts` to frame the body with, whether
 * the box does anything at all (`active`), and the interior width that content
 * lines should be padded to when filling backgrounds to the widget width.
 */
typedef struct ScBoxLayout {
    ScPanelOpts panel;     /**< Ready for `sc_capture_panel_rendered`. */
    bool        active;    /**< False = render the body unframed. */
    int         interior_w;/**< Width rows pad to (bg_extent = WIDGET). */
} ScBoxLayout;

/**
 * Resolves a box's width mode, border and panel opts. `content_w` is the
 * natural (unpadded) content width; `default_mode` is used when the box leaves
 * `width_mode` at `SC_WIDTH_DEFAULT` and `width` is 0 (lists pass
 * `SC_WIDTH_CONTENT`, text-entry widgets `SC_WIDTH_FULL`).
 *
 * The border is drawn when `enabled` or an explicit `border.type`; `enabled`
 * with a NONE type defaults to rounded, otherwise NONE stays borderless. A bg,
 * padding, margin or any width constraint activates the (possibly borderless)
 * panel on its own.
 */
static inline ScBoxLayout sc_box_layout(
    ScBoxStyle box, int content_w, int term_w, ScWidthMode default_mode
) {
    ScWidthMode mode = box.width_mode;
    if (mode == SC_WIDTH_DEFAULT) {
        mode = box.width > 0 ? SC_WIDTH_FIXED : default_mode;
    }

    bool border_drawn = box.enabled || box.border.type != SC_BORDER_NONE;
    ScBorderStyle border = box.border;
    if (border_drawn && border.type == SC_BORDER_NONE) {
        border.type = SC_BORDER_ROUNDED;
    }

    ScBoxLayout layout = { 0 };
    layout.active = box.enabled || box.border.type != SC_BORDER_NONE
        || box.bg.index != 0 || sc_edges_any(box.padding)
        || sc_edges_any(box.margin) || box.width > 0
        || box.width_mode != SC_WIDTH_DEFAULT
        || box.min_width > 0 || box.max_width > 0;

    ScEdges padding = layout.active ? sc_box_padding(box.padding)
                                    : (ScEdges){ 0 };
    int overhead = (border_drawn ? 2 : 0) + padding.left + padding.right;

    ScPanelOpts panel = {
        .border        = border,
        .bg            = box.bg,
        .padding       = padding,
        .margin        = box.margin,
        .content_align = SC_ALIGN_LEFT,
    };

    int interior;
    switch (mode) {
    case SC_WIDTH_FIXED: {
        int outer = box.width > 0 ? box.width : content_w + overhead;
        panel.width = outer;
        interior = outer - overhead;
        break;
    }
    case SC_WIDTH_FULL: {
        panel.full_width = true;
        /* The full-width panel draws its frame across the whole terminal (minus
           margins): inner = term - 2 - margins, content = inner - padding. Match
           that here so the interior width handed to the body equals the panel's
           real content area (a `term_w - 2` here double-counted the border). */
        int outer = term_w - box.margin.left - box.margin.right;
        interior = outer - overhead;
        break;
    }
    case SC_WIDTH_CONTENT:
    default:
        interior = content_w;
        if (box.min_width > 0 && interior < box.min_width) {
            interior = box.min_width;
        }
        if (box.max_width > 0 && interior > box.max_width) {
            interior = box.max_width;
        }
        /* Pure content auto-fits; a min/max clamp pins the panel width. */
        if (box.min_width > 0 || box.max_width > 0) {
            panel.width = interior + overhead;
        }
        break;
    }
    if (interior < 1) { interior = 1; }
    layout.interior_w = interior;
    layout.panel = panel;
    return layout;
}

/**
 * Left offset of a box's content area: `margin.left + border + left padding`.
 * Used to indent the hint footer so it lines up under the framed content.
 * Returns 0 when the box is inactive (inline widget). Mirrors `sc_box_layout`'s
 * activation test and border/padding resolution.
 */
static inline int sc_box_content_left(ScBoxStyle box) {
    bool active = box.enabled || box.border.type != SC_BORDER_NONE
        || box.bg.index != 0 || sc_edges_any(box.padding)
        || sc_edges_any(box.margin) || box.width > 0
        || box.width_mode != SC_WIDTH_DEFAULT
        || box.min_width > 0 || box.max_width > 0;
    if (!active) {
        return 0;
    }
    bool border_drawn = box.enabled || box.border.type != SC_BORDER_NONE;
    ScEdges padding = sc_box_padding(box.padding);
    return box.margin.left + (border_drawn ? 1 : 0) + padding.left;
}

/**
 * Physical rows the engine's labeled-shortcut footer occupies after terminal
 * soft-wrap: the footer is one logical line (`name SP label`, joined by `  ·  `)
 * of visible width `indent + …`; the terminal wraps it to `ceil(width / term_w)`
 * rows. Returns 0 when no shortcut carries a `hint_label`. Widgets reserve this
 * in their height budget (and as the fullscreen bottom-reserve) so a long footer
 * that wraps stays fully on screen. Defined in shortcut.c (needs the chord
 * naming + width helpers). Mirrors `build_shortcut_hint`.
 */
int sc_shortcut_hint_rows(const ScShortcut *items, size_t n,
                          int indent, int term_w);

/**
 * Defines a `static int name(void *st)` that returns the box content-left for a
 * widget whose state struct exposes `opts.box`. Wire it into the vtable's
 * `hint_indent` so the engine aligns the labeled-shortcut footer with the
 * framed content. (Text-entry keeps the box on `state->box` and defines its
 * own accessor.)
 */
#define SC_DEFINE_HINT_INDENT(name, type) \
    static int name(void *st) { \
        return sc_box_content_left(((type *)st)->opts.box); \
    }

/**
 * Prepends `left` spaces to every line of `r` (updating the per-line and max
 * widths), so a captured block lines up under a framed content column. Mutates
 * and returns `r`; `left <= 0` or NULL `r` is a no-op. On a per-line allocation
 * failure that line is left unchanged.
 */
static inline ScRendered *sc_indent_rendered(ScRendered *r, int left) {
    if (!r || left <= 0) {
        return r;
    }
    for (size_t i = 0; i < r->line_count; i++) {
        const char *old = r->lines[i] ? r->lines[i] : "";
        size_t old_len = strlen(old);
        char *line = malloc((size_t)left + old_len + 1);
        if (!line) {
            continue;
        }
        memset(line, ' ', (size_t)left);
        memcpy(line + left, old, old_len + 1);
        free(r->lines[i]);
        r->lines[i] = line;
        if (r->column_widths) {
            r->column_widths[i] += left;
        }
    }
    r->max_column_width += left;
    return r;
}

/**
 * Appends `target_w - used_cols` background-styled spaces to `text`, extending
 * a line to the widget width. No-op when the line already meets the width.
 * Used to fill row backgrounds / highlight bars to the full widget width.
 */
static inline void sc_pad_line_to(
    ScText *text, int used_cols, int target_w, ScTextStyle bg
) {
    int pad = target_w - used_cols;
    if (pad <= 0) {
        return;
    }
    char *spaces = malloc((size_t)pad + 1);
    if (!spaces) {
        return;
    }
    memset(spaces, ' ', (size_t)pad);
    spaces[pad] = '\0';
    sc_text_append(text, spaces, bg);
    free(spaces);
}

/**
 * Frames an already-composed `body` per `box`, consuming `body` and returning
 * the framed block (or `body` unchanged when the box is inactive / on failure).
 * For widgets whose prompt stays inside the body and that need no per-row
 * background fill (confirm, datepicker). `default_mode` picks the width when
 * the box leaves it at `SC_WIDTH_DEFAULT`. select/fuzzy use `sc_box_layout`
 * directly so they can pad the cursor row to the interior width.
 */
static inline ScRendered *sc_box_wrap(
    ScRendered *body, ScBoxStyle box, ScWidthMode default_mode, int term_w
) {
    if (!body) {
        return body;
    }
    int content_w = body->max_column_width;
    ScBoxLayout layout = sc_box_layout(box, content_w, term_w, default_mode);
    if (!layout.active) {
        return body;
    }
    ScRendered *framed = sc_capture_panel_rendered(body, layout.panel);
    if (!framed) {
        return body;
    }
    sc_rendered_free(body);
    return framed;
}


/* ── Rich prompt resolver ────────────────────────────────────────────────── */
/*
 * A widget prompt can be supplied three ways, in precedence order:
 *   1. `rich`  - a caller-built ScText (mixed styles; no escaping needed),
 *   2. `markup`- parse the plain string as inline markup ("[italic]x[/]"),
 *   3. plain   - the string rendered with a single `style`.
 * These helpers centralize that resolution so every widget (inline and boxed)
 * behaves identically. Public API only, so this header stays internal.h-free.
 */

/** Appends the resolved prompt spans into `dst`. */
static inline void sc_prompt_append(
    ScText *dst, const char *plain, ScTextStyle style,
    bool markup, const ScText *rich
) {
    if (rich) {
        size_t n = sc_text_span_count(rich);
        for (size_t i = 0; i < n; i++) {
            ScSpan span = sc_text_span(rich, i);
            if (span.raw_str) { sc_text_append(dst, span.raw_str, span.style); }
        }
    } else if (markup && plain) {
        sc_markup_append(dst, plain);
    } else if (plain) {
        sc_text_append(dst, plain, style);
    }
}

/** Builds a standalone resolved prompt `ScText` (caller frees), or NULL. */
static inline ScText *sc_prompt_build(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    ScText *text = sc_text_new();
    if (!text) { return NULL; }
    sc_prompt_append(text, plain, style, markup, rich);
    return text;
}

/**
 * Like `sc_prompt_build`, but fills each span's background with `bg` when the
 * span carries none, so a boxed prompt/title rendered on the panel border
 * inherits the box background instead of showing on the terminal default.
 * Returns the plain build unchanged when `bg` is inactive (index == 0).
 */
static inline ScText *sc_prompt_build_bg(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich,
    ScColor bg
) {
    ScText *src = sc_prompt_build(plain, style, markup, rich);
    if (!src || bg.index == 0) { return src; }
    ScText *out = sc_text_new();
    if (!out) { return src; }
    size_t n = sc_text_span_count(src);
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(src, i);
        if (!span.raw_str) { continue; }
        ScTextStyle s = span.style;
        if (s.bg.index == 0) { s.bg = bg; }
        sc_text_append(out, span.raw_str, s);
    }
    sc_text_free(src);
    return out;
}

/** Visible column width of the resolved prompt. */
static inline int sc_prompt_width(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    if (rich) { return (int)sc_text_visible_width(rich); }
    if (!markup) {
        /* Fast path: a plain prompt's width is its UTF-8 codepoint count
         * (count non-continuation bytes), no allocation on the render path. */
        int width = 0;
        for (const unsigned char *p = (const unsigned char *)plain;
             plain && *p; p++) {
            if ((*p & 0xC0) != 0x80) { width++; }
        }
        return width;
    }
    ScText *text = sc_prompt_build(plain, style, markup, rich);
    int width = text ? (int)sc_text_visible_width(text) : 0;
    sc_text_free(text);
    return width;
}

/**
 * Returns a heap-allocated plain-text (style-stripped) version of the resolved
 * prompt, for one-line summaries. Caller frees. Never returns NULL for valid
 * input (empty string on OOM-free trivial paths).
 */
static inline char *sc_prompt_plain(
    const char *plain, ScTextStyle style, bool markup, const ScText *rich
) {
    if (!markup && !rich) {
        return strdup(plain ? plain : "");
    }
    ScText *text = sc_prompt_build(plain, style, markup, rich);
    size_t n = text ? sc_text_span_count(text) : 0;
    size_t total = 1;
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(text, i);
        if (span.raw_str) { total += strlen(span.raw_str); }
    }
    char *out = malloc(total);
    if (out) {
        size_t pos = 0;
        for (size_t i = 0; i < n; i++) {
            ScSpan span = sc_text_span(text, i);
            if (span.raw_str) {
                // Clamp to the allocation so the copy is provably in-bounds
                // even if span contents differed between the two passes.
                size_t len = strlen(span.raw_str);
                size_t remaining = total - 1 - pos;
                if (len > remaining) { len = remaining; }
                memcpy(out + pos, span.raw_str, len);
                pos += len;
            }
        }
        out[pos] = '\0';
    }
    sc_text_free(text);
    return out;
}

/**
 * Builds the key-hint footer as a standalone `ScRendered` per `layout`:
 * `SC_HINT_INLINE` is one line, `SC_HINT_STACKED` is one line per ` · `-
 * separated item. Returns NULL when hidden, empty, or on allocation failure.
 * Independent of placement - `sc_hint_place` positions the result.
 *
 * `hint` is the resolved string (widget default or caller override).
 */
static inline ScRendered *sc_hint_render(
    const char *hint, ScHintLayout layout, ScTextStyle style
) {
    layout = sc_hint_resolved(layout);
    if (layout == SC_HINT_HIDDEN || !hint || !hint[0]) {
        return NULL;
    }
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    ScTextStyle resolved = sc_style_set(style)
        ? style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };

    if (layout == SC_HINT_INLINE) {
        sc_text_append(text, hint, resolved);
    } else {
        /* Stacked: split the hint on " · " and emit one item per line. */
        const char *const separator = " \xc2\xb7 ";  /* space U+00B7 space */
        const size_t separator_len = 4;
        const char *cursor = hint;
        bool first = true;
        for (;;) {
            const char *hit = strstr(cursor, separator);
            size_t span = hit ? (size_t)(hit - cursor) : strlen(cursor);
            if (!first) {
                sc_text_append(text, "\n", (ScTextStyle){ 0 });
            }
            /* sc_text_append needs a NUL-terminated string; copy the item. */
            char *item = malloc(span + 1);
            if (item) {
                memcpy(item, cursor, span);
                item[span] = '\0';
                sc_text_append(text, item, resolved);
                free(item);
            }
            first = false;
            if (!hit) {
                break;
            }
            cursor = hit + separator_len;
        }
    }

    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/**
 * Places the hint block around the widget body per `pos` and returns the
 * composed frame. Top/bottom stack vertically; left/right sit beside the body,
 * top-aligned, with a small gap. Both `body` and `hint` are consumed (freed).
 * A NULL `hint` returns `body` unchanged.
 */
static inline ScRendered *sc_hint_place(
    ScRendered *body, ScRendered *hint, ScHintPosition pos
) {
    if (!hint) {
        return body;
    }
    pos = sc_hint_pos_resolved(pos);

    ScRendered *out = NULL;
    if (pos == SC_HINT_POS_TOP || pos == SC_HINT_POS_BOTTOM) {
        const ScRendered *parts[2];
        parts[0] = (pos == SC_HINT_POS_TOP) ? hint : body;
        parts[1] = (pos == SC_HINT_POS_TOP) ? body : hint;
        out = sc_vstack(parts, 2, 0);
    } else {
        ScColumns *cols = sc_columns_new((ScColumnsOpts){
            .gap = 2, .valign = SC_VALIGN_TOP,
            .sep = { .type = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE } });
        if (cols) {
            if (pos == SC_HINT_POS_LEFT) {
                sc_columns_add_rendered(cols, hint, (ScColItem){ 0 });
                sc_columns_add_rendered(cols, body, (ScColItem){ 0 });
            } else {
                sc_columns_add_rendered(cols, body, (ScColItem){ 0 });
                sc_columns_add_rendered(cols, hint, (ScColItem){ 0 });
            }
            out = sc_capture_columns(cols);
            sc_columns_free(cols);
        }
    }
    sc_rendered_free(body);
    sc_rendered_free(hint);
    return out;
}

/**
 * Composes a widget's `body` frame with its key-hint footer: builds the hint
 * per `layout`, then places it per `pos`. Consumes `body`; returns the final
 * frame. The single call every widget makes after assembling its body.
 *
 * `indent` left-pads the hint block (e.g. to line up under a framed content
 * column, via `sc_box_content_left`) for TOP/BOTTOM positions only; LEFT/RIGHT
 * hints sit beside the widget and ignore it. Pass 0 for no indent.
 */
static inline ScRendered *sc_compose_hint(
    ScRendered *body, const char *hint, ScHintLayout layout,
    ScHintPosition pos, ScTextStyle style, int indent
) {
    ScRendered *h = sc_hint_render(hint, layout, style);
    ScHintPosition rp = sc_hint_pos_resolved(pos);
    if (indent > 0 && (rp == SC_HINT_POS_TOP || rp == SC_HINT_POS_BOTTOM)) {
        h = sc_indent_rendered(h, indent);
    }
    return sc_hint_place(body, h, pos);
}

/**
 * Vertically stacks `bottom` beneath `top`, consuming both. Returns the
 * combined block, or `top` unchanged when `bottom` is NULL or stacking
 * fails. Shared by the widgets that stack error lines / extra blocks
 * under their body (text input, number input).
 */
static inline ScRendered *sc_stack_below(ScRendered *top, ScRendered *bottom) {
    if (!bottom) {
        return top;
    }
    const ScRendered *parts[2] = { top, bottom };
    ScRendered *stacked = sc_vstack(parts, 2, 0);
    if (!stacked) {
        sc_rendered_free(bottom);
        return top;
    }
    sc_rendered_free(top);
    sc_rendered_free(bottom);
    return stacked;
}

/**
 * Full-screen composition (prompt.c): returns `[top_pad blank][header][body]`,
 * padded so the (header + body) block sits top/middle/bottom within the terminal
 * height per `valign`. `header` is borrowed (may be NULL); `body` is consumed.
 * Used by the fuzzy/form fullscreen mode so the block grows + re-aligns each
 * frame as the body changes. Returns `body` unchanged when there is nothing to
 * add (no header and TOP / no free rows).
 *
 * `bottom_reserve` keeps that many rows free at the very bottom of the screen so
 * the engine's labeled-shortcut footer (stacked after `render`) is not pushed
 * off-screen: the block is aligned within `term_height - bottom_reserve`. Pass 0
 * when no footer is stacked.
 */
ScRendered *sc_fullscreen_compose(ScRendered *body, const ScRendered *header,
                                  ScVAlign valign, int bottom_reserve);


/* ── Prompt loop engine (prompt.c) ──────────────────────────────────────── */

/**
 * How a widget treats pasted (bracketed-paste) keys. Zero-init = IGNORE,
 * the safest default: widgets without a text field never see pasted keys,
 * so pasted characters cannot navigate, confirm or trigger anything.
 */
typedef enum ScPasteMode {
    SC_PASTE_IGNORE = 0,  /**< Drop all pasted keys (select, confirm, ...). */
    SC_PASTE_TEXT,        /**< Insert pasted chars; drop pasted Enter. */
    SC_PASTE_MULTILINE,   /**< Insert pasted chars and newlines (textarea). */
} ScPasteMode;

/** Per-widget behaviour passed to `sc_prompt_run`. */
typedef struct ScPromptVTable {
    /** Render the current state into a fresh `ScRendered` (engine frees it). */
    ScRendered *(*render)(void *state);

    /** Handle one key; set `*done` to accept, `*cancel` to abort. */
    void (*on_key)(void *state, ScKey key, bool *done, bool *cancel);

    /** Pasted-key policy; zero-init = `SC_PASTE_IGNORE`. */
    ScPasteMode paste;

    /**
     * Optional external-editor hooks (text_input / textarea only). `edit_get`
     * returns a heap copy of the current text (caller frees); `edit_set`
     * replaces the widget's text with the editor result. Both NULL = the
     * widget does not support editing in an external editor.
     */
    char *(*edit_get)(void *state);
    void  (*edit_set)(void *state, const char *text);

    /**
     * When true, the engine does NOT treat a bare `SC_KEY_ESC` as cancel: it
     * forwards Esc through the normal dispatch chain (shortcuts → on_key), so
     * the widget can repurpose Esc (e.g. leave a modal insert mode) and decide
     * itself when to set `*cancel`. Ctrl-C and EOF still always cancel.
     * Zero-init = false = today's behavior (Esc cancels).
     */
    bool capture_escape;

    /**
     * Optional predicate: when set and it returns true, the engine skips
     * shortcuts whose chord is a bare printable character (`SC_KEY_CHAR` with
     * no modifiers) so those keys reach `on_key` as text instead of firing.
     * Used by the modal fuzzy finder to suppress bare-letter shortcuts while in
     * insert mode. NULL / false = all shortcuts dispatch as usual.
     */
    bool (*suppress_char_shortcuts)(void *state);

    /**
     * Optional predicate: when set and it returns true for `key`, the engine
     * runs the external-editor action for that key — in addition to the
     * configured `ScPromptEditor.chord`. Lets a widget bind extra keys to the
     * editor (e.g. Enter on a multiline form field, alongside Ctrl-G). Only
     * consulted when an editor is enabled and `edit_get`/`edit_set` are set.
     * NULL = only the configured chord opens the editor.
     */
    bool (*wants_editor)(void *state, ScKey key);

    /**
     * Optional: columns to indent the engine's labeled-shortcut footer so it
     * lines up under the widget's framed content (typically
     * `sc_box_content_left(box)`). NULL / 0 = footer at column 0 (today's
     * behavior). The widget's own key-hint is indented separately via
     * `sc_compose_hint`'s `indent` argument.
     */
    int (*hint_indent)(void *state);
} ScPromptVTable;

/**
 * Optional external-editor binding consulted by the engine. Pass `NULL` to
 * `sc_prompt_run` when no editor is configured. Active only when `enabled` is
 * true and the vtable provides `edit_get`/`edit_set`.
 */
typedef struct ScPromptEditor {
    /** Enable the editor key (opt-in). */
    bool enabled;

    /** Editor command override; `NULL`/empty = $VISUAL/$EDITOR/nvim/vi. */
    const char *cmd;

    /** Key that opens the editor; zero-init = Ctrl-G. */
    ScKeyChord chord;
} ScPromptEditor;

/**
 * Runs `cmd` (resolved per the chain above) on a temp file seeded with
 * `initial`, on the controlling terminal. On a clean exit (status 0) the saved
 * contents are returned in `*out` (heap; caller frees) and the function returns
 * `true`; on a non-zero exit, signal, or any error it returns `false` and
 * `*out` is untouched. Shell-free (`execvp` with a whitespace-tokenized argv);
 * the temp file is mode 0600 and unlinked before returning. Does not touch
 * raw mode - the engine suspends/resumes the terminal around the call.
 */
bool sc_run_editor(const char *cmd, const char *initial, char **out);

/**
 * Optional custom shortcuts consulted by the engine before `on_key`, shared
 * by every widget. Built from the widget's `Sc*Opts.shortcuts` fields. Pass
 * `NULL` to `sc_prompt_run` when no shortcuts are configured.
 */
typedef struct ScPromptShortcuts {
    /** Borrowed shortcut array (must outlive the run). */
    const ScShortcut *items;

    /** Number of entries in `items`. */
    size_t count;

    /**
     * Optional out-param: set to `-1` before the loop, then to the fired
     * shortcut's `id` when a `SC_SHORTCUT_RETURN` shortcut ends the prompt.
     * Stays `-1` on a normal submit or cancel.
     */
    int *out_id;
} ScPromptShortcuts;

/**
 * Runs the interactive loop for one widget.
 *
 * Enters raw mode, then repeatedly renders the state, draws it in place,
 * reads a decoded key and dispatches it, until the widget signals done or
 * cancel (or EOF / Ctrl-C). Each key is checked against `shortcuts` (when
 * non-NULL) before `on_key`: a RETURN shortcut ends the loop and records its
 * id; a CALLBACK shortcut runs in place and keeps the loop running unless its
 * callback returns `false`. Esc / Ctrl-C stay reserved for cancel. Clears the
 * interactive region and restores the terminal before returning.
 *
 * @return `SC_INPUT_OK` on accept, `SC_INPUT_CANCELLED` on abort,
 *         `SC_INPUT_ERROR` when no terminal is available.
 */
ScInputStatus sc_prompt_run(
    const ScPromptVTable *vtable, void *state, ScPromptShortcuts *shortcuts,
    const ScPromptEditor *editor
);


/* ── Shared line editor (line_editor.c) ─────────────────────────────────── */

/**
 * A growable UTF-8 edit buffer with a byte-offset cursor. Shared by the
 * text/password inputs and the fuzzy finder's query field.
 */
typedef struct ScLineEditor {
    /** UTF-8 bytes, always NUL-terminated. */
    char *buf;

    /** Bytes used (excluding the NUL). */
    size_t len;

    /** Allocated capacity. */
    size_t cap;

    /** Cursor byte offset into `buf`. */
    size_t cursor;
} ScLineEditor;

/** Initializes `self` with an optional initial value (copied). */
void sc_le_init(ScLineEditor *self, const char *initial);

/** Releases the buffer owned by `self`. */
void sc_le_free(ScLineEditor *self);

/**
 * Applies an editing key (insert char, backspace, delete, cursor motion,
 * word/line kill). Returns `true` when the key was an editing action the
 * editor consumed, `false` for keys the widget should handle itself
 * (Enter, Esc, arrows the widget reserves, …).
 */
bool sc_le_handle(ScLineEditor *self, ScKey key);

/**
 * Appends the editor content to `text` as styled spans, including a
 * reverse-style block at the cursor position.
 *
 * @param cursor_style       Style of the cursor cell; when unset (zero-init)
 *                           falls back to black-on-white.
 * @param mask               When non-NULL, every character is rendered as
 *                           this glyph (password masking); "" hides content.
 * @param placeholder        Shown dim when the buffer is empty; may be NULL.
 * @param field_width        Visible width in columns; 0 = unlimited. When the
 *                           content is wider, a cursor-tracking window scrolls
 *                           horizontally and shows `‹`/`›` edge markers.
 */
void sc_le_render_into(
    const ScLineEditor *self, ScText *text,
    ScTextStyle value_style, ScTextStyle cursor_style, const char *mask,
    const char *placeholder, ScTextStyle placeholder_style, int field_width
);


/* ── Input history (history.c) ──────────────────────────────────────────── */

/**
 * Whether a text input should auto-add submitted lines to this history
 * (the handle's `no_auto_add` opt-out, NULL-safe). The recall navigation
 * itself lives in text_input.c; the handle is pure storage.
 */
bool sc_history_auto_add(const ScHistory *history);


/* ── Shared text-entry core (text_input.c) ──────────────────────────────── */

/**
 * Configuration for the shared single-line entry core. Drives both
 * `sc_text_input` (mask == NULL) and `sc_password_input` (mask != NULL), and
 * the snapshot builder. Zero-init fields select the built-in defaults.
 */
typedef struct ScTextEntryCfg {
    const char *prompt;

    /** Prefilled value (live) / shown value (frame). */
    const char *initial;
    const char *placeholder;

    /** NULL = plain text. */
    const char *mask;
    ScTextStyle prompt_style;
    ScTextStyle value_style;

    /** Zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Zero-init = red. */
    ScTextStyle error_style;

    /** Zero-init = dim. */
    ScTextStyle count_style;
    ScTextStyle summary_style;
    bool hide_summary;
    bool hide_char_count;

    /** 0 = unlimited. */
    int max_chars;

    /** Optional frame (border/bg/padding/margin) + field width via box.width
        (0 = terminal width; boxed = panel width). Zero-init = inline. */
    ScBoxStyle box;

    /** Key-hint footer; NULL = default. */
    const char *hint;
    ScHintLayout hint_layout;
    ScHintPosition hint_pos;
    ScTextStyle hint_style;
    ScCharFilter char_filter;
    void *char_filter_ctx;
    const char *const *suggestions;
    size_t n_suggestions;

    /** Suggestion presentation (ghost vs dropdown) + dropdown styling. */
    ScSuggestOpts suggest;

    /**
     * Frame-builder only: 1-based highlighted dropdown row for style
     * snapshots; 0 = no row highlighted. The live prompt manages the
     * highlight itself.
     */
    size_t suggest_cursor;
    bool (*validate)(const char *, void *, const char **);
    void *validate_ctx;

    /** Custom key shortcuts (borrowed) + optional fired-id out-param. */
    const ScShortcut *shortcuts;
    size_t n_shortcuts;
    int *out_shortcut_id;

    /** Rich prompt (overrides string prompt) + markup flag. */
    const ScText *prompt_text;
    bool prompt_markup;

    /** External-editor binding (text_input only; password leaves these off). */
    bool external_editor;
    const char *editor;
    ScKeyChord editor_key;

    /** Input history for Up/Down recall (borrowed; text_input only). */
    ScHistory *history;

    /** Auto-add submitted lines to `history` (resolved opt-outs). */
    bool history_auto;
} ScTextEntryCfg;

/**
 * Runs a single-line text prompt. On `SC_INPUT_OK`, `*out` receives a
 * heap-allocated copy of the value (caller frees).
 */
ScInputStatus sc_text_entry(const ScTextEntryCfg *cfg, char **out);


/* ── Snapshot frame builders (used by the non-interactive style tests) ───── */

/**
 * Each widget exposes a frame builder that runs its normal render path over
 * a constructed state and returns the captured `ScRendered`, so style tests
 * can show a widget in a given style without entering the interactive loop.
 * Print the result with `sc_pad_print(r, (ScPadOpts){0})`; free with
 * `sc_rendered_free`.
 */
ScRendered *sc_confirm_frame(
    const char *question, bool yes, ScConfirmOpts opts
);
ScRendered *sc_text_entry_frame(const ScTextEntryCfg *cfg);
ScRendered *sc_select_frame(ScSelect *select);
ScRendered *sc_fuzzy_frame(ScFuzzy *fuzzy, const char *query);

/* Test accessor: the current scroll offset (first visible display entry). Used
   by the PTY suite to assert the viewport position mid-run. */
size_t sc_fuzzy_scroll_top(const ScFuzzy *fuzzy);
ScRendered *sc_datepicker_frame(const struct tm *seed, ScDatePickerOpts opts);
ScRendered *sc_form_frame(ScForm *form);
ScRendered *sc_form_frame_edit(ScForm *form, int field);
ScRendered *sc_textarea_frame(
    const char *prompt, const char *content, ScTextareaOpts opts
);
ScRendered *sc_number_frame(
    const char *prompt, double value, ScNumberOpts opts
);

/** Test-only seed state for calculator-mode number frames. */
typedef struct ScNumberCalcFrame {
    /** Editor content including the leading `=` (or the accepted display). */
    const char *expr;

    /** Render the post-accept state (full-precision value pending). */
    bool accepted;

    /** The pending full-precision value (accepted state); drives the
        ` = <value>` indicator when it differs from the displayed `expr`. */
    double value;

    /** Render the invalid-expression error line. */
    bool error;

    /** Render the discarded-result warning line. */
    bool discarded;
} ScNumberCalcFrame;

/**
 * Renders a calculator-mode number frame without a TTY (style/logic tests).
 * `opts.calculator` is forced on; the editor is seeded with `frame.expr`.
 */
ScRendered *sc_number_frame_calc(
    const char *prompt, ScNumberCalcFrame frame, ScNumberOpts opts
);


/* ── Theme merge (theme.c): fills zero-init opts from the global theme ───── */

void sc_theme_apply_confirm   (ScConfirmOpts    *o);
void sc_theme_apply_text      (ScTextInputOpts  *o);
void sc_theme_apply_password  (ScPasswordOpts   *o);
void sc_theme_apply_number    (ScNumberOpts     *o);
void sc_theme_apply_textarea  (ScTextareaOpts   *o);
void sc_theme_apply_select    (ScSelectOpts     *o);
void sc_theme_apply_fuzzy     (ScFuzzyOpts      *o);
void sc_theme_apply_datepicker(ScDatePickerOpts *o);
void sc_theme_apply_form      (ScFormOpts       *o);
