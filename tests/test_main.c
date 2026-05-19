#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>   /* free */
#include <string.h>   /* strlen */
#include <unistd.h>   /* usleep */


void test_styles(void);
void test_colors(void);
void test_panels(void);
void test_tables(void);
void test_columns(void);
void test_rules(void);
void test_trees(void);
void test_lists(void);
void test_progressbar(void);
void test_progressbar_animated(void);
void test_spinner(void);
void test_spinner_animated(void);
void test_kv(void);
void test_alert(void);
void test_badge(void);
void test_util(void);
void test_pad(void);
void test_align(void);
void test_markup(void);


int main(void) {
    test_styles();
    test_colors();
    test_panels();
    test_tables();
    test_columns();
    test_rules();
    test_trees();
    test_lists();
    test_progressbar();
    // test_progressbar_animated();
    test_spinner();
    // test_spinner_animated();
    test_kv();
    test_alert();
    test_badge();
    test_util();
    test_pad();
    test_align();
    test_markup();
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

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Lists ━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void test_lists(void) {
    printf("\n\n══════════════════════════  LISTS  ══════════════════════════\n\n");

    /* ── 1. Bullet list (default •) ── */
    {
        printf("--- 1. Bullet list (default •) ---\n");
        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_BULLET,
            .indent   = 2,
            .item_gap = 0,
        });
        ScOptions none = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        sc_list_add_str(l, "Install dependencies", none);
        sc_list_add_str(l, "Configure the environment", none);
        sc_list_add_str(l, "Run the build pipeline", none);
        sc_list_add_str(l, "Deploy to production", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 2. Bullet with custom character and colored marker ── */
    {
        printf("\n--- 2. Custom bullet (→) with colored marker ---\n");
        ScOptions bold_green = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE };
        ScOptions none       = { SC_STYLE_NONE, SC_COLOR_NONE,  SC_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "\xe2\x86\x92",   /* → */
            .marker_opts = bold_green,
            .indent      = 2,
        });
        sc_list_add_str(l, "Frontend: React + TypeScript", none);
        sc_list_add_str(l, "Backend: Go + PostgreSQL", none);
        sc_list_add_str(l, "Infrastructure: Kubernetes on AWS", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 3. Numbered list with word-wrap ── */
    {
        printf("\n--- 3. Numbered list with word-wrap ---\n");
        ScOptions none = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .indent        = 2,
            .width         = 60,
        });
        sc_list_add_str(l,
            "Read the full documentation before starting so that you understand "
            "all the configuration options available.", none);
        sc_list_add_str(l,
            "Back up your existing configuration files, including any custom "
            "settings you have made to the environment.", none);
        sc_list_add_str(l,
            "Run the migration script and verify the output carefully.", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 4. Alpha lowercase with prefix/suffix ── */
    {
        printf("\n--- 4. Alpha lowercase (a) b) c)) ---\n");
        ScOptions none = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_ALPHA_LC,
            .marker_prefix = "",
            .marker_suffix = ")",
            .indent        = 2,
        });
        sc_list_add_str(l, "Define the problem clearly", none);
        sc_list_add_str(l, "Gather relevant data and context", none);
        sc_list_add_str(l, "Propose at least three solutions", none);
        sc_list_add_str(l, "Evaluate trade-offs for each approach", none);
        sc_list_add_str(l, "Select and implement the best option", none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 5. Roman numeral uppercase ── */
    {
        printf("\n--- 5. Roman numerals uppercase (I. II. III.) ---\n");
        ScOptions bold = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };
        ScOptions none = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_ROMAN_UC,
            .marker_suffix = ".",
            .marker_opts   = bold,
            .indent        = 2,
            .item_gap      = 1,
        });
        sc_list_add_str(l, "Introduction",      none);
        sc_list_add_str(l, "Methodology",       none);
        sc_list_add_str(l, "Results",           none);
        sc_list_add_str(l, "Discussion",        none);
        sc_list_add_str(l, "Conclusion",        none);
        sc_list_add_str(l, "References",        none);
        sc_list_add_str(l, "Appendix",          none);
        sc_list_add_str(l, "Acknowledgements",  none);
        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 6. Nested lists ── */
    {
        printf("\n--- 6. Nested lists ---\n");
        ScOptions none  = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScOptions bold  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };
        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .marker_opts   = bold,
            .indent   = 2,
        });

        ScListItem *it1 = sc_list_add_str(l, "Backend", none);
        ScList *sub1 = sc_list_add_sub(it1, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",  /* – */
            .indent  = 2,
        });
        sc_list_add_str(sub1, "REST API (Go)",       none);
        sc_list_add_str(sub1, "Database (Postgres)", none);
        sc_list_add_str(sub1, "Cache (Redis)",       none);

        ScListItem *it2 = sc_list_add_str(l, "Frontend", none);
        ScList *sub2 = sc_list_add_sub(it2, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",
            .indent  = 2,
        });
        sc_list_add_str(sub2, "React components",    none);
        sc_list_add_str(sub2, "State management",    none);

        /* deeper nesting */
        ScListItem *it2b = sc_list_add_str(sub2, "Build tooling", none);
        ScList *sub2b = sc_list_add_sub(it2b, (ScListOpts){
            .marker  = SC_LIST_ALPHA_LC,
            .marker_suffix = ".",
            .indent  = 2,
        });
        sc_list_add_str(sub2b, "Vite",    none);
        sc_list_add_str(sub2b, "ESLint",  none);
        sc_list_add_str(sub2b, "Prettier",none);

        ScListItem *it3 = sc_list_add_str(l, "DevOps", none);
        ScList *sub3 = sc_list_add_sub(it3, (ScListOpts){
            .marker  = SC_LIST_BULLET,
            .bullet  = "\xe2\x80\x93",
            .indent  = 2,
        });
        sc_list_add_str(sub3, "Docker + Kubernetes", none);
        sc_list_add_str(sub3, "GitHub Actions CI",   none);

        sc_list_print(l);
        sc_list_free(l);
    }

    /* ── 7. ScText items ── */
    {
        printf("\n--- 7. ScText items (mixed styles per item) ---\n");
        ScOptions none    = { SC_STYLE_NONE,   SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions bold    = { SC_STYLE_BOLD,   SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions dim     = { SC_STYLE_DIM,    SC_COLOR_NONE,  SC_COLOR_NONE };
        ScOptions keyword = { SC_STYLE_BOLD,   SC_COLOR_CYAN,  SC_COLOR_NONE };

        ScList *l = sc_list_new((ScListOpts){
            .marker   = SC_LIST_BULLET,
            .indent   = 2,
        });

        ScText *t1 = sc_text_new();
        sc_text_append(t1, "Use ", none);
        sc_text_append(t1, "const", keyword);
        sc_text_append(t1, " for values that never change", none);

        ScText *t2 = sc_text_new();
        sc_text_append(t2, "Prefer ", none);
        sc_text_append(t2, "explicit types", bold);
        sc_text_append(t2, " over inference in public APIs", dim);

        ScText *t3 = sc_text_new();
        sc_text_append(t3, "Run ", none);
        sc_text_append(t3, "make test", keyword);
        sc_text_append(t3, " before every commit", none);

        sc_list_add_text(l, t1);
        sc_list_add_text(l, t2);
        sc_list_add_text(l, t3);

        sc_list_print(l);

        sc_list_free(l);
        sc_text_free(t1);
        sc_text_free(t2);
        sc_text_free(t3);
    }

    /* ── 8. List in columns (two lists side by side) ── */
    {
        printf("\n--- 8. Two lists in columns ---\n");
        ScOptions none = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
        ScOptions bold = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };

        ScList *pros = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "+",
            .marker_opts = (ScOptions){ SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
            .indent      = 0,
        });
        sc_list_add_str(pros, "Fast compilation",          none);
        sc_list_add_str(pros, "Small binary size",         none);
        sc_list_add_str(pros, "Excellent performance",     none);
        sc_list_add_str(pros, "Strong type system",        none);

        ScList *cons = sc_list_new((ScListOpts){
            .marker      = SC_LIST_BULLET,
            .bullet      = "\xe2\x80\x93",
            .marker_opts = (ScOptions){ SC_STYLE_BOLD, SC_COLOR_RED, SC_COLOR_NONE },
            .indent      = 0,
        });
        sc_list_add_str(cons, "Steep learning curve",      none);
        sc_list_add_str(cons, "Verbose error messages",    none);
        sc_list_add_str(cons, "Limited reflection",        none);

        ScText *th_pros = sc_text_new();
        sc_text_append(th_pros, "Pros", bold);
        ScText *th_cons = sc_text_new();
        sc_text_append(th_cons, "Cons", bold);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 4,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = SC_COLOR_NONE,
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_list(cl, pros, (ScColItem){ .min_w = 24 });
        sc_columns_add_list(cl, cons, (ScColItem){ .min_w = 24 });
        sc_columns_print(cl);

        sc_columns_free(cl);
        sc_list_free(pros);
        sc_list_free(cons);
        sc_text_free(th_pros);
        sc_text_free(th_cons);
    }
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Progress Bar ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void test_progressbar(void) {
    printf("\n\n══════════════════════  PROGRESSBAR  ══════════════════════\n\n");

    /* ── 1. Alle 4 Stile im Vergleich ── */
    {
        printf("--- 1. Alle 4 Stile (60%%) ---\n");
        ScProgressStyle styles[] = {
            SC_PROGRESS_BLOCK, SC_PROGRESS_ASCII,
            SC_PROGRESS_LINE,  SC_PROGRESS_SHADED,
        };
        const char *names[] = { "Block ", "ASCII ", "Line  ", "Shaded" };
        for (int i = 0; i < 4; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = styles[i],
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .bar_width    = 30,
                .label_width  = 6,
            });
            sc_progressbar_set_label(b, names[i]);
            sc_progressbar_finish(b, 0.6, 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 2. Block-Stil, verschiedene Füllstände ── */
    {
        printf("\n--- 2. Block-Stil bei 0%%, 25%%, 50%%, 75%%, 100%% ---\n");
        double vals[] = { 0.0, 0.25, 0.5, 0.75, 1.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = SC_PROGRESS_BLOCK,
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .bar_width    = 30,
            });
            sc_progressbar_finish(b, vals[i], 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 3. Schwellwert-Farben (grün → gelb → rot) ── */
    {
        printf("\n--- 3. Schwellwert-Farben (gruen->gelb->rot) bei 30%%, 60%%, 85%% ---\n");
        double vals[] = { 0.3, 0.6, 0.85 };
        for (int i = 0; i < 3; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style          = SC_PROGRESS_BLOCK,
                .left_cap       = "[",
                .right_cap      = "]",
                .show_percent   = 1,
                .bar_width      = 30,
                .use_thresholds = 1,
                .threshold_mid  = 0.5,
                .threshold_high = 0.75,
                .color_low      = SC_COLOR_GREEN,
                .color_mid      = SC_COLOR_YELLOW,
                .color_high     = SC_COLOR_RED,
            });
            sc_progressbar_finish(b, vals[i], 0.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 4. show_value: Wert/Max-Anzeige ── */
    {
        printf("\n--- 4. show_value (Wert/Max) mit max=250 ---\n");
        double vals[] = { 0.0, 52.0, 105.0, 189.0, 250.0 };
        for (int i = 0; i < 5; i++) {
            ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
                .style        = SC_PROGRESS_BLOCK,
                .left_cap     = "[",
                .right_cap    = "]",
                .show_percent = 1,
                .show_value   = 1,
                .bar_width    = 24,
                .label_width  = 11,
            });
            sc_progressbar_set_label(b, "Processing");
            sc_progressbar_finish(b, vals[i], 250.0);
            sc_progressbar_free(b);
        }
    }

    /* ── 5. Benutzerdefinierte Klammern: ❮❯ und kein Rahmen ── */
    {
        printf("\n--- 5. Benutzerdefinierte Klammern ---\n");
        /* ❮ = U+276E = \xe2\x9d\xae, ❯ = U+276F = \xe2\x9d\xaf */
        ScProgressBar *b1 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_SHADED,
            .left_cap     = "\xe2\x9d\xae",
            .right_cap    = "\xe2\x9d\xaf",
            .show_percent = 1,
            .bar_width    = 28,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b1, "Upload ");
        sc_progressbar_finish(b1, 0.72, 0.0);
        sc_progressbar_free(b1);

        ScProgressBar *b2 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_LINE,
            .left_cap     = NULL,
            .right_cap    = NULL,
            .show_percent = 1,
            .bar_width    = 30,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b2, "Build  ");
        sc_progressbar_finish(b2, 0.45, 0.0);
        sc_progressbar_free(b2);

        ScProgressBar *b3 = sc_progressbar_new((ScProgressBarOpts){
            .style        = SC_PROGRESS_ASCII,
            .left_cap     = "|",
            .right_cap    = "|",
            .show_percent = 1,
            .bar_width    = 28,
            .label_width  = 7,
        });
        sc_progressbar_set_label(b3, "Deploy ");
        sc_progressbar_finish(b3, 0.91, 0.0);
        sc_progressbar_free(b3);
    }
}

