#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Render-time state for a single number prompt. */
typedef struct NumberState {
    const char *prompt;
    ScLineEditor ed;
    ScNumberOpts opts;
    bool bounded;

    /** Calculator: a result was accepted (Enter #1); `calc_value` is live. */
    bool calc_accepted;

    /** Calculator: the accepted full-precision result (already clamped). */
    double calc_value;

    /** Calculator: the last Enter found an invalid expression. */
    bool calc_error;

    /** Calculator: an edit discarded a pending result that differed from
        the display - the warning line is shown until submit/new accept. */
    bool calc_discarded;
} NumberState;

static const char *const DEFAULT_HINT =
    "\xe2\x86\x91/\xe2\x86\x93 adjust \xc2\xb7 enter submit \xc2\xb7 "
    "esc cancel";
static const char *const CALC_AVAILABLE_HINT =
    "\xe2\x86\x91/\xe2\x86\x93 adjust \xc2\xb7 = calc \xc2\xb7 "
    "enter submit \xc2\xb7 esc cancel";
static const char *const CALC_MODE_HINT =
    "enter accept result \xc2\xb7 esc cancel";
static const char *const CALC_ERROR_TEXT = "invalid expression";
static const char *const CALC_DISCARDED_TEXT =
    "exact result discarded - the displayed value will be submitted";

/** Format string for full-precision values (round-trip exact for doubles). */
#define CALC_PRECISE_FORMAT "%.17g"

/** Format for the pending-result indicator: enough digits to show that the
    pending value is more precise than the rounded display, without the noise
    of the full 17-digit round-trip form. */
#define CALC_PENDING_FORMAT "%.10g"


static bool init_state(NumberState *self, const char *prompt,
                       ScNumberOpts opts, double value);
static void submit_value(NumberState *self, const char *prompt, double *out,
                         ScInputStatus *status);
static ScRendered *number_render(void *state);
    static ScRendered *number_body_inline(NumberState *self);
    static ScRendered *number_body_boxed(NumberState *self);
        static void range_str(const NumberState *self, char *buf, size_t cap);
    static ScRendered *stack_calc_error(NumberState *self, ScRendered *body);
    static ScRendered *stack_calc_warning(NumberState *self, ScRendered *body);
    static const char *number_hint(const NumberState *self);
static void number_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static bool number_filter_char(NumberState *self, ScKey *key);
        static bool is_calc_char(uint32_t codepoint);
    static void calc_on_enter(NumberState *self);
    static void note_pending_discard(NumberState *self);
    static double parse_value(const NumberState *self);
        static void normalized_value(const NumberState *self, char *buf,
                                     size_t cap);
    static double clamp(const NumberState *self, double value);
    static void set_value(NumberState *self, double value);
        static void format_value(const NumberState *self, double value,
                                 char *buf, size_t cap);
            static char number_sep(const NumberState *self);
            static void apply_decimal_sep(const NumberState *self, char *buf);

/* Calculator helpers */
static bool in_calc_mode(const NumberState *self);
static bool calc_eval_current(const NumberState *self, double *result);
static bool calc_pending_differs(const NumberState *self);
static void calc_display_value(const NumberState *self, double value,
                               char *buf, size_t cap);
static void calc_pending_str(const NumberState *self, char *buf, size_t cap);
static void calc_preview_str(const NumberState *self, char *buf, size_t cap);
static ScTextStyle resolve_error_style(const NumberState *self);
static ScTextStyle resolve_warn_style(const NumberState *self);


