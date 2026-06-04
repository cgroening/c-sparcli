#include "input_internal.h"


static ScInputTheme g_theme;  /* zero-init = no theme installed */


static void free_theme_strings(void);

/* Field-merge helpers: each fills the target only when the caller left it
 * unset, so per-call options always win over the theme. */
static void m_color(ScColor *dst, ScColor src);
static void m_style(ScTextStyle *dst, ScTextStyle src);
static void m_glyph(const char **dst, const char *src);
static void m_border(ScBorderStyle *dst, ScBorderStyle src);
static void m_box(ScBoxStyle *dst, ScBoxStyle src);


void sc_input_set_theme(const ScInputTheme *theme) {
    free_theme_strings();
    g_theme = theme ? *theme : (ScInputTheme){ 0 };
    // Copy the glyph strings so the caller's buffers only need to live until
    // this call returns (FFI bindings pass temporaries). The copies are owned
    // by g_theme and released on the next set call.
    g_theme.cursor_marker = sc_dup_opt_str(g_theme.cursor_marker);
    g_theme.marker = sc_dup_opt_str(g_theme.marker);
    g_theme.checkbox_on = sc_dup_opt_str(g_theme.checkbox_on);
    g_theme.checkbox_off = sc_dup_opt_str(g_theme.checkbox_off);
}

/** Releases the glyph copies owned by the current theme. */
static void free_theme_strings(void) {
    free((char *)g_theme.cursor_marker);
    free((char *)g_theme.marker);
    free((char *)g_theme.checkbox_on);
    free((char *)g_theme.checkbox_off);
}

ScInputTheme sc_input_theme(void) {
    return g_theme;
}


void sc_theme_apply_confirm(ScConfirmOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_text(ScTextInputOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->error_style, t.error_style);
    m_style(&o->count_style, t.count_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_password(ScPasswordOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->error_style, t.error_style);
    m_style(&o->count_style, t.count_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_number(ScNumberOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_textarea(ScTextareaOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_select(ScSelectOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_glyph(&o->cursor_marker, t.cursor_marker);
    m_glyph(&o->marker, t.marker);
    m_glyph(&o->checkbox_on, t.checkbox_on);
    m_glyph(&o->checkbox_off, t.checkbox_off);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_fuzzy(ScFuzzyOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->counter_style, t.count_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_glyph(&o->cursor_marker, t.cursor_marker);
    m_glyph(&o->marker, t.marker);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}

void sc_theme_apply_datepicker(ScDatePickerOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_box(&o->box, t.box);
    if (o->hint_layout == SC_HINT_LAYOUT_DEFAULT) {
        o->hint_layout = t.hint_layout;
    }
    if (o->hint_pos == SC_HINT_POS_DEFAULT) {
        o->hint_pos = t.hint_pos;
    }
}


/* ── Field-merge helpers ────────────────────────────────────────────────── */

static void m_color(ScColor *dst, ScColor src) {
    if (dst->index == 0 && src.index != 0) {
        *dst = src;
    }
}

static void m_style(ScTextStyle *dst, ScTextStyle src) {
    if (!sc_style_set(*dst) && sc_style_set(src)) {
        *dst = src;
    }
}

static void m_glyph(const char **dst, const char *src) {
    if (!*dst && src) {
        *dst = src;
    }
}

static void m_border(ScBorderStyle *dst, ScBorderStyle src) {
    if (dst->type == SC_BORDER_NONE && src.type != SC_BORDER_NONE) {
        *dst = src;
    }
}

/** Merges box defaults sub-field by sub-field, so a caller can override any
 *  single aspect (e.g. just the border) and inherit the rest from the theme. */
static void m_box(ScBoxStyle *dst, ScBoxStyle src) {
    if (!dst->enabled) { dst->enabled = src.enabled; }
    m_border(&dst->border, src.border);
    m_color(&dst->bg, src.bg);
    bool padding_unset = dst->padding.top == 0 && dst->padding.right == 0 &&
                         dst->padding.bottom == 0 && dst->padding.left == 0;
    if (padding_unset) { dst->padding = src.padding; }
    bool margin_unset = dst->margin.top == 0 && dst->margin.right == 0 &&
                        dst->margin.bottom == 0 && dst->margin.left == 0;
    if (margin_unset) { dst->margin = src.margin; }
    if (dst->width == 0) { dst->width = src.width; }
}