void test_progressbar_animated(void) {
    printf("\n--- Progressbar animated (~3s) ---\n");
    ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
        .style          = SC_PROGRESS_BLOCK,
        .left_cap       = "[",
        .right_cap      = "]",
        .show_percent   = 1,
        .show_value     = 1,
        .bar_width      = 40,
        .label_width    = 10,
        .use_thresholds = 1,
        .threshold_mid  = 0.33,
        .threshold_high = 0.66,
        .color_low      = SC_COLOR_RED,
        .color_mid      = SC_COLOR_YELLOW,
        .color_high     = SC_COLOR_GREEN,
    });
    sc_progressbar_set_label(b, "Installing");
    for (int v = 0; v <= 100; v += 2) {
        sc_progressbar_draw(b, (double)v, 100.0);
        usleep(60000);
    }
    sc_progressbar_finish(b, 100.0, 100.0);
    sc_progressbar_free(b);
}

/* ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ Spinner ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━ */

void test_spinner(void) {
    printf("\n\n═══════════════════════════  SPINNER  ═══════════════════════════\n\n");

    /* ── 1. Braille ── */
    {
        printf("--- 1. Braille ---\n");
        ScSpinner *s = sc_spinner_new("Compiling...", (ScSpinnerOpts){
            .style = SC_SPINNER_BRAILLE,
            .color = SC_COLOR_CYAN,
        });
        sc_spinner_tick(s);
        sc_spinner_tick(s);
        sc_spinner_finish(s, 1, "Build complete");
        sc_spinner_free(s);
    }

    /* ── 2. Pipe ── */
    {
        printf("--- 2. Pipe ---\n");
        ScSpinner *s = sc_spinner_new("Connecting...", (ScSpinnerOpts){
            .style = SC_SPINNER_PIPE,
            .color = SC_COLOR_YELLOW,
        });
        sc_spinner_tick(s);
        sc_spinner_tick(s);
        sc_spinner_finish(s, 0, "Connection refused");
        sc_spinner_free(s);
    }

    /* ── 3. Dots ── */
    {
        printf("--- 3. Dots ---\n");
        ScSpinner *s = sc_spinner_new("Analyzing...", (ScSpinnerOpts){
            .style = SC_SPINNER_DOTS,
            .color = SC_COLOR_MAGENTA,
        });
        sc_spinner_tick(s);
        sc_spinner_tick(s);
        sc_spinner_finish(s, 1, "Analysis complete");
        sc_spinner_free(s);
    }

    /* ── 4. Arrow ── */
    {
        printf("--- 4. Arrow ---\n");
        ScSpinner *s = sc_spinner_new("Deploying...", (ScSpinnerOpts){
            .style = SC_SPINNER_ARROW,
            .color = SC_COLOR_BLUE,
        });
        sc_spinner_tick(s);
        sc_spinner_tick(s);
        sc_spinner_finish(s, 1, "Deployed successfully");
        sc_spinner_free(s);
    }
}

