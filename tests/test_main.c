#include "sparcli.h"
#include <stdio.h>


void test_styles(void);
void test_colors(void);
void test_panels(void);
void test_tables(void);
void test_columns(void);
void test_rules(void);
void test_trees(void);


int main(void) {
    test_styles();
    test_colors();
    test_panels();
    test_tables();
    test_columns();
    test_rules();
    test_trees();
    return 0;
}


void test_styles(void) {
    ScOptions plain       = { SC_STYLE_NONE,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions dim         = { SC_STYLE_DIM,    SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold        = { SC_STYLE_BOLD,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions italic      = { SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions underline   = { SC_STYLE_UNDER,  SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold_italic = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_under = {
        SC_STYLE_BOLD | SC_STYLE_UNDER, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_italic_under = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC | SC_STYLE_UNDER,
        SC_COLOR_NONE,
        SC_COLOR_NONE
    };

    sc_println("Plain text",             plain);
    sc_println("Dim text",               dim);
    sc_println("Bold text",              bold);
    sc_println("Italic text",            italic);
    sc_println("Underlined text",        underline);

    sc_println("", plain);
    sc_println("Bold + Italic",          bold_italic);
    sc_println("Bold + Underlined",      bold_under);
    sc_println("Bold + Italic + Under",  bold_italic_under);
}


void test_colors(void) {
    ScOptions red       = { SC_STYLE_NONE, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions bold_red  = { SC_STYLE_BOLD, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions on_blue   = { SC_STYLE_NONE, SC_COLOR_WHITE, SC_COLOR_BLUE };
    ScOptions cyan_bold = { SC_STYLE_BOLD, SC_COLOR_CYAN,  SC_COLOR_NONE };
    ScOptions green_rgb = {
        SC_STYLE_NONE, sc_rgb(77, 255, 0), SC_COLOR_NONE
    };
    ScOptions rgb_bg    = {
        SC_STYLE_ITALIC, SC_COLOR_WHITE, sc_rgb(80, 0, 120)
    };

    printf("\n");
    sc_println("Red text",               red);
    sc_println("Bold red text",          bold_red);
    sc_println("White on blue",          on_blue);
    sc_println("Bold cyan",              cyan_bold);
    sc_println("RGB orange text",        green_rgb);
    sc_println("Italic white on purple", rgb_bg);
}


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


void test_tables(void) {
    printf("\n");

    /* ── 1. Single border, header row ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Name", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Age",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "City", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Alice"), SC_CELL("30"), SC_CELL("Berlin")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bob"),   SC_CELL("24"), SC_CELL("Hamburg") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Carol"), SC_CELL("35"), SC_CELL("München") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 2. Double border, header row + header col, colored separators ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_DOUBLE, SC_COLOR_CYAN, SC_COLOR_NONE,
                             SC_COLOR_YELLOW, SC_COLOR_YELLOW, 0, 0, 0 },
            .header_row  = 1, .header_col   = 1,
            .header_row_bg = SC_COLOR_NONE, .header_col_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Product",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q1",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q2",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q3",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Apples"),   SC_CELL("120"), SC_CELL("95"),  SC_CELL("140") }, 4);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bananas"),  SC_CELL("80"),  SC_CELL("110"), SC_CELL("70")  }, 4);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Cherries"), SC_CELL("45"),  SC_CELL("60"),  SC_CELL("90")  }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 3. Rounded border, striped rows ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_ROUNDED, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .striped     = 1, .stripe_bg = sc_rgb(40, 40, 60),
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Lang",   (ScColOpts){0,0,12, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Year",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Typed",  (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("C"),      SC_CELL("1972"), SC_CELL("Static")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Python"), SC_CELL("1991"), SC_CELL("Dynamic") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Rust"),   SC_CELL("2010"), SC_CELL("Static")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Lua"),    SC_CELL("1993"), SC_CELL("Dynamic") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Zig"),    SC_CELL("2016"), SC_CELL("Static")  }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 4. ScText cells with mixed styles ── */
    {
        ScOptions plain = { SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions bold  = { SC_STYLE_BOLD, SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions red   = { SC_STYLE_NONE, SC_COLOR_RED,   SC_COLOR_NONE };
        ScOptions green = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE };

        ScText *ok  = sc_text_new();
        sc_text_append(ok, "● ", green);
        sc_text_append(ok, "OK", plain);

        ScText *err = sc_text_new();
        sc_text_append(err, "✗ ", red);
        sc_text_append(err, "FAIL", bold);

        ScText *lbl = sc_text_new();
        sc_text_append(lbl, "web", plain);
        sc_text_append(lbl, "-server", bold);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD | SC_STYLE_UNDER, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Status",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Uptime",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL_T(lbl), SC_CELL_T(ok),  SC_CELL("99.9%")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("database"), SC_CELL_T(err), SC_CELL("72.1%")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("cache"),    SC_CELL_T(ok),  SC_CELL("100.0%") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
        sc_text_free(ok);
        sc_text_free(err);
        sc_text_free(lbl);
    }

    printf("\n");

    /* ── 5. Column alignment: left / center / right ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 2, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Left",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Center", (ScColOpts){0,0,14, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Right",  (ScColOpts){0,0,14, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("abc"),         SC_CELL("abc"),         SC_CELL("abc")         }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("longer text"), SC_CELL("longer text"), SC_CELL("longer text") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("x"),           SC_CELL("x"),           SC_CELL("x")           }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 6. Vertical alignment: top / middle / bottom ── */
    {
        ScOptions p = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScText *tall = sc_text_new();
        sc_text_append(tall, "Line 1\nLine 2\nLine 3\nLine 4", p);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 1,
        });
        sc_table_add_col(tb, "Tall Cell", (ScColOpts){0,0,0,  SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Top",       (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Middle",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_MIDDLE, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Bottom",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_BOTTOM, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_T(tall), SC_CELL("▲ top"), SC_CELL("● mid"), SC_CELL("▼ bot"),
        }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
        sc_text_free(tall);
    }

    printf("\n");

    /* ── 7. Fixed / min / max column widths ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Fixed=16", (ScColOpts){0,  0,  16, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Min=10",   (ScColOpts){10, 0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Max=8",    (ScColOpts){0,  8,  0,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Auto",     (ScColOpts){0,  0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("short"),       SC_CELL("hi"),  SC_CELL("truncated?"), SC_CELL("auto") }, 4);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("a bit longer"), SC_CELL("pad"), SC_CELL("clipped"),   SC_CELL("fits") }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 8. No border ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_NONE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD | SC_STYLE_UNDER, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Animal", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Sound",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Legs",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Dog"),   SC_CELL("Woof"),  SC_CELL("4") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bird"),  SC_CELL("Tweet"), SC_CELL("2") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Snake"), SC_CELL("Hiss"),  SC_CELL("0") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 9. ASCII border ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_ASCII, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("host"),    SC_CELL("localhost") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("port"),    SC_CELL("8080")      }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("timeout"), SC_CELL("30s")       }, 2);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 10. Thick border, golden color ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_THICK, sc_rgb(255,180,0), SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, sc_rgb(255,180,0), SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "OS",     (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Kernel", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Shell",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Linux"),   SC_CELL("6.8"),   SC_CELL("bash") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("macOS"),   SC_CELL("XNU"),   SC_CELL("zsh")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Windows"), SC_CELL("NT 10"), SC_CELL("pwsh") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 11. Double border, title top, cyan ── */
    {
        ScOptions title_opts = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE };
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_DOUBLE, SC_COLOR_CYAN, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title       = " Leaderboard ",
            .title_opts  = title_opts,
            .title_pos   = SC_TITLE_TOP,
            .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1,
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Rank",   (ScColOpts){0,0,6,  SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Player", (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Score",  (ScColOpts){0,0,8,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("1"), SC_CELL("Alice"),   SC_CELL("9800") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("2"), SC_CELL("Bob"),     SC_CELL("7650") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("3"), SC_CELL("Charlie"), SC_CELL("6100") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 12. Single border, cell_pad_x = 0 ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 0, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "A", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "B", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "C", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("1"), SC_CELL("2"), SC_CELL("3") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("4"), SC_CELL("5"), SC_CELL("6") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 13. Nur Innenrahmen (no outer frame) ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 1, 0, 0 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Age",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "City",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Alice"),   SC_CELL("30"), SC_CELL("Berlin")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bob"),     SC_CELL("24"), SC_CELL("Hamburg") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Carol"),   SC_CELL("35"), SC_CELL("München") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Diana"),   SC_CELL("29"), SC_CELL("Köln")    }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 14. Nur Header-Zeile + Header-Spalte (no outer, no inner rows/cols) ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 1, 1, 1 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_col  = 1, .header_col_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Role",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Team",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Alice"), SC_CELL("Engineer"),  SC_CELL("Backend")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bob"),   SC_CELL("Designer"),  SC_CELL("Frontend") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Carol"), SC_CELL("Manager"),   SC_CELL("Platform") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 15. Nur Header-Trennlinie + Stripes, kein weiterer Rahmen ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 1, 1, 1 },
            .header_row  = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .striped     = 1, .stripe_bg = sc_rgb(40, 40, 60),
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Level", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Alice"),   SC_CELL("9800"), SC_CELL("Expert") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bob"),     SC_CELL("7650"), SC_CELL("Master") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Carol"),   SC_CELL("6100"), SC_CELL("Expert") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Diana"),   SC_CELL("5300"), SC_CELL("Adept")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Ethan"),   SC_CELL("4200"), SC_CELL("Novice") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 16. ScText-Zellen mit Farbe und Hintergrundfarbe ── */
    {
        /* Styles */
        ScOptions plain   = { SC_STYLE_NONE,                    SC_COLOR_NONE,    SC_COLOR_NONE };
        ScOptions bold    = { SC_STYLE_BOLD,                    SC_COLOR_NONE,    SC_COLOR_NONE };
        ScOptions dim     = { SC_STYLE_DIM,                     SC_COLOR_NONE,    SC_COLOR_NONE };
        ScOptions b_green = { SC_STYLE_BOLD,                    SC_COLOR_GREEN,   SC_COLOR_NONE };
        ScOptions b_red   = { SC_STYLE_BOLD,                    SC_COLOR_RED,     SC_COLOR_NONE };
        ScOptions b_yel   = { SC_STYLE_BOLD,                    SC_COLOR_YELLOW,  SC_COLOR_NONE };
        ScOptions cyan_bg = { SC_STYLE_BOLD,                    SC_COLOR_WHITE,   sc_rgb(0,80,120) };
        ScOptions mag_bg  = { SC_STYLE_BOLD | SC_STYLE_ITALIC,  SC_COLOR_WHITE,   sc_rgb(100,0,80) };
        ScOptions grn_bg  = { SC_STYLE_NONE,                    SC_COLOR_BLACK,   sc_rgb(60,160,60) };
        ScOptions red_bg  = { SC_STYLE_NONE,                    SC_COLOR_WHITE,   sc_rgb(160,30,30) };
        ScOptions gold    = { SC_STYLE_BOLD,                    sc_rgb(255,180,0),SC_COLOR_NONE };

        /* Zelle: Produktname mit Hintergrundfarbe */
        ScText *c_api = sc_text_new();
        sc_text_append(c_api, "REST ", plain);
        sc_text_append(c_api, "API", cyan_bg);

        ScText *c_db = sc_text_new();
        sc_text_append(c_db, "Data", plain);
        sc_text_append(c_db, "base", mag_bg);

        ScText *c_auth = sc_text_new();
        sc_text_append(c_auth, "Auth", grn_bg);
        sc_text_append(c_auth, " v2", bold);

        /* Zelle: Status */
        ScText *s_ok = sc_text_new();
        sc_text_append(s_ok, "● ", b_green);
        sc_text_append(s_ok, "OK", plain);

        ScText *s_warn = sc_text_new();
        sc_text_append(s_warn, "▲ ", b_yel);
        sc_text_append(s_warn, "WARN", bold);

        ScText *s_err = sc_text_new();
        sc_text_append(s_err, "✗ ", b_red);
        sc_text_append(s_err, "ERROR", bold);

        /* Zelle: Latenz mit Farbkodierung */
        ScText *l_fast = sc_text_new();
        sc_text_append(l_fast, "12 ms", b_green);

        ScText *l_mid = sc_text_new();
        sc_text_append(l_mid, "340 ms", b_yel);

        ScText *l_slow = sc_text_new();
        sc_text_append(l_slow, "2.1 s", b_red);

        /* Zelle: Requests */
        ScText *r_high = sc_text_new();
        sc_text_append(r_high, "18 432", gold);
        sc_text_append(r_high, " req/s", dim);

        ScText *r_mid = sc_text_new();
        sc_text_append(r_mid, "4 201", plain);
        sc_text_append(r_mid, " req/s", dim);

        ScText *r_low = sc_text_new();
        sc_text_append(r_low, "91", b_red);
        sc_text_append(r_low, " req/s", dim);

        /* Zelle: Error-Rate */
        ScText *e_none = sc_text_new();
        sc_text_append(e_none, "0.0 %", grn_bg);

        ScText *e_some = sc_text_new();
        sc_text_append(e_some, "1.3 %", b_yel);

        ScText *e_high = sc_text_new();
        sc_text_append(e_high, "18.7 %", red_bg);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_ROUNDED, sc_rgb(80,80,100), sc_rgb(80,80,100),
                             sc_rgb(80,80,100), sc_rgb(80,80,100), 0, 0, 0 },
            .header_row  = 1, .header_row_bg = sc_rgb(30,30,50),
            .header_opts = { SC_STYLE_BOLD, sc_rgb(180,180,220), SC_COLOR_NONE },
            .cell_pad_x  = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Service",   (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Status",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Latency",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Requests",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Err-Rate",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_T(c_api),  SC_CELL_T(s_ok),   SC_CELL_T(l_fast), SC_CELL_T(r_high), SC_CELL_T(e_none),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_T(c_db),   SC_CELL_T(s_warn),  SC_CELL_T(l_mid),  SC_CELL_T(r_mid),  SC_CELL_T(e_some),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_T(c_auth), SC_CELL_T(s_err),  SC_CELL_T(l_slow), SC_CELL_T(r_low),  SC_CELL_T(e_high),
        }, 5);
        sc_table_print(tb);
        sc_table_free(tb);

        sc_text_free(c_api);  sc_text_free(c_db);   sc_text_free(c_auth);
        sc_text_free(s_ok);   sc_text_free(s_warn); sc_text_free(s_err);
        sc_text_free(l_fast); sc_text_free(l_mid);  sc_text_free(l_slow);
        sc_text_free(r_high); sc_text_free(r_mid);  sc_text_free(r_low);
        sc_text_free(e_none); sc_text_free(e_some); sc_text_free(e_high);
    }

    printf("\n");

    /* ── 17. Word-Wrap: lange Texte umbrechen statt abschneiden ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_ROUNDED, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title       = " Word Wrap ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Component",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Description", (ScColOpts){0,0,24, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    1, SC_COLOR_NONE});
        sc_table_add_col(tb, "Status",      (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_MIDDLE, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Auth Service"),
            SC_CELL("Handles OAuth2 token validation and refresh for all API clients"),
            SC_CELL("OK"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Data Pipeline"),
            SC_CELL("Ingests streaming events from Kafka and writes to the data warehouse"),
            SC_CELL("WARN"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Scheduler"),
            SC_CELL("Runs periodic jobs using a distributed cron-like mechanism"),
            SC_CELL("OK"),
        }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 18. Footer Row: Zusammenfassung / Summenzeile ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .footer_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .footer_row_bg = sc_rgb(30, 40, 30),
            .title       = " Footer Row (Totals) ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Item",  (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Qty",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Price", (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Total", (ScColOpts){0,0,10, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Widget A"),   SC_CELL("3"), SC_CELL("$ 12.00"), SC_CELL("$  36.00"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Gadget B"),   SC_CELL("1"), SC_CELL("$ 49.99"), SC_CELL("$  49.99"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Doohickey"),  SC_CELL("5"), SC_CELL("$  4.50"), SC_CELL("$  22.50"),
        }, 4);
        sc_table_add_footer_row(tb, (ScCell[]){
            SC_CELL("TOTAL"), SC_CELL("9"), SC_CELL(""), SC_CELL("$ 108.49"),
        }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 19. Per-Row Background: einzelne Zeilen hervorheben ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title       = " Per-Row Background ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Build",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Branch", (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Result", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Time",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("#101"), SC_CELL("main"),      SC_CELL("PASS"), SC_CELL("1m 23s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#102"), SC_CELL("feature/x"), SC_CELL("FAIL"), SC_CELL("0m 47s"),
        }, 4, sc_rgb(60, 20, 20));
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("#103"), SC_CELL("main"),      SC_CELL("PASS"), SC_CELL("1m 31s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#104"), SC_CELL("hotfix/y"),  SC_CELL("PASS"), SC_CELL("2m 05s"),
        }, 4, sc_rgb(20, 50, 20));
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#105"), SC_CELL("release/2"), SC_CELL("FAIL"), SC_CELL("3m 12s"),
        }, 4, sc_rgb(60, 20, 20));
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 20. Per-Column Background: Spalten farblich hervorheben ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = sc_rgb(30, 30, 50),
            .header_opts   = { SC_STYLE_BOLD, sc_rgb(200, 200, 255), SC_COLOR_NONE },
            .title         = " Per-Column Background ",
            .title_opts    = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos     = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad     = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Feature",    (ScColOpts){0,0,18, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Free",       (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Pro",        (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_rgb(20, 45, 90)});
        sc_table_add_col(tb, "Enterprise", (ScColOpts){0,0,12, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_rgb(60, 40, 0)});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Storage"),       SC_CELL("5 GB"),      SC_CELL("50 GB"),      SC_CELL("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("API calls/mo"),  SC_CELL("1 000"),     SC_CELL("50 000"),     SC_CELL("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Team members"),  SC_CELL("1"),         SC_CELL("25"),         SC_CELL("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Support"),       SC_CELL("—"),         SC_CELL("Email"),      SC_CELL("24/7"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Custom domain"), SC_CELL("✗"),         SC_CELL("✓"),          SC_CELL("✓"),
        }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 21. Total Width: Spalten proportional auf feste Gesamtbreite skalieren ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .total_width = 60,
            .title       = " Total Width = 60 cols ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_MAGENTA, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Country",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Capital",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Population", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Germany"),     SC_CELL("Berlin"),    SC_CELL("83M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("France"),      SC_CELL("Paris"),     SC_CELL("68M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Netherlands"), SC_CELL("Amsterdam"), SC_CELL("18M") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 22. Colspan: Zellen über mehrere Spalten spannen ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_YELLOW, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title       = " Colspan ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Name", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q1",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q2",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q3",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Q4",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        /* spanning row: "H1" spans Q1+Q2, "H2" spans Q3+Q4 */
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL(""),
            SC_CELL_CS("H1 (Q1+Q2)", 2), SC_CELL_SKIP,
            SC_CELL_CS("H2 (Q3+Q4)", 2), SC_CELL_SKIP,
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Alice"),
            SC_CELL("120"), SC_CELL("95"), SC_CELL("140"), SC_CELL("110"),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Bob"),
            SC_CELL("80"), SC_CELL("110"), SC_CELL("70"), SC_CELL("95"),
        }, 5);
        /* full-width spanning note */
        sc_table_add_row(tb, (ScCell[]){
           SC_CELL_CSA("* values in units sold", 5, SC_ALIGN_CENTER),
            SC_CELL_SKIP, SC_CELL_SKIP, SC_CELL_SKIP, SC_CELL_SKIP,
        }, 5);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 23. Rowspan: Zellen über mehrere Zeilen spannen ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title         = " Rowspan ",
            .title_opts    = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos     = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad     = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Category", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_MIDDLE, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Item",     (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Price",    (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        /* "Fruits" spans 3 rows */
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_RS("Fruits", 3), SC_CELL("Apple"),  SC_CELL("$  0.50"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_ROW_SKIP,             SC_CELL("Banana"), SC_CELL("$  0.30"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_ROW_SKIP,             SC_CELL("Cherry"), SC_CELL("$  2.00"),
        }, 3);
        /* "Veggies" spans 2 rows */
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_RS("Veggies", 2), SC_CELL("Carrot"), SC_CELL("$  0.80"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_ROW_SKIP,              SC_CELL("Pepper"), SC_CELL("$  1.20"),
        }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 24. RTL: Spalten von rechts nach links ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .rtl         = 1,
            .title       = " RTL Column Order ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "Col A", (ScColOpts){0,0,10, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Col B", (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Col C", (ScColOpts){0,0,10, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("Alpha"), SC_CELL("Beta"), SC_CELL("Gamma"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("1"), SC_CELL("2"), SC_CELL("3"),
        }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 25. Max Rows: Tabelle auf n Zeilen begrenzen + Indikator ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                               SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row    = 1, .header_row_bg = SC_COLOR_NONE,
            .header_opts   = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .max_rows    = 4,
            .title       = " Max Rows = 4 (10 total) ",
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_RED, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .title_pad   = 1, .cell_pad_x = 1, .cell_pad_y = 0,
        });
        sc_table_add_col(tb, "#",     (ScColOpts){0,0,4,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Score", (ScColOpts){0,0,7,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        /* Strings must outlive sc_table_print; use per-row arrays rather than loop-local buffers. */
        char row_idx[10][4], row_name[10][12], row_score[10][8];
        for (int i = 1; i <= 10; i++) {
            snprintf(row_idx[i-1],   sizeof(row_idx[i-1]),   "%d", i);
            snprintf(row_name[i-1],  sizeof(row_name[i-1]),  "Player %d", i);
            snprintf(row_score[i-1], sizeof(row_score[i-1]), "%d", 10000 - i * 850);
            sc_table_add_row(tb, (ScCell[]){
                SC_CELL(row_idx[i-1]), SC_CELL(row_name[i-1]), SC_CELL(row_score[i-1]),
            }, 3);
        }
        sc_table_print(tb);
        sc_table_free(tb);
    }
}


