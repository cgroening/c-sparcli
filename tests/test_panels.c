#include "sparcli.h"
#include <stdio.h>


void test_panels(void) {
    ScTextStyle plain   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    ScTextStyle bold    = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    ScTextStyle red     = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,  SC_ANSI_COLOR_NONE };
    ScTextStyle bold_ttl = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };

    printf("\n");

    /* No border + ASCII border side by side */
    ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 3, .sep_color = SC_ANSI_COLOR_NONE });
    sc_columns_add_panel_str(cl, "No border panel", (ScPanelOpts){
        .border       = SC_BORDER_NONE,
        .border_color = SC_ANSI_COLOR_NONE,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Title",
        .title_opts   = plain,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 1, 0, 1},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    }, (ScColItem){ 0 });
    sc_columns_add_panel_str(cl, "ASCII border", (ScPanelOpts){
        .border       = SC_BORDER_ASCII,
        .border_color = SC_ANSI_COLOR_NONE,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Title",
        .title_opts   = bold_ttl,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 2, 0, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    }, (ScColItem){ 0 });
    sc_columns_print(cl);
    sc_columns_free(cl);




    /* No border */
    sc_panel_str("No border panel", (ScPanelOpts){
        .border       = SC_BORDER_NONE,
        .border_color = SC_ANSI_COLOR_NONE,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Title",
        .title_opts   = plain,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 1, 0, 1},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* ASCII border, title top-left */
    sc_panel_str("ASCII border", (ScPanelOpts){
        .border       = SC_BORDER_ASCII,
        .border_color = SC_ANSI_COLOR_NONE,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Title",
        .title_opts   = bold_ttl,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 2, 0, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* Single border, title top-center, cyan, padding, multi-line */
    sc_panel_str("Single border\nSecond line\nThird line", (ScPanelOpts){
        .border       = SC_BORDER_SINGLE,
        .border_color = SC_ANSI_COLOR_CYAN,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Info",
        .title_opts   = bold_ttl,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_CENTER,
        .padding = {1, 2, 1, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* Double border, title bottom-right, yellow */
    sc_panel_str("Double border", (ScPanelOpts){
        .border       = SC_BORDER_DOUBLE,
        .border_color = SC_ANSI_COLOR_YELLOW,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Warning",
        .title_opts   = bold_ttl,
        .title_pos    = SC_TITLE_BOTTOM,
        .title_align  = SC_ALIGN_RIGHT,
        .padding = {0, 2, 0, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* Rounded border, no title, RGB border color */
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        .border       = SC_BORDER_ROUNDED,
        .border_color = sc_ansi_color_from_rgb(180, 100, 255),
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 2, 0, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* Thick border, title top-left, green */
    sc_panel_str("Thick border", (ScPanelOpts){
        .border       = SC_BORDER_THICK,
        .border_color = SC_ANSI_COLOR_GREEN,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = "Thick",
        .title_opts   = bold_ttl,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {0, 2, 0, 2},
        .title_pad    = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    printf("\n");

    /* ScText content with mixed styles inside a panel */
    ScTextStyle on_blue = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_GREEN };

    ScText *t = sc_text_new();
    sc_text_append(t, "Normal ",    plain);
    sc_text_append(t, "bold ",      bold);
    sc_text_append(t, "and red\n",  red);
    sc_text_append(t, "second line", plain);
    sc_panel_text(t, (ScPanelOpts){
        .border       = SC_BORDER_ROUNDED,
        .border_color = SC_ANSI_COLOR_GREEN,
        .border_bg    = SC_ANSI_COLOR_NONE,
        .bg           = SC_ANSI_COLOR_NONE,
        .title        = " Styled Content ",
        .title_opts   = on_blue,
        .title_pos    = SC_TITLE_TOP,
        .title_align  = SC_ALIGN_LEFT,
        .padding = {1, 2, 1, 2},
        .title_pad    = 0,
        .content_align = SC_ALIGN_LEFT,
    });
    sc_text_free(t);

    printf("\n");

    /* Panel with content background color */
    sc_panel_str("Content has a dark blue background.", (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = SC_ANSI_COLOR_CYAN,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = sc_ansi_color_from_rgb(20, 30, 60),
        .title         = " With bg ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 1,
        .padding = {1, 2, 1, 2},
        .content_align = SC_ALIGN_LEFT,
        .full_width    = 1,
    });

    printf("\n");

    /* Panel with border background color */
    sc_panel_str("Border itself is highlighted.", (ScPanelOpts){
        .border        = SC_BORDER_THICK,
        .border_color  = SC_ANSI_COLOR_WHITE,
        .border_bg     = sc_ansi_color_from_rgb(80, 0, 120),
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = " border_bg ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_CENTER,
        .title_pad     = 1,
        .padding = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
        .full_width    = 1,
    });

    printf("\n");

    /* Full-width panel, content centered */
    sc_panel_str("This panel stretches to the full terminal width.\nContent is centered.", (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = SC_ANSI_COLOR_CYAN,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = " Full Width ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_CENTER,
        .padding = {0, 2, 0, 2},
        .title_pad     = 1,
        .content_align = SC_ALIGN_CENTER,
        .full_width    = 1,
    });

    printf("\n");

    /* Full-width panel, content right-aligned */
    sc_panel_str("Right-aligned text.\nSecond line.", (ScPanelOpts){
        .border        = SC_BORDER_SINGLE,
        .border_color  = SC_ANSI_COLOR_NONE,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = " Right-Aligned Content ",
        .title_opts    = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_RIGHT,
        .padding = {0, 1, 0, 1},
        .title_pad     = 1,
        .content_align = SC_ALIGN_RIGHT,
        .full_width    = 1,
    });
}