void test_spinner_animated(void) {
    printf("\n--- Spinner animated (~2.5s) ---\n");
    ScSpinner *s = sc_spinner_new("Loading...", (ScSpinnerOpts){
        .style = SC_SPINNER_BRAILLE,
        .color = SC_COLOR_CYAN,
    });

    /* Phase 1: ~1s */
    for (int i = 0; i < 12; i++) { sc_spinner_tick(s); usleep(80000); }

    /* Phase 2: ~1s */
    sc_spinner_set_label(s, "Fetching data...");
    for (int i = 0; i < 12; i++) { sc_spinner_tick(s); usleep(80000); }

    /* Phase 3: ~0.5s */
    sc_spinner_set_label(s, "Finalizing...");
    for (int i = 0; i < 6; i++) { sc_spinner_tick(s); usleep(80000); }

    sc_spinner_finish(s, 1, "Done");
    sc_spinner_free(s);
}


void test_kv(void) {
    printf("\n");

    /* ── 1. Plain: default 2-space separator, auto key width ── */
    printf("--- KV 1. Plain ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){ 0 });
        sc_kv_add(kv, "Host",     "localhost");
        sc_kv_add(kv, "Port",     "8080");
        sc_kv_add(kv, "Database", "myapp_db");
        sc_kv_add(kv, "User",     "admin");
        sc_kv_add(kv, "Status",   "running");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 2. Custom separator, bold keys, green values ── */
    printf("--- KV 2. Styled with ': ' separator ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .sep      = ": ",
            .key_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .val_opts = { SC_STYLE_NONE, SC_COLOR_GREEN, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Version",  "2.4.1");
        sc_kv_add(kv, "License",  "MIT");
        sc_kv_add(kv, "Author",   "sparcli contributors");
        sc_kv_add(kv, "Language", "C11");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 3. Fixed key_width + word-wrap ── */
    printf("--- KV 3. Fixed key_width=18, word-wrap ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .key_width = 18,
            .sep       = ": ",
            .wrap_val  = 1,
            .key_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Name",        "sparcli");
        sc_kv_add(kv, "Description", "A C11 terminal UI library for styled output "
                                     "with colored text, panels, tables, lists, trees, "
                                     "progress bars and spinners.");
        sc_kv_add(kv, "Build",       "make && make test");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 4. Margin + item_gap ── */
    printf("--- KV 4. Margin=4, item_gap=1 ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .margin   = 4,
            .item_gap = 1,
            .sep      = "  \xe2\x86\x92  ", /* →  */
            .key_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Input",  "raw_data.csv");
        sc_kv_add(kv, "Output", "processed.json");
        sc_kv_add(kv, "Format", "JSON Lines");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }
}


void test_alert(void) {
    printf("\n");

    sc_alert_info   ("This is an informational message.");
    sc_alert_warning("Disk usage is above 85%. Consider cleaning up old files.");
    sc_alert_error  ("Failed to connect to database: connection timeout.");
    sc_alert_success("Deployment completed successfully in 3.2 seconds.");

    printf("\n");

    /* Multi-line content via embedded \n */
    sc_alert_success("Build passed: 147 tests, 0 failures\nCoverage: 94.2%");

    /* sc_alert_text with rich ScText */
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Loaded ",   (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append(t, "42",        (ScOptions){ SC_STYLE_BOLD, SC_COLOR_BLUE, SC_COLOR_NONE });
        sc_text_append(t, " modules",  (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_alert_text(SC_ALERT_INFO, t);
        sc_text_free(t);
    }
}


void test_badge(void) {
    printf("\n");

    /* ── 1. Default brackets ── */
    printf("--- Badge 1. Default brackets ---\n");
    sc_print_badge("INFO",  (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("WARN",  (ScBadgeOpts){ 0 });
    fputc(' ', stdout);
    sc_print_badge("ERROR", (ScBadgeOpts){ 0 });
    fputc('\n', stdout);

    printf("\n");

    /* ── 2. Custom caps ── */
    printf("--- Badge 2. Custom caps ---\n");
    sc_print_badge("v1.2.3", (ScBadgeOpts){
        .left_cap  = "\xe2\x9d\xae",  /* ❮ U+276E */
        .right_cap = "\xe2\x9d\xaf",  /* ❯ U+276F */
    });
    fputc(' ', stdout);
    sc_print_badge("main", (ScBadgeOpts){
        .left_cap  = "(",
        .right_cap = ")",
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 3. Colored badges with padding ── */
    printf("--- Badge 3. Colored + padded ---\n");
    sc_print_badge("BETA", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_BLACK, SC_COLOR_YELLOW },
    });
    fputc(' ', stdout);
    sc_print_badge("STABLE", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_GREEN },
    });
    fputc(' ', stdout);
    sc_print_badge("DEPRECATED", (ScBadgeOpts){
        .pad       = 1,
        .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_RED },
    });
    fputc('\n', stdout);

    printf("\n");

    /* ── 4. sc_text_append_badge inside ScText ── */
    printf("--- Badge 4. Inside ScText ---\n");
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Status: ",
                       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append_badge(t, "OK", (ScBadgeOpts){
            .pad       = 1,
            .text_opts = { SC_STYLE_BOLD, SC_COLOR_WHITE, SC_COLOR_GREEN },
        });
        sc_text_append(t, "  Release: ",
                       (ScOptions){ SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_text_append_badge(t, "v2.0.0", (ScBadgeOpts){
            .left_cap  = "(",
            .right_cap = ")",
            .text_opts = { SC_STYLE_NONE, SC_COLOR_CYAN, SC_COLOR_NONE },
        });
        sc_print_text(t);
        fputc('\n', stdout);
        sc_text_free(t);
    }
}


void test_util(void) {
    printf("\n");

    /* ── 1. sc_strip_ansi ── */
    printf("--- Util 1. sc_strip_ansi ---\n");
    {
        const char *colored = "\033[1;32mHello\033[0m, \033[31mworld\033[0m!";
        char       *plain   = sc_strip_ansi(colored);
        printf("Before: %s\n", colored);
        printf("After:  %s\n", plain);
        free(plain);
    }

    printf("\n");

    /* ── 2. sc_truncate ── */
    printf("--- Util 2. sc_truncate ---\n");
    {
        const char *long_str = "This is a very long string that needs truncation";
        char *t1 = sc_truncate(long_str, 20, "...");
        char *t2 = sc_truncate(long_str, 20, "\xe2\x80\xa6"); /* … U+2026 */
        char *t3 = sc_truncate("Short", 20, "...");           /* fits: no change */
        printf("Original (len=%d): %s\n", (int)strlen(long_str), long_str);
        printf("20 cols + '...':   %s\n", t1);
        printf("20 cols + '\xe2\x80\xa6':    %s\n", t2);
        printf("Short unchanged:   %s\n", t3);
        free(t1); free(t2); free(t3);
    }

    printf("\n");

    /* ── 3. sc_clear_line ── */
    printf("--- Util 3. sc_clear_line ---\n");
    {
        printf("This line will be cleared...");
        fflush(stdout);
        usleep(400000);
        sc_clear_line();
        printf("Line cleared and replaced!\n");
    }
}


void test_pad(void) {
    printf("\n");

    /* ── 1. sc_pad_str convenience: left indent ── */
    printf("--- Pad 1. sc_pad_str left=8 ---\n");
    sc_pad_str("Hello, world!", (ScPadOpts){ .left = 8 });
    sc_pad_str("Second line.",  (ScPadOpts){ .left = 8 });

    printf("\n");

    /* ── 2. Indent a table with top/bottom/left padding ── */
    printf("--- Pad 2. Table with top=1, bottom=1, left=4 ---\n");
    {
        ScTable *t = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x = 1,
        });
        sc_table_add_col(t, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Alice"), SC_CELL("9800") }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Bob"),   SC_CELL("7650") }, 2);

        ScRendered *r = sc_capture_table(t);
        sc_pad_print(r, (ScPadOpts){ .top = 1, .bottom = 1, .left = 4 });
        sc_rendered_free(r);
        sc_table_free(t);
    }

    /* ── 3. Indent a KV list with left padding ── */
    printf("--- Pad 3. KV list left=6 ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .sep      = ": ",
            .key_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Host", "localhost");
        sc_kv_add(kv, "Port", "5432");
        sc_kv_add(kv, "DB",   "myapp");

        ScRendered *r = sc_capture_kv(kv);
        sc_pad_print(r, (ScPadOpts){ .left = 6 });
        sc_rendered_free(r);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 4. Indent a panel (nested padding demo) ── */
    printf("--- Pad 4. Panel indented by left=4 ---\n");
    {
        ScRendered *r = sc_capture_panel_str(
            "This panel is indented\nwithout being inside Columns.",
            (ScPanelOpts){
                .border       = SC_BORDER_ROUNDED,
                .border_color = SC_COLOR_CYAN,
                .pad_x        = 1,
                .width        = 40,
            }
        );
        sc_pad_print(r, (ScPadOpts){ .left = 4 });
        sc_rendered_free(r);
    }
}


void test_align(void) {
    printf("\n");

    /* ── 1. Center a string ── */
    printf("--- Align 1. Center string ---\n");
    sc_align_str("──── Centered Title ────", SC_ALIGN_CENTER, 0);

    printf("\n");

    /* ── 2. Right-align a string ── */
    printf("--- Align 2. Right-align string ---\n");
    sc_align_str("Right-aligned text", SC_ALIGN_RIGHT, 0);
    sc_align_str("Short",              SC_ALIGN_RIGHT, 0);

    printf("\n");

    /* ── 3. Center-align a table ── */
    printf("--- Align 3. Center-aligned table ---\n");
    {
        ScTable *t = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_MAGENTA, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Summary ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_MAGENTA, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(t, "Item",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(t, "Total", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Passed"),  SC_CELL("147") }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Failed"),  SC_CELL("0")   }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Skipped"), SC_CELL("3")   }, 2);

        ScRendered *r = sc_capture_table(t);
        sc_align_print(r, SC_ALIGN_CENTER, 0);
        sc_rendered_free(r);
        sc_table_free(t);
    }

    printf("\n");

    /* ── 4. Center a fixed-width rule — shows reusability: same ScRendered
           printed with sc_pad_print (left-aligned, blank lines) and then
           with sc_align_print (centred) to compare both at once ── */
    printf("--- Align 4. Same capture used twice: padded then centered ---\n");
    {
        ScRendered *r = sc_capture_rule_str(
            "Section Header",
            (ScRuleOpts){
                .style       = SC_BORDER_SINGLE,
                .color       = SC_COLOR_YELLOW,
                .title_opts  = { SC_STYLE_BOLD, SC_COLOR_YELLOW, SC_COLOR_NONE },
                .title_align = SC_ALIGN_CENTER,
                .width       = 40,
            }
        );
        printf("  padded (left-aligned):\n");
        sc_pad_print(r, (ScPadOpts){ .left = 4 });
        printf("  aligned (centered):\n");
        sc_align_print(r, SC_ALIGN_CENTER, 0);
        sc_rendered_free(r);
    }

    printf("\n");

    /* ── 5. sc_columns_add_rendered: two tables side by side via capture ── */
    printf("--- Align 5. columns_add_rendered: pad left + align center ---\n");
    {
        /* Build two small tables */
        ScTable *ta = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_CYAN, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Left (padded) ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(ta, "A", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(ta, "B", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(ta, (ScCell[]){ SC_CELL("Alpha"), SC_CELL("10") }, 2);
        sc_table_add_row(ta, (ScCell[]){ SC_CELL("Beta"),  SC_CELL("20") }, 2);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_GREEN, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title = " Right (captured) ", .title_opts = { SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE },
            .title_pos = SC_TITLE_TOP, .title_align = SC_ALIGN_CENTER, .title_pad = 1,
            .cell_pad_x = 1,
        });
        sc_table_add_col(tb, "X", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_col(tb, "Y", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Gamma"), SC_CELL("30") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Delta"), SC_CELL("40") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Eps"),   SC_CELL("50") }, 2);

        /* Capture: left table gets extra left indent, right is plain */
        ScRendered *ra = sc_capture_table(ta);
        ScRendered *rb = sc_capture_table(tb);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 4,
            .sep_style = SC_BORDER_NONE,
            .sep_color = SC_COLOR_NONE,
        });
        sc_columns_add_rendered(cl, ra, (ScColItem){ 0 });
        sc_columns_add_rendered(cl, rb, (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);

        sc_rendered_free(ra);
        sc_rendered_free(rb);
        sc_table_free(ta);
        sc_table_free(tb);
    }
}

/* ── test_markup ─────────────────────────────────────────────────────────── */

void test_markup(void) {
    printf("\n");

    /* ── 1. Basic styles ── */
    printf("--- Markup 1. basic styles ---\n");
    sc_markup_println("[bold]bold[/]  [italic]italic[/]  [dim]dim[/]  [underline]underline[/]  [u]u-alias[/]  [bold italic underline]all three[/]");

    printf("\n");

    /* ── 2. Named foreground colors ── */
    printf("--- Markup 2. named fg colors ---\n");
    sc_markup_println("[red]red[/]  [green]green[/]  [yellow]yellow[/]  [blue]blue[/]  [magenta]magenta[/]  [cyan]cyan[/]  [white]white[/]  [black]black[/]");

    printf("\n");

    /* ── 3. Background colors ── */
    printf("--- Markup 3. background colors ---\n");
    sc_markup_println("[on blue]on blue[/]  [white on red]white on red[/]  [bold yellow on blue]bold yellow on blue[/]");

    printf("\n");

    /* ── 4. RGB colors ── */
    printf("--- Markup 4. rgb colors ---\n");
    sc_markup_println("[rgb(255,165,0)]orange warning[/]  [rgb(128,0,128)]purple[/]  [white on rgb(30,30,30)]dark bg[/]");

    printf("\n");

    /* ── 5. Nesting / stack ── */
    printf("--- Markup 5. nesting / stack ---\n");
    sc_markup_println("[bold][red]bold+red[/] still bold[/] plain");

    printf("\n");

    /* ── 6. Unknown tags: verbatim (default) vs. stripped ── */
    printf("--- Markup 6a. unknown tags → verbatim (default) ---\n");
    sc_markup_println("[blink]text with unknown tag[/blink]");
    sc_markup_println("[bold]known [blink]unknown-mid[/blink] still known[/]");

    printf("\n");

    printf("--- Markup 6b. unknown tags → stripped (strip_unknown=1) ---\n");
    sc_markup_println_opts("[blink]text with unknown tag[/blink]",
                           (ScMarkupOpts){ .strip_unknown = 1 });
    sc_markup_println_opts("[bold]known [blink]unknown-mid[/blink] still known[/]",
                           (ScMarkupOpts){ .strip_unknown = 1 });

    printf("\n");

    printf("--- Markup 6c. sc_markup_parse_opts side-by-side ---\n");
    {
        const char *markup = "prefix [blink]unknown[/blink] [bold]bold[/] suffix";
        printf("  verbatim: ");
        ScText *a = sc_markup_parse(markup);
        sc_print_text(a); printf("\n");
        sc_text_free(a);
        printf("  stripped: ");
        ScText *b = sc_markup_parse_opts(markup, (ScMarkupOpts){ .strip_unknown = 1 });
        sc_print_text(b); printf("\n");
        sc_text_free(b);
    }

    printf("\n");

    /* ── 7. Escape [[ → literal [ ── */
    printf("--- Markup 7. [[ escape ---\n");
    sc_markup_println("[[bold]] is a literal bracket pair");
    sc_markup_println("[bold]styled [[with brackets]][/]");

    printf("\n");

    /* ── 8. sc_markup_print / sc_markup_println ── */
    printf("--- Markup 8. sc_markup_print + println ---\n");
    sc_markup_print("[green]print (no newline)[/green] ");
    sc_markup_print("[yellow]still same line[/]");
    printf("\n");
    sc_markup_println("[cyan]println (adds newline)[/]");

    printf("\n");

    /* ── 9. sc_markup_append into existing ScText ── */
    printf("--- Markup 9. sc_markup_append ---\n");
    {
        ScText *t = sc_text_new();
        sc_text_append(t, "Plain prefix — ", (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE });
        sc_markup_append(t, "[bold red]appended markup[/] — ");
        sc_markup_append(t, "[italic cyan]second append[/]");
        sc_print_text(t);
        printf("\n");
        sc_text_free(t);
    }

    printf("\n");

    /* ── 10. SC_CELL_M in a table ── */
    printf("--- Markup 10. SC_CELL_M in table ---\n");
    {
        ScTable *t = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_COLOR_NONE, SC_COLOR_NONE,
                            SC_COLOR_NONE, SC_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .cell_pad_x = 1,
        });
        sc_table_add_col(t, "Status",  (ScColOpts){ 0, 0, 14, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE });
        sc_table_add_col(t, "Message", (ScColOpts){ 0, 0, 28, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_COLOR_NONE });
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[green]✔ OK[/]"),      SC_CELL_M("Build [bold]passed[/]")          }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[yellow]⚠ Warn[/]"),   SC_CELL_M("[italic]Deprecation detected[/]") }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL_M("[red]✖ Error[/]"),     SC_CELL_M("[bold red]Compilation failed[/]") }, 2);
        sc_table_print(t);
        sc_table_free(t);
    }

    printf("\n");

    /* ── 11. sc_markup_parse + sc_panel_text ── */
    printf("--- Markup 11. sc_markup_parse + sc_panel_text ---\n");
    {
        ScText *content = sc_markup_parse(
            "[bold]sparcli[/] supports [italic]Rich-compatible[/] markup.\n"
            "Colors: [red]red[/], [green]green[/], [blue]blue[/].\n"
            "Combine: [bold yellow on blue] bold yellow on blue [/]"
        );
        sc_panel_text(content, (ScPanelOpts){
            .border       = SC_BORDER_ROUNDED,
            .border_color = SC_COLOR_CYAN,
            .title        = " Markup Demo ",
            .title_opts   = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
            .title_pos    = SC_TITLE_TOP,
            .title_align  = SC_ALIGN_CENTER,
            .title_pad    = 1,
            .pad_x        = 2,
            .pad_y        = 1,
        });
        sc_text_free(content);
    }
}