void test_columns(void) {
    printf("\n");

    /* ── 1. Zwei Tabellen nebeneinander ── */
    {
        ScTable *t1 = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Team A ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t1, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t1, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Alice"), SC_CELL("9800") }, 2);
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Bob"),   SC_CELL("7650") }, 2);
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Carol"), SC_CELL("6100") }, 2);

        ScTable *t2 = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Team B ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_MAGENTA, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t2, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t2, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Dave"),  SC_CELL("8200") }, 2);
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Eve"),   SC_CELL("7100") }, 2);
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Frank"), SC_CELL("5400") }, 2);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 3 });
        sc_columns_add_table(cl, t1, (ScColItem){0});
        sc_columns_add_table(cl, t2, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(t1);
        sc_table_free(t2);
    }

    printf("\n");

    /* ── 2. Panel + Tabelle mit Trennlinie ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_ROUNDED, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x = 1,
        });
        sc_table_add_col(tb, "Port",    (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "State",   (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("22"),   SC_CELL("ssh"),   SC_CELL("open")   }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("80"),   SC_CELL("http"),  SC_CELL("open")   }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("443"),  SC_CELL("https"), SC_CELL("open")   }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("3306"), SC_CELL("mysql"), SC_CELL("closed") }, 3);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_NONE,
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(cl,
            "Host: 192.168.1.1\nOS: Linux 6.8\nUptime: 42 days\nCPU: 4 cores\nRAM: 16 GB",
            (ScPanelOpts){
                .border       = SC_BORDER_ROUNDED,
                .border_color = SC_COLOR_GREEN,
                .title        = " System Info ",
                .title_opts   = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
                .title_pos    = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
                .pad_x        = 2, .title_pad = 1,
            },
            (ScColItem){0});
        sc_columns_add_table(cl, tb, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 3. valign: drei Panels unterschiedlicher Höhe ── */
    {
        ScPanelOpts po_top = {
            .border = SC_BORDER_SINGLE, .border_color = SC_COLOR_NONE,
            .title = " Top ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER,
            .pad_x = 1, .title_pad = 1,
        };
        ScPanelOpts po_mid = po_top;
        po_mid.title = " Middle ";
        po_mid.border_color = SC_COLOR_NONE;
        ScPanelOpts po_bot = po_top;
        po_bot.title = " Bottom ";

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap    = 4,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C\nLine D\nLine E", po_top,  (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C",                  po_mid, (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A",                                   po_bot, (ScColItem){.fixed_w=18});
        sc_columns_print(cl);
        sc_columns_free(cl);

        printf("\n");

        cl = sc_columns_new((ScColumnsOpts){ .gap = 4, .valign = SC_VALIGN_MIDDLE });
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C\nLine D\nLine E", po_top,  (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C",                  po_mid, (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A",                                   po_bot, (ScColItem){.fixed_w=18});
        sc_columns_print(cl);
        sc_columns_free(cl);

        printf("\n");

        cl = sc_columns_new((ScColumnsOpts){ .gap = 4, .valign = SC_VALIGN_BOTTOM });
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C\nLine D\nLine E", po_top,  (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A\nLine B\nLine C",                  po_mid, (ScColItem){.fixed_w=18});
        sc_columns_add_panel_str(cl, "Line A",                                   po_bot, (ScColItem){.fixed_w=18});
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 4. total_width auf drei flex-Spalten verteilt ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap         = 1,
            .sep_style   = SC_BORDER_SINGLE,
            .sep_color   = SC_COLOR_NONE,
            .total_width = 60,
        });
        sc_columns_add_str(cl, "Left column\nwith two lines",    (ScColItem){ .align = SC_ALIGN_LEFT   });
        sc_columns_add_str(cl, "Centered column",                (ScColItem){ .align = SC_ALIGN_CENTER });
        sc_columns_add_str(cl, "Right column\nalso two lines",   (ScColItem){ .align = SC_ALIGN_RIGHT  });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 5. Verschachtelte Columns ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x = 1,
        });
        sc_table_add_col(tb, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("env"),     SC_CELL("production") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("region"),  SC_CELL("eu-west-1")  }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("version"), SC_CELL("2.4.1")      }, 2);

        /* inner: two string columns stacked */
        ScColumns *inner = sc_columns_new((ScColumnsOpts){ .gap = 2, .valign = SC_VALIGN_TOP });
        sc_columns_add_str(inner, "CPU:  34 %\nMEM:  61 %\nDISK: 48 %", (ScColItem){.fixed_w=14});
        sc_columns_add_str(inner, "NET-IN:  1.2 MB/s\nNET-OUT: 0.4 MB/s\nPKTS:    12 403",
                           (ScColItem){.fixed_w=18});

        ScColumns *outer = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_NONE,
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_table(outer, tb, (ScColItem){0});
        sc_columns_add_columns(outer, inner, (ScColItem){0});
        sc_columns_print(outer);

        sc_columns_free(outer);
        sc_columns_free(inner);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 6. min_w / max_w / fixed_w + item.align ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .sep_style = SC_BORDER_ROUNDED,
            .sep_color = SC_COLOR_NONE,
        });
        sc_columns_add_str(cl, "fixed=20\nshort",
                           (ScColItem){ .fixed_w = 20, .align = SC_ALIGN_LEFT   });
        sc_columns_add_str(cl, "min=12, auto-width",
                           (ScColItem){ .min_w   = 12, .align = SC_ALIGN_CENTER });
        sc_columns_add_str(cl, "max=10, this text is longer than the limit",
                           (ScColItem){ .max_w   = 10, .align = SC_ALIGN_RIGHT  });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 7. Separator: double border, farbig ── */
    {
        ScTable *t1 = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Revenue ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t1, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t1, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Q1"), SC_CELL("1 240 000") }, 2);
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Q2"), SC_CELL("1 580 000") }, 2);
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Q3"), SC_CELL("1 390 000") }, 2);
        sc_table_add_row(t1, (ScCell[]){ SC_CELL("Q4"), SC_CELL("1 820 000") }, 2);

        ScTable *t2 = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Costs ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t2, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t2, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Q1"), SC_CELL("980 000")   }, 2);
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Q2"), SC_CELL("1 100 000") }, 2);
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Q3"), SC_CELL("870 000")   }, 2);
        sc_table_add_row(t2, (ScCell[]){ SC_CELL("Q4"), SC_CELL("1 250 000") }, 2);

        ScTable *t3 = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1, .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Profit ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t3, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t3, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t3, (ScCell[]){ SC_CELL("Q1"), SC_CELL("260 000") }, 2);
        sc_table_add_row(t3, (ScCell[]){ SC_CELL("Q2"), SC_CELL("480 000") }, 2);
        sc_table_add_row(t3, (ScCell[]){ SC_CELL("Q3"), SC_CELL("520 000") }, 2);
        sc_table_add_row(t3, (ScCell[]){ SC_CELL("Q4"), SC_CELL("570 000") }, 2);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep_style = SC_BORDER_DOUBLE,
            .sep_color = sc_rgb(80, 80, 140),
        });
        sc_columns_add_table(cl, t1, (ScColItem){0});
        sc_columns_add_table(cl, t2, (ScColItem){0});
        sc_columns_add_table(cl, t3, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(t1);
        sc_table_free(t2);
        sc_table_free(t3);
    }
}


