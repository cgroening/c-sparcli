#include "sparcli.h"
#include <stdio.h>


void test_tables(void) {
    printf("\n");

    /* ── 1. Single border, header row ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Name", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Age",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "City", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_DOUBLE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_YELLOW, 0, 0, 0 },
            .header.row  = 1, .header.col   = 1,
            .header.row_bg = SC_ANSI_COLOR_NONE, .header.col_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Product",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q1",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q2",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q3",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_ROUNDED, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .striped     = 1, .stripe_bg = sc_ansi_color_from_rgb(40, 40, 60),
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Lang",   (ScColOpts){0,0,12, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Year",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Typed",  (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
        ScTextStyle plain = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle bold  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle red   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,   SC_ANSI_COLOR_NONE };
        ScTextStyle green = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE };

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
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Status",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Uptime",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 2, 0, 2},
        });
        sc_table_add_col(tb, "Left",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Center", (ScColOpts){0,0,14, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Right",  (ScColOpts){0,0,14, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("abc"),         SC_CELL("abc"),         SC_CELL("abc")         }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("longer text"), SC_CELL("longer text"), SC_CELL("longer text") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("x"),           SC_CELL("x"),           SC_CELL("x")           }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 6. Vertical alignment: top / middle / bottom ── */
    {
        ScTextStyle p = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        ScText *tall = sc_text_new();
        sc_text_append(tall, "Line 1\nLine 2\nLine 3\nLine 4", p);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {1, 1, 1, 1},
        });
        sc_table_add_col(tb, "Tall Cell", (ScColOpts){0,0,0,  SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Top",       (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Middle",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Bottom",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_BOTTOM, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Fixed=16", (ScColOpts){0,  0,  16, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Min=10",   (ScColOpts){10, 0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Max=8",    (ScColOpts){0,  8,  0,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Auto",     (ScColOpts){0,  0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("short"),       SC_CELL("hi"),  SC_CELL("truncated?"), SC_CELL("auto") }, 4);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("a bit longer"), SC_CELL("pad"), SC_CELL("clipped"),   SC_CELL("fits") }, 4);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 8. No border ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Animal", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Sound",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Legs",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_ASCII, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
            .borders     = { SC_BORDER_THICK, sc_ansi_color_from_rgb(255,180,0), SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, sc_ansi_color_from_rgb(255,180,0), SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "OS",     (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Kernel", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Shell",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Linux"),   SC_CELL("6.8"),   SC_CELL("bash") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("macOS"),   SC_CELL("XNU"),   SC_CELL("zsh")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Windows"), SC_CELL("NT 10"), SC_CELL("pwsh") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 11. Double border, title top, cyan ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_DOUBLE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Word Wrap ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Component",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Description", (ScColOpts){0,0,24, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    1, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Status",      (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
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

    /* ── 18. Footer row ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .footer.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .footer.row_bg = sc_ansi_color_from_rgb(30, 40, 30),
            .title = {.text = " Footer Row (Totals) ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Item",  (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Qty",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Price", (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Total", (ScColOpts){0,0,10, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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

    /* ── 19. Per-row background ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Per-Row Background ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Build",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Branch", (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Result", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Time",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("#101"), SC_CELL("main"),      SC_CELL("PASS"), SC_CELL("1m 23s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#102"), SC_CELL("feature/x"), SC_CELL("FAIL"), SC_CELL("0m 47s"),
        }, 4, sc_ansi_color_from_rgb(60, 20, 20));
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL("#103"), SC_CELL("main"),      SC_CELL("PASS"), SC_CELL("1m 31s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#104"), SC_CELL("hotfix/y"),  SC_CELL("PASS"), SC_CELL("2m 05s"),
        }, 4, sc_ansi_color_from_rgb(20, 50, 20));
        sc_table_add_row_bg(tb, (ScCell[]){
            SC_CELL("#105"), SC_CELL("release/2"), SC_CELL("FAIL"), SC_CELL("3m 12s"),
        }, 4, sc_ansi_color_from_rgb(60, 20, 20));
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 20. Per-column background ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = sc_ansi_color_from_rgb(30, 30, 50),
            .header.opts   = { SC_TEXT_ATTR_BOLD, sc_ansi_color_from_rgb(200, 200, 255), SC_ANSI_COLOR_NONE },
            .title = {.text = " Per-Column Background ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Feature",    (ScColOpts){0,0,18, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Free",       (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Pro",        (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_ansi_color_from_rgb(20, 45, 90)});
        sc_table_add_col(tb, "Enterprise", (ScColOpts){0,0,12, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_ansi_color_from_rgb(60, 40, 0)});
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

    /* ── 21. Total width ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .total_width = 60,
            .title = {.text = " Total Width = 60 cols ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Country",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Capital",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Population", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Germany"),     SC_CELL("Berlin"),    SC_CELL("83M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("France"),      SC_CELL("Paris"),     SC_CELL("68M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Netherlands"), SC_CELL("Amsterdam"), SC_CELL("18M") }, 3);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 22. Colspan ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Colspan ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Name", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q1",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q2",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q3",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Q4",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
        sc_table_add_row(tb, (ScCell[]){
           SC_CELL_CSA("* values in units sold", 5, SC_ALIGN_CENTER),
            SC_CELL_SKIP, SC_CELL_SKIP, SC_CELL_SKIP, SC_CELL_SKIP,
        }, 5);
        sc_table_print(tb);
        sc_table_free(tb);
    }

    printf("\n");

    /* ── 23. Rowspan ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Rowspan ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Category", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Item",     (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Price",    (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            SC_CELL_RS("Fruits", 3), SC_CELL("Apple"),  SC_CELL("$  0.50"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_ROW_SKIP,             SC_CELL("Banana"), SC_CELL("$  0.30"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            SC_ROW_SKIP,             SC_CELL("Cherry"), SC_CELL("$  2.00"),
        }, 3);
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

    /* ── 24. RTL column order ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .rtl         = 1,
            .title = {.text = " RTL Column Order ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "Col A", (ScColOpts){0,0,10, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Col B", (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Col C", (ScColOpts){0,0,10, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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

    /* ── 25. Max rows ── */
    {
        ScTable *tb = sc_table_new((ScTableOpts){
            .borders       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                               SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row    = 1, .header.row_bg = SC_ANSI_COLOR_NONE,
            .header.opts   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .max_rows    = 4,
            .title = {.text = " Max Rows = 4 (10 total) ", .opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_TITLE_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "#",     (ScColOpts){0,0,4,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Name",  (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Score", (ScColOpts){0,0,7,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
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
