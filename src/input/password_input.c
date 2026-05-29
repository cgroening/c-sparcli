#include "input_internal.h"


ScInputStatus sc_password_input(
    const char *prompt, char **out, ScPasswordOpts opts
) {
    const char *mask = opts.mask ? opts.mask : "*";
    ScTextStyle value_style = { 0 };  /* masked glyphs use the default color */
    return sc_text_entry(
        prompt, out, NULL, opts.placeholder, mask,
        opts.prompt_style, value_style,
        opts.validate, opts.validate_ctx
    );
}