void test_rules(void) {
    printf("\n");

    /* ── 1. Verschiedene Linienstile, keine Farbe ── */
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_ASCII,   .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_SINGLE,  .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_DOUBLE,  .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_ROUNDED, .color = SC_COLOR_NONE });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_THICK,   .color = SC_COLOR_NONE });

    printf("\n");

    /* ── 2. Linienstile mit Farbe ── */
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_SINGLE, .color = SC_COLOR_CYAN    });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_DOUBLE, .color = SC_COLOR_YELLOW  });
    sc_rule_str(NULL, (ScRuleOpts){ .style = SC_BORDER_THICK,  .color = sc_rgb(180,60,60) });

    printf("\n");

    /* ── 3. Titel: Ausrichtung links / mitte / rechts ── */
    sc_rule_str("Links",  (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_LEFT,
    });
    sc_rule_str("Mitte",  (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
    });
    sc_rule_str("Rechts", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 4. Titel mit Linien- und Textfarbe ── */
    sc_rule_str("Section A", (ScRuleOpts){
        .style       = SC_BORDER_SINGLE,
        .color       = SC_COLOR_CYAN,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .title_pad   = 2,
    });
    sc_rule_str("Warning", (ScRuleOpts){
        .style       = SC_BORDER_DOUBLE,
        .color       = SC_COLOR_YELLOW,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
    });
    sc_rule_str("Error", (ScRuleOpts){
        .style       = SC_BORDER_THICK,
        .color       = sc_rgb(200, 50, 50),
        .title_opts  = { SC_STYLE_BOLD, sc_rgb(200, 50, 50), SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .title_pad   = 2,
    });

    printf("\n");

    /* ── 5. Feste Breite mit Ausrichtung im Terminal ── */
    sc_rule_str("left-aligned rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_LEFT,
    });
    sc_rule_str("centered rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_CENTER,
    });
    sc_rule_str("right-aligned rule", (ScRuleOpts){
        .style = SC_BORDER_SINGLE, .color = SC_COLOR_NONE,
        .title_opts  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .width = 40, .align = SC_ALIGN_RIGHT,
    });

    printf("\n");

    /* ── 6. Margin und pad_y ── */
    sc_rule_str("mit Margin und pad_y", (ScRuleOpts){
        .style       = SC_BORDER_ROUNDED,
        .color       = SC_COLOR_GREEN,
        .title_opts  = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
        .title_align = SC_ALIGN_CENTER,
        .margin      = 8,
        .pad_y       = 1,
    });

    /* ── 7. ScText-Variante mit mehreren Spans ── */
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Status: ", (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE });
        sc_text_append(t, "OK",       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE });
        sc_rule_text(t, (ScRuleOpts){
            .style       = SC_BORDER_SINGLE,
            .color       = SC_COLOR_NONE,
            .title_align = SC_ALIGN_CENTER,
        });
        sc_text_free(t);
    }
}