ScInputStatus sc_number_input(const char *prompt, double *out,
                              ScNumberOpts opts) {
    if (!prompt || !out) {
        return SC_INPUT_ERROR;
    }
    sc_theme_apply_number(&opts);

    NumberState state;
    if (!init_state(&state, prompt, opts, opts.initial)) {
        return SC_INPUT_ERROR;
    }

    ScPromptVTable vtable = {
        .render = number_render,
        .on_key = number_on_key,
        .paste = SC_PASTE_TEXT,
    };
    ScPromptShortcuts sk = {
        opts.shortcuts, opts.n_shortcuts, opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, &state, opts.shortcuts ? &sk : NULL, NULL);

    if (status == SC_INPUT_OK) {
        submit_value(&state, prompt, out, &status);
    }
    sc_le_free(&state.ed);
    return status;
}

ScRendered *sc_number_frame(const char *prompt, double value,
                            ScNumberOpts opts) {
    NumberState state;
    if (!init_state(&state, prompt, opts, value)) {
        return NULL;
    }
    ScRendered *rendered = number_render(&state);
    sc_le_free(&state.ed);
    return rendered;
}

ScRendered *sc_number_frame_calc(const char *prompt, ScNumberCalcFrame frame,
                                 ScNumberOpts opts) {
    opts.calculator = true;
    NumberState state;
    if (!init_state(&state, prompt, opts, opts.initial)) {
        return NULL;
    }
    // Seed the editor with the expression (or accepted display) directly.
    sc_le_free(&state.ed);
    sc_le_init(&state.ed, frame.expr ? frame.expr : "");
    if (!state.ed.buf) {
        return NULL;
    }
    state.calc_accepted = frame.accepted;
    state.calc_value = frame.value;
    state.calc_error = frame.error;
    state.calc_discarded = frame.discarded;

    ScRendered *rendered = number_render(&state);
    sc_le_free(&state.ed);
    return rendered;
}

/** Initializes `self`, seeding the editor with the clamped value
    (or empty when `opts.start_empty` is set). */
static bool init_state(NumberState *self, const char *prompt,
                       ScNumberOpts opts, double value) {
    self->prompt = prompt;
    self->opts = opts;
    self->bounded = opts.max > opts.min;
    self->calc_accepted = false;
    self->calc_value = 0;
    self->calc_error = false;
    self->calc_discarded = false;
    char buf[64] = "";
    if (!opts.start_empty) {
        format_value(self, clamp(self, value), buf, sizeof buf);
    }
    sc_le_init(&self->ed, buf);
    return self->ed.buf != NULL;
}

/**
 * Produces `*out`, the optional `*out_text` and the summary line after a
 * successful prompt run.
 *
 * A pending calculator result keeps its full precision (the field shows the
 * rounded display) unless `calc_store_rounded` is set - then the displayed
 * text is the value, exactly like a typed number.
 */
static void submit_value(NumberState *self, const char *prompt, double *out,
                         ScInputStatus *status) {
    const ScNumberOpts *opts = &self->opts;
    double value;
    char text_buf[64];

    if (self->calc_accepted && !opts->calc_store_rounded) {
        // Full-precision calculator result: the field shows the (possibly
        // rounded) display, but *out/out_text carry the exact value.
        value = self->calc_value;
        snprintf(text_buf, sizeof text_buf, CALC_PRECISE_FORMAT, value);
    } else {
        // Typed value or rounded calculator result: the displayed text is
        // the value. Clamp and rewrite the buffer so text, *out and
        // *out_text all agree.
        value = clamp(self, parse_value(self));
        set_value(self, value);
        normalized_value(self, text_buf, sizeof text_buf);
    }

    *out = value;
    if (opts->out_text) {
        *opts->out_text = sc_dup_str(text_buf);
        if (!*opts->out_text) {
            *status = SC_INPUT_ERROR;
        }
    }
    if (!opts->hide_summary) {
        // The summary shows what the field showed (the display form).
        char line[96];
        snprintf(line, sizeof line, "? %s %s", prompt,
                 self->ed.buf ? self->ed.buf : "");
        sc_println(line, opts->summary_style);
    }
}

