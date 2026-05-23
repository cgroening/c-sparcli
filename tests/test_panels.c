#include "sparcli.h"
#include <stdio.h>


static ScTextStyle plain   = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static ScTextStyle bold    = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static ScTextStyle red     = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,  SC_ANSI_COLOR_NONE
};
static ScTextStyle bold_ttl = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};


void test_panels(void) {
    // No border
    sc_panel_str("No border panel", (ScPanelOpts){
        .border        = SC_BORDER_NONE,
        .border_color  = SC_ANSI_COLOR_NONE,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Title",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 1,
        .padding       = {0, 1, 0, 1},
        .content_align = SC_ALIGN_LEFT,
    });

    // ASCII border, title top-left
    sc_panel_str("ASCII border", (ScPanelOpts){
        .border        = SC_BORDER_ASCII,
        .border_color  = SC_ANSI_COLOR_NONE,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Title",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 1,
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Single border, title top-center, cyan, multi-line, padding, margin
    sc_panel_str("Panel with padding and margin", (ScPanelOpts){
        .border        = SC_BORDER_SINGLE,
        .border_color  = SC_ANSI_COLOR_CYAN,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Info",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_CENTER,
        .title_pad     = 4,
        .padding       = {2, 4, 2, 4},
        .margin        = {1, 5, 1, 5},
        .content_align = SC_ALIGN_LEFT,
    });

    // Double border, title bottom-right, yellow
    sc_panel_str("Double border", (ScPanelOpts){
        .border        = SC_BORDER_DOUBLE,
        .border_color  = SC_ANSI_COLOR_YELLOW,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Warning",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_BOTTOM,
        .title_align   = SC_ALIGN_RIGHT,
        .title_pad     = 1,
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Rounded border, no title, RGB border color
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = sc_ansi_color_from_rgb(180, 100, 255),
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 1,
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Thick border, title top-left, green
    sc_panel_str("Thick border", (ScPanelOpts){
        .border        = SC_BORDER_THICK,
        .border_color  = SC_ANSI_COLOR_GREEN,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Thick",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .padding       = {0, 2, 0, 2},
        .title_pad     = 1,
        .content_align = SC_ALIGN_LEFT,
    });

    // ScText content with mixed styles inside a panel
    ScTextStyle on_blue = {
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_GREEN
    };

    ScText *t = sc_text_new();
    sc_text_append(t, "Normal ",    plain);
    sc_text_append(t, "bold ",      bold);
    sc_text_append(t, "and red\n",  red);
    sc_text_append(t, "second line", plain);
    sc_panel_text(t, (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = SC_ANSI_COLOR_GREEN,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Styled Content",
        .title_style   = on_blue,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 0,
        .padding       = {1, 2, 1, 2},
        .content_align = SC_ALIGN_LEFT,
    });
    sc_text_free(t);

    // Panel with content background color
    ScColor dark_blue = sc_ansi_color_from_rgb(20, 30, 60);
    ScTextStyle bold_on_dark_blue = {
        SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE, dark_blue
    };
    sc_panel_str("Content has a dark blue background.", (ScPanelOpts){
        .border        = SC_BORDER_ROUNDED,
        .border_color  = SC_ANSI_COLOR_CYAN,
        .border_bg     = dark_blue,
        .bg            = dark_blue,
        .title         = "With bg",
        .title_style   = bold_on_dark_blue,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_LEFT,
        .title_pad     = 1,
        .padding       = {1, 2, 1, 2},
        .content_align = SC_ALIGN_LEFT,
        .full_width    = 1,
    });

    // Panel with border background color
    sc_panel_str("Border itself is highlighted.", (ScPanelOpts){
        .border        = SC_BORDER_THICK,
        .border_color  = SC_ANSI_COLOR_WHITE,
        .border_bg     = sc_ansi_color_from_rgb(80, 0, 120),
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "border_bg",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_CENTER,
        .title_pad     = 1,
        .content_align = SC_ALIGN_LEFT,
        .padding       = {0, 2, 0, 2},
        .full_width    = 1,
    });

    // Full-width panel, content centered
    {
        char content[] = "This panel stretches to the full terminal width.\n"
                         "Content is centered.";
        sc_panel_str(content, (ScPanelOpts){
            .border        = SC_BORDER_ROUNDED,
            .border_color  = SC_ANSI_COLOR_CYAN,
            .border_bg     = SC_ANSI_COLOR_NONE,
            .bg            = SC_ANSI_COLOR_NONE,
            .title         = "Full Width",
            .title_style   = bold_ttl,
            .title_pos     = SC_TITLE_TOP,
            .title_align   = SC_ALIGN_CENTER,
            .title_pad     = 1,
            .padding       = {0, 2, 0, 2},
            .content_align = SC_ALIGN_CENTER,
            .full_width    = 1,
        });
    }

    // Full-width panel, content right-aligned
    sc_panel_str("Right-aligned text.\nSecond line.", (ScPanelOpts){
        .border        = SC_BORDER_SINGLE,
        .border_color  = SC_ANSI_COLOR_NONE,
        .border_bg     = SC_ANSI_COLOR_NONE,
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = "Right-Aligned Content",
        .title_style   = bold_ttl,
        .title_pos     = SC_TITLE_TOP,
        .title_align   = SC_ALIGN_RIGHT,
        .title_pad     = 1,
        .padding       = {0, 1, 0, 1},
        .content_align = SC_ALIGN_RIGHT,
        .full_width    = 1,
    });
}