void test_trees(void) {
    /* shorthand opts */
    static const ScOptions PLAIN = { SC_STYLE_NONE,   SC_COLOR_NONE,  SC_COLOR_NONE };
    static const ScOptions BOLD  = { SC_STYLE_BOLD,   SC_COLOR_NONE,  SC_COLOR_NONE };

    printf("\n");

    /* ── 1. Verzeichnisstruktur (SINGLE, colored connectors) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = SC_COLOR_NONE,
        });
        ScOptions dir  = { SC_STYLE_BOLD,   SC_COLOR_CYAN,  SC_COLOR_NONE };
        ScOptions file = { SC_STYLE_NONE,   SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions ic_d = { SC_STYLE_NONE,   SC_COLOR_CYAN,  SC_COLOR_NONE };
        ScOptions ic_f = { SC_STYLE_NONE,   SC_COLOR_NONE,  SC_COLOR_NONE };

        ScTreeNode *src  = sc_tree_add_str(t, NULL, "src/",     dir,  "▸ ", ic_d);
        ScTreeNode *inc  = sc_tree_add_str(t, NULL, "include/", dir,  "▸ ", ic_d);
        ScTreeNode *tst  = sc_tree_add_str(t, NULL, "tests/",   dir,  "▸ ", ic_d);
                          sc_tree_add_str(t, NULL, "Makefile",  file, "· ", ic_f);

        sc_tree_add_str(t, src, "main.c",    file, "· ", ic_f);
        sc_tree_add_str(t, src, "panel.c",   file, "· ", ic_f);
        sc_tree_add_str(t, src, "table.c",   file, "· ", ic_f);
        sc_tree_add_str(t, src, "tree.c",    file, "· ", ic_f);

        sc_tree_add_str(t, inc, "sparcli.h", file, "· ", ic_f);

        ScTreeNode *fix = sc_tree_add_str(t, tst, "fixtures/", dir,  "▸ ", ic_d);
        sc_tree_add_str(t, tst, "test_main.c", file, "· ", ic_f);
        sc_tree_add_str(t, fix, "sample.json", file, "· ", ic_f);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 2. Kategorie-Baum, mehrere Roots (ROUNDED) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_ROUNDED,
            .connector_color = sc_rgb(100, 100, 100),
        });
        ScOptions root_o = { SC_STYLE_BOLD,  SC_COLOR_WHITE,   SC_COLOR_NONE };
        ScOptions cat_o  = { SC_STYLE_NONE,  SC_COLOR_YELLOW,  SC_COLOR_NONE };
        ScOptions item_o = { SC_STYLE_NONE,  SC_COLOR_NONE,    SC_COLOR_NONE };

        ScTreeNode *fe = sc_tree_add_str(t, NULL, "Frontend", root_o, NULL, PLAIN);
        ScTreeNode *be = sc_tree_add_str(t, NULL, "Backend",  root_o, NULL, PLAIN);
        ScTreeNode *db = sc_tree_add_str(t, NULL, "Database", root_o, NULL, PLAIN);

        ScTreeNode *react = sc_tree_add_str(t, fe, "React",   cat_o, NULL, PLAIN);
        ScTreeNode *css   = sc_tree_add_str(t, fe, "CSS",     cat_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Components", item_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Hooks",      item_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Context",    item_o, NULL, PLAIN);
        sc_tree_add_str(t, css,   "Tailwind",   item_o, NULL, PLAIN);
        sc_tree_add_str(t, css,   "Modules",    item_o, NULL, PLAIN);

        ScTreeNode *api = sc_tree_add_str(t, be, "REST API", cat_o, NULL, PLAIN);
        sc_tree_add_str(t, be, "Auth",      cat_o, NULL, PLAIN);
        sc_tree_add_str(t, api, "Routes",   item_o, NULL, PLAIN);
        sc_tree_add_str(t, api, "Middleware", item_o, NULL, PLAIN);

        sc_tree_add_str(t, db, "PostgreSQL", item_o, NULL, PLAIN);
        sc_tree_add_str(t, db, "Redis",      item_o, NULL, PLAIN);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 3. Prefix-Icons mit Farbe (Status-Baum) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = sc_rgb(80, 80, 80),
        });
        ScOptions ok_ic  = { SC_STYLE_NONE, SC_COLOR_GREEN,  SC_COLOR_NONE };
        ScOptions err_ic = { SC_STYLE_NONE, SC_COLOR_RED,    SC_COLOR_NONE };
        ScOptions wrn_ic = { SC_STYLE_NONE, SC_COLOR_YELLOW, SC_COLOR_NONE };
        ScOptions ok_tx  = { SC_STYLE_NONE, SC_COLOR_NONE,   SC_COLOR_NONE };
        ScOptions err_tx = { SC_STYLE_BOLD, SC_COLOR_RED,    SC_COLOR_NONE };
        ScOptions wrn_tx = { SC_STYLE_NONE, SC_COLOR_YELLOW, SC_COLOR_NONE };

        ScTreeNode *svc = sc_tree_add_str(t, NULL, "Services",
            (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE }, NULL, PLAIN);
        ScTreeNode *web = sc_tree_add_str(t, svc, "web-server",   ok_tx,  "● ", ok_ic);
        ScTreeNode *api = sc_tree_add_str(t, svc, "api-gateway",  wrn_tx, "◆ ", wrn_ic);
                          sc_tree_add_str(t, svc, "auth-service",  err_tx, "✖ ", err_ic);

        sc_tree_add_str(t, web, "port 80   → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, web, "port 443  → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, api, "port 8080 → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, api, "latency   → high", wrn_tx, "· ", wrn_ic);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 4. Verschiedene Linienstile ── */
    {
        /* ASCII */
        ScTree *ta = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_ASCII, .connector_color = SC_COLOR_NONE });
        ScTreeNode *a1 = sc_tree_add_str(ta, NULL, "root (ASCII)", BOLD, NULL, PLAIN);
        ScTreeNode *a2 = sc_tree_add_str(ta, a1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(ta, a1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(ta, a2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(ta);
        sc_tree_free(ta);
        printf("\n");

        /* DOUBLE */
        ScTree *td = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_DOUBLE, .connector_color = SC_COLOR_NONE });
        ScTreeNode *d1 = sc_tree_add_str(td, NULL, "root (DOUBLE)", BOLD, NULL, PLAIN);
        ScTreeNode *d2 = sc_tree_add_str(td, d1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(td, d1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(td, d2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(td);
        sc_tree_free(td);
        printf("\n");

        /* THICK */
        ScTree *tt = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_THICK, .connector_color = sc_rgb(120, 80, 200) });
        ScTreeNode *t1 = sc_tree_add_str(tt, NULL, "root (THICK, colored)", BOLD, NULL, PLAIN);
        ScTreeNode *t2 = sc_tree_add_str(tt, t1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(tt, t1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(tt, t2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(tt);
        sc_tree_free(tt);
    }

    printf("\n");

    /* ── 5. no_guide = 1 (keine vertikalen Führungslinien) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style    = SC_BORDER_SINGLE,
            .no_guide = 1,
            .connector_color = SC_COLOR_NONE,
        });
        ScTreeNode *r  = sc_tree_add_str(t, NULL, "root", BOLD, NULL, PLAIN);
        ScTreeNode *c1 = sc_tree_add_str(t, r, "child A", PLAIN, NULL, PLAIN);
        ScTreeNode *c2 = sc_tree_add_str(t, r, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(t, r, "child C", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c1, "leaf A1", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c1, "leaf A2", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c2, "leaf B1", PLAIN, NULL, PLAIN);
        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 6. ScText-Variante mit gemischten Spans ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = SC_COLOR_CYAN,
        });

        ScText *root_t = sc_text_new();
        sc_text_append(root_t, "Deployment ", (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE,  SC_COLOR_NONE });
        sc_text_append(root_t, "v2.4.1",      (ScOptions){ SC_STYLE_BOLD, SC_COLOR_CYAN,  SC_COLOR_NONE });
        ScTreeNode *root = sc_tree_add_text(t, NULL, root_t, NULL, PLAIN);

        ScText *ok_t = sc_text_new();
        sc_text_append(ok_t, "build   ", (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE });
        sc_text_append(ok_t, "PASSED",   (ScOptions){ SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE });

        ScText *fail_t = sc_text_new();
        sc_text_append(fail_t, "tests   ", (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append(fail_t, "FAILED",   (ScOptions){ SC_STYLE_BOLD, SC_COLOR_RED,  SC_COLOR_NONE });

        ScText *skip_t = sc_text_new();
        sc_text_append(skip_t, "deploy  ", (ScOptions){ SC_STYLE_NONE,  SC_COLOR_NONE,   SC_COLOR_NONE });
        sc_text_append(skip_t, "SKIPPED", (ScOptions){ SC_STYLE_NONE,  SC_COLOR_YELLOW, SC_COLOR_NONE });

        sc_tree_add_text(t, root, ok_t,   NULL, PLAIN);
        sc_tree_add_text(t, root, fail_t, NULL, PLAIN);
        sc_tree_add_text(t, root, skip_t, NULL, PLAIN);

        sc_tree_print(t);
        sc_text_free(root_t);
        sc_text_free(ok_t);
        sc_text_free(fail_t);
        sc_text_free(skip_t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 7. Tree neben Tabelle in Columns ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = sc_rgb(80, 140, 80),
        });
        ScOptions dir_o  = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE };
        ScOptions file_o = { SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions ic_d   = { SC_STYLE_NONE, SC_COLOR_GREEN, SC_COLOR_NONE };
        ScOptions ic_f   = { SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE };

        ScTreeNode *app  = sc_tree_add_str(tree, NULL, "app/",       dir_o,  "▸ ", ic_d);
        ScTreeNode *lib  = sc_tree_add_str(tree, NULL, "lib/",       dir_o,  "▸ ", ic_d);
        ScTreeNode *comp = sc_tree_add_str(tree, app,  "components/",dir_o,  "▸ ", ic_d);
                           sc_tree_add_str(tree, app,  "main.ts",    file_o, "· ", ic_f);
        sc_tree_add_str(tree, comp, "Button.tsx", file_o, "· ", ic_f);
        sc_tree_add_str(tree, comp, "Input.tsx",  file_o, "· ", ic_f);
        sc_tree_add_str(tree, lib,  "utils.ts",   file_o, "· ", ic_f);
        sc_tree_add_str(tree, lib,  "api.ts",     file_o, "· ", ic_f);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                             SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row  = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " File Stats ", .title_align = SC_ALIGN_CENTER,
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title_pos   = SC_TITLE_TOP, .title_pad = 1,
            .cell_pad_x  = 1,
        });
        sc_table_add_col(tb, "File",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Lines", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Size",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Button.tsx"), SC_CELL("142"), SC_CELL("3.2 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Input.tsx"),  SC_CELL( "89"), SC_CELL("1.8 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("main.ts"),    SC_CELL( "34"), SC_CELL("0.7 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("utils.ts"),   SC_CELL("210"), SC_CELL("4.9 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("api.ts"),     SC_CELL("178"), SC_CELL("3.8 KB") }, 3);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_NONE,
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_tree(cl, tree, (ScColItem){0});
        sc_columns_add_table(cl, tb, (ScColItem){0});
        sc_columns_print(cl);

        sc_columns_free(cl);
        sc_tree_free(tree);
        sc_table_free(tb);
    }
}