static ScRendered *number_render(void *state) {
    NumberState *self = state;
    ScRendered *body = self->opts.box.enabled ? number_body_boxed(self)
                                              : number_body_inline(self);
    body = stack_calc_error(self, body);
    body = stack_calc_warning(self, body);
    return sc_compose_hint(body, number_hint(self), self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/** Inline body: "prompt value [= preview]  [min-max]". */
static ScRendered *number_body_inline(NumberState *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    sc_prompt_append(text, self->prompt, self->opts.prompt_style,
                     self->opts.prompt_markup, self->opts.prompt_text);
    sc_text_append(text, " ", (ScTextStyle){ 0 });

    int prompt_w = sc_prompt_width(
        self->prompt, self->opts.prompt_style,
        self->opts.prompt_markup, self->opts.prompt_text
    );
    int field = sc_terminal_width() - prompt_w - 1;
    if (field < 1) {
        field = 1;
    }
    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    ScTextStyle dim_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                              SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, text, self->opts.value_style,
                      self->opts.cursor_style, NULL, NULL, placeholder_style,
                      field);

    // Live calculator preview: " = 7,5" / " = ?" while editing an expression.
    if (in_calc_mode(self)) {
        char preview[96];
        calc_preview_str(self, preview, sizeof preview);
        sc_text_append(text, preview, dim_style);
    }

    // Pending-result indicator: " = 0,3333333333" while the accepted
    // full-precision result (not the rounded display) would be submitted.
    if (calc_pending_differs(self)) {
        char pending[96];
        calc_pending_str(self, pending, sizeof pending);
        sc_text_append(text, pending, dim_style);
    }

    char range[80];
    range_str(self, range, sizeof range);
    if (range[0]) {
        sc_text_append(text, "  ", (ScTextStyle){ 0 });
        sc_text_append(text, range, dim_style);
    }
    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    return body;
}

/** Boxed body: value (+ preview) inside a panel, range on the bottom border. */
static ScRendered *number_body_boxed(NumberState *self) {
    ScText *inner = sc_text_new();
    if (!inner) {
        return NULL;
    }
    int panel_width = self->opts.box.width > 0 ? self->opts.box.width
                                               : sc_terminal_width() - 2;
    int field = panel_width - 4;
    if (field < 1) {
        field = 1;
    }
    ScTextStyle placeholder_style = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                      SC_ANSI_COLOR_NONE };
    sc_le_render_into(&self->ed, inner, self->opts.value_style,
                      self->opts.cursor_style, NULL, NULL, placeholder_style,
                      field);

    // Live calculator preview inside the panel.
    if (in_calc_mode(self)) {
        char preview[96];
        calc_preview_str(self, preview, sizeof preview);
        sc_text_append(inner, preview, placeholder_style);
    }

    // Pending-result indicator inside the panel (see number_body_inline).
    if (calc_pending_differs(self)) {
        char pending[96];
        calc_pending_str(self, pending, sizeof pending);
        sc_text_append(inner, pending, placeholder_style);
    }

    char range[80];
    range_str(self, range, sizeof range);
    ScText *title_text = sc_prompt_build(self->prompt, self->opts.prompt_style,
                                         self->opts.prompt_markup,
                                         self->opts.prompt_text);
    ScPanelOpts panel_opts = {
        .border = self->opts.box.border,
        .bg = self->opts.box.bg,
        .title = { .text = self->prompt, .rich_text = title_text,
                   .style = self->opts.prompt_style,
                   .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding = sc_box_padding(self->opts.box.padding),
        .margin = self->opts.box.margin,
        .content_align = SC_ALIGN_LEFT,
    };
    if (panel_opts.border.type == SC_BORDER_NONE) {
        panel_opts.border.type = SC_BORDER_ROUNDED;
    }
    if (self->opts.box.width > 0) {
        panel_opts.width = self->opts.box.width;
    } else {
        panel_opts.full_width = true;
    }
    if (range[0]) {
        panel_opts.subtitle = (ScTitle){
            .text = range,
            .style = {
                SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
            },
            .halign = SC_ALIGN_RIGHT, .pad = 1, .pos = SC_POSITION_BOTTOM,
        };
    }

    ScRendered *panel = sc_capture_panel_text(inner, panel_opts);
    sc_text_free(inner);
    sc_text_free(title_text);
    return panel;
}

/** Writes "[min-max]" into `buf` (with the configured decimal separator),
    or an empty string when unbounded. */
static void range_str(const NumberState *self, char *buf, size_t cap) {
    if (!self->bounded) {
        buf[0] = '\0';
        return;
    }
    snprintf(buf, cap, "[%.*f-%.*f]", self->opts.decimals,
             self->opts.min, self->opts.decimals, self->opts.max);
    if (number_sep(self) != '.') {
        for (char *p = buf; *p; p++) {
            if (*p == '.') {
                *p = number_sep(self);
            }
        }
    }
}

/**
 * Stacks the red invalid-expression line beneath the body when the last
 * Enter hit an invalid calculator expression.
 */
static ScRendered *stack_calc_error(NumberState *self, ScRendered *body) {
    if (!self->calc_error) {
        return body;
    }
    ScText *error_text = sc_text_new();
    if (!error_text) {
        return body;
    }
    sc_text_append(error_text, "  ", (ScTextStyle){ 0 });
    sc_text_append(error_text, CALC_ERROR_TEXT, resolve_error_style(self));
    ScRendered *error_rendered = sc_capture_text(error_text);
    sc_text_free(error_text);
    return sc_stack_below(body, error_rendered);
}

/**
 * Stacks the warning line beneath the body after an edit discarded a pending
 * full-precision result that differed from the display: from then on the
 * displayed value is what gets submitted. Hidden while a new expression is
 * being typed; cleared on the next accepted result.
 */
static ScRendered *stack_calc_warning(NumberState *self, ScRendered *body) {
    if (!self->calc_discarded || in_calc_mode(self)) {
        return body;
    }
    ScText *warn_text = sc_text_new();
    if (!warn_text) {
        return body;
    }
    const char *message = self->opts.calc_warn_text
        ? self->opts.calc_warn_text : CALC_DISCARDED_TEXT;
    sc_text_append(warn_text, "  ", (ScTextStyle){ 0 });
    sc_text_append(warn_text, message, resolve_warn_style(self));
    ScRendered *warn_rendered = sc_capture_text(warn_text);
    sc_text_free(warn_text);
    return sc_stack_below(body, warn_rendered);
}

/** Resolves the footer hint for the current mode. */
static const char *number_hint(const NumberState *self) {
    if (self->opts.hint) {
        return self->opts.hint;
    }
    if (in_calc_mode(self)) {
        return CALC_MODE_HINT;
    }
    if (self->opts.calculator) {
        return CALC_AVAILABLE_HINT;
    }
    return DEFAULT_HINT;
}

static void number_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    NumberState *self = state;
    double step = self->opts.step != 0 ? self->opts.step : 1;

    switch (key.type) {
        case SC_KEY_UP:
            if (in_calc_mode(self)) {
                return;   // no stepping inside an expression
            }
            note_pending_discard(self);
            set_value(self, clamp(self, parse_value(self) + step));
            self->calc_accepted = false;   // stepping replaces a calc result
            return;
        case SC_KEY_DOWN:
            if (in_calc_mode(self)) {
                return;
            }
            note_pending_discard(self);
            set_value(self, clamp(self, parse_value(self) - step));
            self->calc_accepted = false;
            return;
        case SC_KEY_ENTER:
            if (in_calc_mode(self)) {
                calc_on_enter(self);
                return;
            }
            // An empty field is not a number: ignore Enter so the prompt
            // never submits "nothing" as 0 (matters for start_empty).
            if (!self->ed.buf || self->ed.buf[0] == '\0') {
                return;
            }
            *done = true;
            return;
        case SC_KEY_CHAR:
            if (!number_filter_char(self, &key)) {
                return;
            }
            break;   // accepted character: fall through to the editor
        default:
            break;
    }

    // Editing keys reshape the buffer: any length change discards a pending
    // calculator result and clears the error (the text is the value again).
    // Precision loss is checked before the edit, against the accepted display.
    bool pending_differed = calc_pending_differs(self);
    size_t len_before = self->ed.len;
    sc_le_handle(&self->ed, key);
    if (self->ed.len != len_before) {
        if (pending_differed) {
            self->calc_discarded = true;
        }
        self->calc_accepted = false;
        self->calc_error = false;
    }
}

