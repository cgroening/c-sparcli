#include "sparcli.h"
#include <stdio.h>


void test_panels(void) {
    ScOptions plain   = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold    = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions red     = { SC_STYLE_NONE, SC_COLOR_RED,  SC_COLOR_NONE };
    ScOptions bold_ttl = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };

    printf("\n");

    /* No border */
    sc_panel_str("No border panel", (ScPanelOpts){
        SC_BORDER_NONE, SC_COLOR_NONE, NULL, plain,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 1, 0, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* ASCII border, title top-left */
    sc_panel_str("ASCII border", (ScPanelOpts){
        SC_BORDER_ASCII, SC_COLOR_NONE, "Title", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* Single border, title top-center, cyan, padding, multi-line */
    sc_panel_str("Single border\nSecond line\nThird line", (ScPanelOpts){
        SC_BORDER_SINGLE, SC_COLOR_CYAN, "Info", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_CENTER, 2, 1, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* Double border, title bottom-right, yellow */
    sc_panel_str("Double border", (ScPanelOpts){
        SC_BORDER_DOUBLE, SC_COLOR_YELLOW, "Warning", bold_ttl,
        SC_TITLE_BOTTOM, SC_ALIGN_RIGHT, 2, 0, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* Rounded border, no title, RGB border color */
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        SC_BORDER_ROUNDED, sc_rgb(180, 100, 255), NULL, plain,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* Thick border, title top-left, green */
    sc_panel_str("Thick border", (ScPanelOpts){
        SC_BORDER_THICK, SC_COLOR_GREEN, "Thick", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1, SC_ALIGN_LEFT, 0
    });

    printf("\n");

    /* ScText content with mixed styles inside a panel */
    ScOptions on_blue   = { SC_STYLE_NONE, SC_COLOR_BLACK, SC_COLOR_GREEN };

    ScText *t = sc_text_new();
    sc_text_append(t, "Normal ",    plain);
    sc_text_append(t, "bold ",      bold);
    sc_text_append(t, "and red\n",  red);
    sc_text_append(t, "second line", plain);
    sc_panel_text(t, (ScPanelOpts){
        SC_BORDER_ROUNDED, SC_COLOR_GREEN, " Styled Content ", on_blue,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 1, 0, 0, SC_ALIGN_LEFT, 0
    });
    sc_text_free(t);

    printf("\n");

    /* Full-width panel, content centered */
    sc_panel_str("This panel stretches to the full terminal width.\nContent is centered.", (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = SC_COLOR_CYAN,
        .title         = " Full Width ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_CENTER,
        .pad_x         = 2,
        .pad_y         = 0,
        .title_pad     = 1,
        .content_align = SC_ALIGN_CENTER,
        .full_width    = 1,
    });

    printf("\n");

    /* Full-width panel, content right-aligned */
    sc_panel_str("Right-aligned text.\nSecond line.", (ScPanelOpts){
        .border        = SC_BORDER_SINGLE,
        .border_color  = SC_COLOR_NONE,
        .title         = " Right-Aligned Content ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_RIGHT,
        .pad_x         = 1,
        .pad_y         = 0,
        .title_pad     = 1,
        .content_align = SC_ALIGN_RIGHT,
        .full_width    = 1,
    });
}
