#include "sparcli.h"
#include <stdio.h>


void test_columns(void) {
    printf("\n");

    /* ── 1. Two tables side by side ── */
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

    /* ── 2. Panel + table with separator ── */
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

    /* ── 3. valign: three panels of different heights ── */
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

    /* ── 4. total_width distributed across three flex columns ── */
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

    /* ── 5. Nested columns ── */
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

    /* ── 7. Separator: double border, colored ── */
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
            .sep_color = sc_color_from_rgb(80, 80, 140),
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
