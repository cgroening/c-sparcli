#include "sparcli.h"
#include <stdio.h>


void test_styles(void);
void test_colors(void);
void test_panels(void);
void test_tables(void);


int main(void) {
    test_styles();
    test_colors();
    test_panels();
    test_tables();
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
        SC_TITLE_TOP, SC_ALIGN_LEFT, 1, 0, 0, 1
    });

    printf("\n");

    /* ASCII border, title top-left */
    sc_panel_str("ASCII border", (ScPanelOpts){
        SC_BORDER_ASCII, SC_COLOR_NONE, "Title", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
    });

    printf("\n");

    /* Single border, title top-center, cyan, padding, multi-line */
    sc_panel_str("Single border\nSecond line\nThird line", (ScPanelOpts){
        SC_BORDER_SINGLE, SC_COLOR_CYAN, "Info", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_CENTER, 2, 1, 0, 1
    });

    printf("\n");

    /* Double border, title bottom-right, yellow */
    sc_panel_str("Double border", (ScPanelOpts){
        SC_BORDER_DOUBLE, SC_COLOR_YELLOW, "Warning", bold_ttl,
        SC_TITLE_BOTTOM, SC_ALIGN_RIGHT, 2, 0, 0, 1
    });

    printf("\n");

    /* Rounded border, no title, RGB border color */
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        SC_BORDER_ROUNDED, sc_rgb(180, 100, 255), NULL, plain,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
    });

    printf("\n");

    /* Thick border, title top-left, green */
    sc_panel_str("Thick border", (ScPanelOpts){
        SC_BORDER_THICK, SC_COLOR_GREEN, "Thick", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
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
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 1, 0, 0
    });
    sc_text_free(t);
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
        sc_table_add_col(tb, "Name", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Age",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "City", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Product",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Q1",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Q2",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Q3",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Lang",   (ScColOpts){0,0,12, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
        sc_table_add_col(tb, "Year",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Typed",  (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Status",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Uptime",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Left",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
        sc_table_add_col(tb, "Center", (ScColOpts){0,0,14, SC_ALIGN_CENTER, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Right",  (ScColOpts){0,0,14, SC_ALIGN_RIGHT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Tall Cell", (ScColOpts){0,0,0,  SC_ALIGN_LEFT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Top",       (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Middle",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_MIDDLE});
        sc_table_add_col(tb, "Bottom",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_BOTTOM});
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
        sc_table_add_col(tb, "Fixed=16", (ScColOpts){0,  0,  16, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Min=10",   (ScColOpts){10, 0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Max=8",    (ScColOpts){0,  8,  0,  SC_ALIGN_RIGHT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Auto",     (ScColOpts){0,  0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Animal", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Sound",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Legs",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "OS",     (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
        sc_table_add_col(tb, "Kernel", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Shell",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Rank",   (ScColOpts){0,0,6,  SC_ALIGN_CENTER, SC_VALIGN_TOP});
        sc_table_add_col(tb, "Player", (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
        sc_table_add_col(tb, "Score",  (ScColOpts){0,0,8,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "A", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "B", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "C", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Age",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP});
        sc_table_add_col(tb, "City",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Role",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Team",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP});
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
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP});
        sc_table_add_col(tb, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP});
        sc_table_add_col(tb, "Level", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Alice"),   SC_CELL("9800"), SC_CELL("Expert") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Bob"),     SC_CELL("7650"), SC_CELL("Master") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Carol"),   SC_CELL("6100"), SC_CELL("Expert") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Diana"),   SC_CELL("5300"), SC_CELL("Adept")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Ethan"),   SC_CELL("4200"), SC_CELL("Novice") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }
}
