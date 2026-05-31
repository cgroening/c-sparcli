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
} NumberState;

static const char *const DEFAULT_HINT =
    "\xe2\x86\x91/\xe2\x86\x93 adjust \xc2\xb7 enter submit \xc2\xb7 "
    "esc cancel";


static bool init_state(NumberState *self, const char *prompt,
                       ScNumberOpts opts, double value);
static ScRendered *number_render(void *state);
    static ScRendered *number_render_inline(NumberState *self);
    static ScRendered *number_render_boxed(NumberState *self);
        static void range_str(const NumberState *self, char *buf, size_t cap);
        static const char *number_hint(const NumberState *self);
static void number_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static double clamp(const NumberState *self, double value);
    static void set_value(NumberState *self, double value);


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
    };
    ScPromptShortcuts sk = {
        opts.shortcuts, opts.n_shortcuts, opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, &state, opts.shortcuts ? &sk : NULL, NULL);

    if (status == SC_INPUT_OK) {
        *out = clamp(&state, strtod(state.ed.buf, NULL));
        if (!opts.hide_summary) {
            char line[96];
            snprintf(
                line, sizeof line, "? %s %.*f", prompt, opts.decimals, *out
            );
            sc_println(line, opts.summary_style);
        }
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

/** Initializes `self`, seeding the editor with the clamped value. */
static bool init_state(NumberState *self, const char *prompt,
                       ScNumberOpts opts, double value) {
    self->prompt = prompt;
    self->opts = opts;
    self->bounded = opts.max > opts.min;
    char buf[64];
    snprintf(buf, sizeof buf, "%.*f", opts.decimals, clamp(self, value));
    sc_le_init(&self->ed, buf);
    return self->ed.buf != NULL;
}

static ScRendered *number_render(void *state) {
    NumberState *self = state;
    return self->opts.boxed ? number_render_boxed(self)
                            : number_render_inline(self);
}

/** Inline: "prompt value  [min-max]" then the footer. */
static ScRendered *number_render_inline(NumberState *self) {
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
    sc_le_render_into(&self->ed, text, self->opts.value_style,
                      self->opts.cursor_style, NULL, NULL, placeholder_style,
                      field);

    char range[80];
    range_str(self, range, sizeof range);
    if (range[0]) {
        sc_text_append(text, "  ", (ScTextStyle){ 0 });
        sc_text_append(text, range, (ScTextStyle){ SC_TEXT_ATTR_DIM,
                       SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    }
    ScRendered *body = sc_capture_text(text);
    sc_text_free(text);
    return sc_compose_hint(body, number_hint(self), self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/** Boxed: value inside a panel, range on the bottom border, footer below. */
static ScRendered *number_render_boxed(NumberState *self) {
    ScText *inner = sc_text_new();
    if (!inner) {
        return NULL;
    }
    int panel_width = self->opts.width > 0 ? self->opts.width
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

    char range[80];
    range_str(self, range, sizeof range);
    ScText *title_text = sc_prompt_build(self->prompt, self->opts.prompt_style,
                                         self->opts.prompt_markup,
                                         self->opts.prompt_text);
    ScPanelOpts panel_opts = {
        .border = self->opts.border,
        .title = { .text = self->prompt, .rich_text = title_text,
                   .style = self->opts.prompt_style,
                   .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding = { .left = 1, .right = 1 },
        .content_align = SC_ALIGN_LEFT,
    };
    if (panel_opts.border.type == SC_BORDER_NONE) {
        panel_opts.border.type = SC_BORDER_ROUNDED;
    }
    if (self->opts.width > 0) {
        panel_opts.width = self->opts.width;
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
    if (!panel) {
        return NULL;
    }
    return sc_compose_hint(panel, number_hint(self), self->opts.hint_layout,
                           self->opts.hint_pos, self->opts.hint_style);
}

/** Writes "[min-max]" into `buf`, or an empty string when unbounded. */
static void range_str(const NumberState *self, char *buf, size_t cap) {
    if (self->bounded) {
        snprintf(buf, cap, "[%.*f\xe2\x80\x93%.*f]", self->opts.decimals,
                 self->opts.min, self->opts.decimals, self->opts.max);
    } else {
        buf[0] = '\0';
    }
}

static const char *number_hint(const NumberState *self) {
    return self->opts.hint ? self->opts.hint : DEFAULT_HINT;
}

static void number_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    NumberState *self = state;
    double step = self->opts.step != 0 ? self->opts.step : 1;

    switch (key.type) {
        case SC_KEY_UP:
            set_value(self, clamp(self, strtod(self->ed.buf, NULL) + step));
            return;
        case SC_KEY_DOWN:
            set_value(self, clamp(self, strtod(self->ed.buf, NULL) - step));
            return;
        case SC_KEY_ENTER:
            *done = true;
            return;
        case SC_KEY_CHAR:
            if (!sc_filter_decimal(key.codepoint, NULL)) {
                return;
            }
            break;   // accepted digit/sign/point: fall through to the editor
        default:
            break;
    }
    sc_le_handle(&self->ed, key);
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
    snprintf(buf, sizeof buf, "%.*f", self->opts.decimals, value);
    sc_le_free(&self->ed);
    sc_le_init(&self->ed, buf);
}
