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
        .border        = { .style = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Title", .style = { .opts = bold_ttl, .align = SC_ALIGN_LEFT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {0, 1, 0, 1},
        .content_align = SC_ALIGN_LEFT,
    });

    // ASCII border, title top-left
    sc_panel_str("ASCII border", (ScPanelOpts){
        .border        = { .style = SC_BORDER_ASCII, .color = SC_ANSI_COLOR_NONE },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Title", .style = { .opts = bold_ttl, .align = SC_ALIGN_LEFT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Single border, title top-center, cyan, multi-line, padding, margin
    sc_panel_str("Panel with padding and margin", (ScPanelOpts){
        .border        = { .style = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_CYAN },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Info", .style = { .opts = bold_ttl, .align = SC_ALIGN_CENTER, .pad = 4 }, .pos = SC_TITLE_TOP },
        .padding       = {2, 4, 2, 4},
        .margin        = {1, 5, 1, 5},
        .content_align = SC_ALIGN_LEFT,
    });

    // Double border, title bottom-right, yellow
    sc_panel_str("Double border", (ScPanelOpts){
        .border        = { .style = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_YELLOW },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Warning", .style = { .opts = bold_ttl, .align = SC_ALIGN_RIGHT, .pad = 1 }, .pos = SC_TITLE_BOTTOM },
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Rounded border, no title, RGB border color
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        .border        = { .style = SC_BORDER_ROUNDED, .color = sc_ansi_color_from_rgb(180, 100, 255) },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .style = { .align = SC_ALIGN_LEFT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {0, 2, 0, 2},
        .content_align = SC_ALIGN_LEFT,
    });

    // Thick border, title top-left, green
    sc_panel_str("Thick border", (ScPanelOpts){
        .border        = { .style = SC_BORDER_THICK, .color = SC_ANSI_COLOR_GREEN },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Thick", .style = { .opts = bold_ttl, .align = SC_ALIGN_LEFT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {0, 2, 0, 2},
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
        .border        = { .style = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_GREEN },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Styled Content", .style = { .opts = on_blue, .align = SC_ALIGN_LEFT, .pad = 0 }, .pos = SC_TITLE_TOP },
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
        .border        = { .style = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_CYAN, .bg = dark_blue },
        .bg            = dark_blue,
        .title         = { .text = "With bg", .style = { .opts = bold_on_dark_blue, .align = SC_ALIGN_LEFT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {1, 2, 1, 2},
        .content_align = SC_ALIGN_LEFT,
        .full_width    = 1,
    });

    // Panel with border background color
    sc_panel_str("Border itself is highlighted.", (ScPanelOpts){
        .border        = { .style = SC_BORDER_THICK, .color = SC_ANSI_COLOR_WHITE, .bg = sc_ansi_color_from_rgb(80, 0, 120) },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "border_bg", .style = { .opts = bold_ttl, .align = SC_ALIGN_CENTER, .pad = 1 }, .pos = SC_TITLE_TOP },
        .content_align = SC_ALIGN_LEFT,
        .padding       = {0, 2, 0, 2},
        .full_width    = 1,
    });

    // Full-width panel, content centered
    {
        char content[] = "This panel stretches to the full terminal width.\n"
                         "Content is centered.";
        sc_panel_str(content, (ScPanelOpts){
            .border        = { .style = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_CYAN },
            .bg            = SC_ANSI_COLOR_NONE,
            .title         = { .text = "Full Width", .style = { .opts = bold_ttl, .align = SC_ALIGN_CENTER, .pad = 1 }, .pos = SC_TITLE_TOP },
            .padding       = {0, 2, 0, 2},
            .content_align = SC_ALIGN_CENTER,
            .full_width    = 1,
        });
    }

    // Full-width panel, content right-aligned
    sc_panel_str("Right-aligned text.\nSecond line.", (ScPanelOpts){
        .border        = { .style = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
        .bg            = SC_ANSI_COLOR_NONE,
        .title         = { .text = "Right-Aligned Content", .style = { .opts = bold_ttl, .align = SC_ALIGN_RIGHT, .pad = 1 }, .pos = SC_TITLE_TOP },
        .padding       = {0, 1, 0, 1},
        .content_align = SC_ALIGN_RIGHT,
        .full_width    = 1,
    });
}