/**
 * Filters a typed character for the current mode; may rewrite the decimal
 * separator. Returns `true` when the key should reach the editor.
 */
static bool number_filter_char(NumberState *self, ScKey *key) {
    if (in_calc_mode(self)) {
        return is_calc_char(key->codepoint);
    }
    // '=' as the first character enters calculator mode (when enabled).
    if (key->codepoint == '=' && self->opts.calculator
        && (!self->ed.buf || self->ed.buf[0] == '\0')) {
        return true;
    }
    if (!sc_filter_decimal(key->codepoint, NULL)) {
        return false;
    }
    // A typed '.' or ',' becomes the configured separator.
    if (key->codepoint == '.' || key->codepoint == ',') {
        key->codepoint = (uint32_t)(unsigned char)number_sep(self);
        key->bytes[0] = number_sep(self);
        key->bytes[1] = '\0';
    }
    return true;
}

/** Characters allowed inside a calculator expression. */
static bool is_calc_char(uint32_t codepoint) {
    return (codepoint >= '0' && codepoint <= '9')
        || codepoint == '.' || codepoint == ',' || codepoint == ' '
        || codepoint == '+' || codepoint == '-'
        || codepoint == '*' || codepoint == '/'
        || codepoint == '(' || codepoint == ')';
}

/**
 * Enter on an expression: evaluate it and replace the field with the result
 * (the "accept" step). The full-precision result is kept for submission; the
 * field shows the display form. An invalid expression raises the error line
 * and keeps the prompt open.
 */
