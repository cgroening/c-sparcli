#include "test_style.h"
#include "input/input_internal.h"


void style_textarea(void) {
    const char *body = "first line\nsecond line\nthird line";

    /* Inline (default), for contrast. */
    style_show("textarea: inline, multi-line",
        sc_textarea_frame(
            "Notes", "first line\nsecond line", (ScTextareaOpts){ 0 }));

    /* Boxed in a panel: prompt = top title, footer below the box. */
    style_show("textarea: boxed (panel), fixed width 36",
        sc_textarea_frame(
            "Notes", body, (ScTextareaOpts){ .boxed = true, .width = 36 }));

    /* Boxed with a double border + styled prompt. */
    style_show("textarea: boxed double border + cyan prompt",
        sc_textarea_frame("Description", "lorem ipsum\ndolor sit amet",
            (ScTextareaOpts){
                .boxed = true, .width = 40,
                .border = {
                    .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_BLUE
                },
                .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                                  SC_ANSI_COLOR_NONE } }));

    /* Boxed full width (printed flush-left so the right border fits). */
    style_show_flush("textarea: boxed full width (flush-left)",
        sc_textarea_frame(
            "Body", "spanning the whole terminal width\nsecond line",
            (ScTextareaOpts){ .boxed = true }));

    /* Soft-wrap: a single long logical line wraps to fit the narrow box. */
    style_show("textarea: long line soft-wraps inside the box",
        sc_textarea_frame("Wrap",
            "This is one very long logical line that must soft-wrap inside the "
            "narrow box instead of overflowing its right border.",
            (ScTextareaOpts){ .boxed = true, .width = 34 }));
}
