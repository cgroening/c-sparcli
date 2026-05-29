#include "test_style.h"
#include "input/input_internal.h"


void style_text(void) {
    ScTextStyle none = { 0 };

    /* Default cursor block (black on white), with a value. */
    style_show("text: default cursor, plain value",
        sc_text_entry_frame("Name:", "Ada Lovelace", NULL, NULL, none, none, none));

    /* Empty field with a dim placeholder + default cursor. */
    style_show("text: empty with placeholder",
        sc_text_entry_frame("Email:", "", "you@example.com", NULL, none, none, none));

    /* Styled value + custom cursor (white on blue). */
    style_show("text: cyan value, cursor white-on-blue",
        sc_text_entry_frame("Host:", "localhost", NULL, NULL,
            (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
            (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_BLUE }));

    /* Password masks. */
    style_show("password: default mask '*'",
        sc_text_entry_frame("Password:", "hunter2", NULL, "*", none, none, none));
    style_show("password: bullet mask",
        sc_text_entry_frame("PIN:", "1234", NULL, "\xe2\x80\xa2" /* • */,
                            none, none, none));
    style_show("password: hidden mask (length concealed)",
        sc_text_entry_frame("Secret:", "topsecret", NULL, "", none, none, none));
}