static void calc_on_enter(NumberState *self) {
    double result;
    if (!calc_eval_current(self, &result)) {
        self->calc_error = true;
        return;
    }
    double clamped = clamp(self, result);
    self->calc_value = clamped;
    self->calc_error = false;
    self->calc_discarded = false;   // a fresh exact value is pending again

    char buf[64];
    calc_display_value(self, clamped, buf, sizeof buf);
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, buf);
    // The buffer no longer starts with '=', so the next Enter takes the
    // normal submit path while `calc_accepted` marks the pending value.
    self->calc_accepted = self->ed.buf != NULL;
}

/** Raises the discard warning when the pending full-precision result differs
    from the displayed value (call before the buffer is rewritten). */
static void note_pending_discard(NumberState *self) {
    if (calc_pending_differs(self)) {
        self->calc_discarded = true;
    }
}

/** Parses the editor content into a double, separator-normalized. */
static double parse_value(const NumberState *self) {
    char buf[64];
    normalized_value(self, buf, sizeof buf);
    return strtod(buf, NULL);
}

/** Writes the editor content with ',' rewritten to '.' (parse/out form). */
static void normalized_value(const NumberState *self, char *buf, size_t cap) {
    const char *src = self->ed.buf ? self->ed.buf : "";
    size_t i = 0;
    for (; src[i] && i + 1 < cap; i++) {
        buf[i] = src[i] == ',' ? '.' : src[i];
    }
    buf[i] = '\0';
}

/** Clamps `value` to `[min, max]` when the prompt is bounded. */
static double clamp(const NumberState *self, double value) {
    if (!self->bounded) {
        return value;
    }
    if (value < self->opts.min) {
        return self->opts.min;
    }
    if (value > self->opts.max) {
        return self->opts.max;
    }
    return value;
}

