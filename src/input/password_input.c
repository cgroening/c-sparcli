#include "input_internal.h"


ScInputStatus sc_password_input(
    const char *prompt, char **out, ScPasswordOpts opts
) {
    sc_theme_apply_password(&opts);
    ScTextEntryCfg cfg = {
        .prompt          = prompt,
        .placeholder     = opts.placeholder,
        .mask            = opts.mask ? opts.mask : "*",
        .prompt_style    = opts.prompt_style,
        /* value_style stays unset: masked glyphs use the default color. */
        .cursor_style    = opts.cursor_style,
        .error_style     = opts.error_style,
        .count_style     = opts.count_style,
        .summary_style   = opts.summary_style,
        .hide_summary    = opts.hide_summary,
        .hide_char_count = opts.hide_char_count,
        .max_chars       = opts.max_chars,
        .boxed           = opts.boxed,
        .border          = opts.border,
        .width           = opts.width,
        .hint            = opts.hint,
        .hint_layout     = opts.hint_layout,
        .hint_pos        = opts.hint_pos,
        .hint_style      = opts.hint_style,
        .char_filter     = opts.char_filter,
        .char_filter_ctx = opts.char_filter_ctx,
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
    };
    return sc_text_entry(&cfg, out);
}
