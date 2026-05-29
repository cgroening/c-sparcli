#include "input_internal.h"


static ScInputTheme g_theme;  /* zero-init = no theme */

void sc_input_set_theme(const ScInputTheme *theme) {
    g_theme = theme ? *theme : (ScInputTheme){ 0 };
}

ScInputTheme sc_input_theme(void) {
    return g_theme;
}


/* ── Field-merge helpers: theme fills only what the caller left unset ────── */

static void m_color(ScColor *dst, ScColor src) {
    if (dst->index == 0 && src.index != 0) { *dst = src; }
}
static void m_style(ScTextStyle *dst, ScTextStyle src) {
    if (!sc_style_set(*dst) && sc_style_set(src)) { *dst = src; }
}
static void m_glyph(const char **dst, const char *src) {
    if (!*dst && src) { *dst = src; }
}
static void m_border(ScBorderStyle *dst, ScBorderStyle src) {
    if (dst->type == SC_BORDER_NONE && src.type != SC_BORDER_NONE) { *dst = src; }
}


void sc_theme_apply_confirm(ScConfirmOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    o->hide_hint = o->hide_hint || t.hide_hint;
}

void sc_theme_apply_text(ScTextInputOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->error_style, t.error_style);
    m_style(&o->count_style, t.count_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_border(&o->border, t.border);
    o->hide_hint = o->hide_hint || t.hide_hint;
}

void sc_theme_apply_password(ScPasswordOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->error_style, t.error_style);
    m_style(&o->count_style, t.count_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_border(&o->border, t.border);
    o->hide_hint = o->hide_hint || t.hide_hint;
}

void sc_theme_apply_number(ScNumberOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_border(&o->border, t.border);
    o->hide_hint = o->hide_hint || t.hide_hint;
}

void sc_theme_apply_textarea(ScTextareaOpts *o) {
    ScInputTheme t = g_theme;
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->cursor_style, t.cursor_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    m_border(&o->border, t.border);
    o->hide_hint = o->hide_hint || t.hide_hint;
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
    o->hide_hint = o->hide_hint || t.hide_hint;
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
    o->hide_hint = o->hide_hint || t.hide_hint;
}

void sc_theme_apply_datepicker(ScDatePickerOpts *o) {
    ScInputTheme t = g_theme;
    m_color(&o->accent, t.accent);
    m_style(&o->prompt_style, t.prompt_style);
    m_style(&o->selected_style, t.selected_style);
    m_style(&o->summary_style, t.summary_style);
    m_style(&o->hint_style, t.hint_style);
    o->hide_hint = o->hide_hint || t.hide_hint;
}