/** Replaces the editor content with `value` formatted to `decimals` places. */
static void set_value(NumberState *self, double value) {
    char buf[64];
    format_value(self, value, buf, sizeof buf);
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, buf);
}

/** Formats `value` with `decimals` places and the configured separator. */
static void format_value(const NumberState *self, double value, char *buf,
                         size_t cap) {
    snprintf(buf, cap, "%.*f", self->opts.decimals, value);
    apply_decimal_sep(self, buf);
}

/** Returns the configured decimal separator, resolving `0` to '.'. */
static char number_sep(const NumberState *self) {
    return self->opts.decimal_sep == ',' ? ',' : '.';
}

/** Rewrites the first '.' in `buf` to the configured decimal separator. */
static void apply_decimal_sep(const NumberState *self, char *buf) {
    if (number_sep(self) == '.') {
        return;
    }
    char *dot = strchr(buf, '.');
    if (dot) {
        *dot = number_sep(self);
    }
}


/* ── Calculator helpers ─────────────────────────────────────────────────── */

/** True while the field holds an expression (calculator enabled + leading
    '='). Backspacing the '=' away exits the mode automatically. */
static bool in_calc_mode(const NumberState *self) {
    return self->opts.calculator && self->ed.buf && self->ed.buf[0] == '=';
}

/** Evaluates the current expression (the text after the leading '='). */
static bool calc_eval_current(const NumberState *self, double *result) {
    const char *expr = self->ed.buf ? self->ed.buf + 1 : "";
    return sc_calc_eval(expr, result);
}

/**
 * True when the pending full-precision result would submit a different value
 * than the displayed text - i.e. discarding it loses precision. Always false
 * with `calc_store_rounded` (the display is the value there).
 */
static bool calc_pending_differs(const NumberState *self) {
    if (!self->calc_accepted || self->opts.calc_store_rounded) {
        return false;
    }
    return clamp(self, parse_value(self)) != self->calc_value;
}

/**
 * Formats a calculator result for display: rounded to `decimals` (default)
 * or full precision (`calc_show_precise`), with the configured separator.
 */
static void calc_display_value(const NumberState *self, double value,
                               char *buf, size_t cap) {
    if (!self->opts.calc_show_precise) {
        format_value(self, value, buf, cap);
        return;
    }
    snprintf(buf, cap, CALC_PRECISE_FORMAT, value);
    apply_decimal_sep(self, buf);
}

/** Writes the pending-result indicator: " = <value>" with the pending value
    at indicator precision and the configured separator. */
static void calc_pending_str(const NumberState *self, char *buf, size_t cap) {
    char value_str[64];
    snprintf(value_str, sizeof value_str, CALC_PENDING_FORMAT,
             self->calc_value);
    apply_decimal_sep(self, value_str);
    snprintf(buf, cap, " = %s", value_str);
}

/** Writes the live preview: " = <result>" or " = ?" when invalid. The raw
    (unclamped) result is shown; clamping happens when it is accepted. */
static void calc_preview_str(const NumberState *self, char *buf, size_t cap) {
    double result;
    if (!calc_eval_current(self, &result)) {
        snprintf(buf, cap, " = ?");
        return;
    }
    char value_str[64];
    calc_display_value(self, result, value_str, sizeof value_str);
    snprintf(buf, cap, " = %s", value_str);
}

/** Error-line style; zero-init = red. */
static ScTextStyle resolve_error_style(const NumberState *self) {
    if (sc_style_set(self->opts.error_style)) {
        return self->opts.error_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,
                          SC_ANSI_COLOR_NONE };
}

/** Discard-warning style; zero-init = yellow. */
static ScTextStyle resolve_warn_style(const NumberState *self) {
    if (sc_style_set(self->opts.calc_warn_style)) {
        return self->opts.calc_warn_style;
    }
    return (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW,
                          SC_ANSI_COLOR_NONE };
}
