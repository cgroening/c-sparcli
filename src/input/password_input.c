#include "input_internal.h"


ScInputStatus sc_password_input(
    const char *prompt, char **out, ScPasswordOpts opts
) {
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
        .validate        = opts.validate,
        .validate_ctx    = opts.validate_ctx,
    };
    return sc_text_entry(&cfg, out);
}
