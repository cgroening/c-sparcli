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
        sc_table_add_column(tb, "Name", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Age",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "City", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Alice"), sc_cell("30"), sc_cell("Berlin")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Bob"),   sc_cell("24"), sc_cell("Hamburg") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Carol"), sc_cell("35"), sc_cell("München") }, 3);
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
        sc_table_add_column(tb, "Product",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q1",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q2",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q3",       (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Apples"),   sc_cell("120"), sc_cell("95"),  sc_cell("140") }, 4);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Bananas"),  sc_cell("80"),  sc_cell("110"), sc_cell("70")  }, 4);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Cherries"), sc_cell("45"),  sc_cell("60"),  sc_cell("90")  }, 4);
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
        sc_table_add_column(tb, "Lang",   (ScColOpts){0,0,12, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Year",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Typed",  (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("C"),      sc_cell("1972"), sc_cell("Static")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Python"), sc_cell("1991"), sc_cell("Dynamic") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Rust"),   sc_cell("2010"), sc_cell("Static")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Lua"),    sc_cell("1993"), sc_cell("Dynamic") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Zig"),    sc_cell("2016"), sc_cell("Static")  }, 3);
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
        sc_table_add_column(tb, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Status",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Uptime",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell_t(lbl), sc_cell_t(ok),  sc_cell("99.9%")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("database"), sc_cell_t(err), sc_cell("72.1%")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("cache"),    sc_cell_t(ok),  sc_cell("100.0%") }, 3);
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
        sc_table_add_column(tb, "Left",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Center", (ScColOpts){0,0,14, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Right",  (ScColOpts){0,0,14, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("abc"),         sc_cell("abc"),         sc_cell("abc")         }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("longer text"), sc_cell("longer text"), sc_cell("longer text") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("x"),           sc_cell("x"),           sc_cell("x")           }, 3);
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
        sc_table_add_column(tb, "Tall Cell", (ScColOpts){0,0,0,  SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Top",       (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Middle",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Bottom",    (ScColOpts){0,0,10, SC_ALIGN_LEFT, SC_VALIGN_BOTTOM, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell_t(tall), sc_cell("▲ top"), sc_cell("● mid"), sc_cell("▼ bot"),
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
        sc_table_add_column(tb, "Fixed=16", (ScColOpts){0,  0,  16, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Min=10",   (ScColOpts){10, 0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Max=8",    (ScColOpts){0,  8,  0,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Auto",     (ScColOpts){0,  0,  0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("short"),       sc_cell("hi"),  sc_cell("truncated?"), sc_cell("auto") }, 4);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("a bit longer"), sc_cell("pad"), sc_cell("clipped"),   sc_cell("fits") }, 4);
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
        sc_table_add_column(tb, "Animal", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Sound",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Legs",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Dog"),   sc_cell("Woof"),  sc_cell("4") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Bird"),  sc_cell("Tweet"), sc_cell("2") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Snake"), sc_cell("Hiss"),  sc_cell("0") }, 3);
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
        sc_table_add_column(tb, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("host"),    sc_cell("localhost") }, 2);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("port"),    sc_cell("8080")      }, 2);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("timeout"), sc_cell("30s")       }, 2);
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
        sc_table_add_column(tb, "OS",     (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Kernel", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Shell",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Linux"),   sc_cell("6.8"),   sc_cell("bash") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("macOS"),   sc_cell("XNU"),   sc_cell("zsh")  }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Windows"), sc_cell("NT 10"), sc_cell("pwsh") }, 3);
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
            .title = {.text = " Word Wrap ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Component",   (ScColOpts){0,0,14, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Description", (ScColOpts){0,0,24, SC_ALIGN_LEFT,   SC_VALIGN_TOP,    1, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Status",      (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Auth Service"),
            sc_cell("Handles OAuth2 token validation and refresh for all API clients"),
            sc_cell("OK"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Data Pipeline"),
            sc_cell("Ingests streaming events from Kafka and writes to the data warehouse"),
            sc_cell("WARN"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Scheduler"),
            sc_cell("Runs periodic jobs using a distributed cron-like mechanism"),
            sc_cell("OK"),
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
            .title = {.text = " Footer Row (Totals) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Item",  (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Qty",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Price", (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Total", (ScColOpts){0,0,10, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Widget A"),   sc_cell("3"), sc_cell("$ 12.00"), sc_cell("$  36.00"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Gadget B"),   sc_cell("1"), sc_cell("$ 49.99"), sc_cell("$  49.99"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Doohickey"),  sc_cell("5"), sc_cell("$  4.50"), sc_cell("$  22.50"),
        }, 4);
        sc_table_add_footer_row(tb, (ScCell[]){
            sc_cell("TOTAL"), sc_cell("9"), sc_cell(""), sc_cell("$ 108.49"),
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
            .title = {.text = " Per-Row Background ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Build",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Branch", (ScColOpts){0,0,0, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Result", (ScColOpts){0,0,0, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Time",   (ScColOpts){0,0,0, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("#101"), sc_cell("main"),      sc_cell("PASS"), sc_cell("1m 23s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            sc_cell("#102"), sc_cell("feature/x"), sc_cell("FAIL"), sc_cell("0m 47s"),
        }, 4, sc_ansi_color_from_rgb(60, 20, 20));
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("#103"), sc_cell("main"),      sc_cell("PASS"), sc_cell("1m 31s"),
        }, 4);
        sc_table_add_row_bg(tb, (ScCell[]){
            sc_cell("#104"), sc_cell("hotfix/y"),  sc_cell("PASS"), sc_cell("2m 05s"),
        }, 4, sc_ansi_color_from_rgb(20, 50, 20));
        sc_table_add_row_bg(tb, (ScCell[]){
            sc_cell("#105"), sc_cell("release/2"), sc_cell("FAIL"), sc_cell("3m 12s"),
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
            .title = {.text = " Per-Column Background ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Feature",    (ScColOpts){0,0,18, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Free",       (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Pro",        (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_ansi_color_from_rgb(20, 45, 90)});
        sc_table_add_column(tb, "Enterprise", (ScColOpts){0,0,12, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, sc_ansi_color_from_rgb(60, 40, 0)});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Storage"),       sc_cell("5 GB"),      sc_cell("50 GB"),      sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("API calls/mo"),  sc_cell("1 000"),     sc_cell("50 000"),     sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Team members"),  sc_cell("1"),         sc_cell("25"),         sc_cell("Unlimited"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Support"),       sc_cell("—"),         sc_cell("Email"),      sc_cell("24/7"),
        }, 4);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Custom domain"), sc_cell("✗"),         sc_cell("✓"),          sc_cell("✓"),
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
            .title = {.text = " Total Width = 60 cols ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Country",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Capital",    (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Population", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Germany"),     sc_cell("Berlin"),    sc_cell("83M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("France"),      sc_cell("Paris"),     sc_cell("68M") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Netherlands"), sc_cell("Amsterdam"), sc_cell("18M") }, 3);
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
            .title = {.text = " Colspan ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Name", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q1",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q2",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q3",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Q4",   (ScColOpts){0,0,6,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell(""),
            sc_cell_cs("H1 (Q1+Q2)", 2), sc_cell_skip(),
            sc_cell_cs("H2 (Q3+Q4)", 2), sc_cell_skip(),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Alice"),
            sc_cell("120"), sc_cell("95"), sc_cell("140"), sc_cell("110"),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Bob"),
            sc_cell("80"), sc_cell("110"), sc_cell("70"), sc_cell("95"),
        }, 5);
        sc_table_add_row(tb, (ScCell[]){
           sc_cell_csa("* values in units sold", 5, SC_ALIGN_CENTER),
            sc_cell_skip(), sc_cell_skip(), sc_cell_skip(), sc_cell_skip(),
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
            .title = {.text = " Rowspan ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Category", (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_MIDDLE, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Item",     (ScColOpts){0,0,0,  SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Price",    (ScColOpts){0,0,9,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell_rs("Fruits", 3), sc_cell("Apple"),  sc_cell("$  0.50"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_row_skip(),             sc_cell("Banana"), sc_cell("$  0.30"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_row_skip(),             sc_cell("Cherry"), sc_cell("$  2.00"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell_rs("Veggies", 2), sc_cell("Carrot"), sc_cell("$  0.80"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_row_skip(),              sc_cell("Pepper"), sc_cell("$  1.20"),
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
            .title = {.text = " RTL Column Order ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "Col A", (ScColOpts){0,0,10, SC_ALIGN_LEFT,   SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Col B", (ScColOpts){0,0,10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Col C", (ScColOpts){0,0,10, SC_ALIGN_RIGHT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("Alpha"), sc_cell("Beta"), sc_cell("Gamma"),
        }, 3);
        sc_table_add_row(tb, (ScCell[]){
            sc_cell("1"), sc_cell("2"), sc_cell("3"),
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
            .title = {.text = " Max Rows = 4 (10 total) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP }, .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "#",     (ScColOpts){0,0,4,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Name",  (ScColOpts){0,0,12, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Score", (ScColOpts){0,0,7,  SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        /* Strings must outlive sc_table_print; use per-row arrays rather than loop-local buffers. */
        char row_idx[10][4], row_name[10][12], row_score[10][8];
        for (int i = 1; i <= 10; i++) {
            snprintf(row_idx[i-1],   sizeof(row_idx[i-1]),   "%d", i);
            snprintf(row_name[i-1],  sizeof(row_name[i-1]),  "Player %d", i);
            snprintf(row_score[i-1], sizeof(row_score[i-1]), "%d", 10000 - i * 850);
            sc_table_add_row(tb, (ScCell[]){
                sc_cell(row_idx[i-1]), sc_cell(row_name[i-1]), sc_cell(row_score[i-1]),
            }, 3);
        }
        sc_table_print(tb);
        sc_table_free(tb);
    }
}
