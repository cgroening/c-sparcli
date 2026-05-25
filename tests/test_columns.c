#include "sparcli.h"
#include <stdio.h>


void test_columns(void) {
    printf("\n");

    /* ── 1. Two tables side by side ── */
    {
        ScTableOpts t1_opts = {
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Team A ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        };
        ScTableData *table_data_1 = sc_table_new();
        sc_table_add_column(table_data_1, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_1, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Alice"), sc_cell("9800") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Bob"),   sc_cell("7650") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Carol"), sc_cell("6100") }, 2);

        ScTableOpts t2_opts = {
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Team B ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        };
        ScTableData *table_data_2 = sc_table_new();
        sc_table_add_column(table_data_2, "Name",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_2, "Score", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Dave"),  sc_cell("8200") }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Eve"),   sc_cell("7100") }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Frank"), sc_cell("5400") }, 2);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){ .gap = 3 });
        sc_columns_add_table(cl, table_data_1, t1_opts, (ScColItem){0});
        sc_columns_add_table(cl, table_data_2, t2_opts, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(table_data_1);
        sc_table_free(table_data_2);
    }

    printf("\n");

    /* ── 2. Panel + table with separator ── */
    {
        ScTableData *table_data = sc_table_new();
        sc_table_add_column(table_data, "Port",    (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data, "Service", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data, "State",   (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("22"),   sc_cell("ssh"),   sc_cell("open")   }, 3);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("80"),   sc_cell("http"),  sc_cell("open")   }, 3);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("443"),  sc_cell("https"), sc_cell("open")   }, 3);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("3306"), sc_cell("mysql"), sc_cell("closed") }, 3);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(cl,
            "Host: 192.168.1.1\nOS: Linux 6.8\nUptime: 42 days\nCPU: 4 cores\nRAM: 16 GB",
            (ScPanelOpts){
                .border  = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_GREEN },
                .title = {.text = " System Info ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE },
                          .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
                .padding = {0, 2, 0, 2},
            },
            (ScColItem){0});
        sc_columns_add_table(cl, table_data, (ScTableOpts){
            .borders    = { SC_BORDER_ROUNDED, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        }, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(table_data);
    }

    printf("\n");

    /* ── 3. valign: three panels of different heights ── */
    {
        ScPanelOpts po_top = {
            .border  = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .title = {.text = " Top ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .padding = {0, 1, 0, 1},
        };
        ScPanelOpts po_mid = po_top;
        po_mid.title.text = " Middle ";
        ScPanelOpts po_bot = po_top;
        po_bot.title.text = " Bottom ";

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
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
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
        ScTableData *table_data = sc_table_new();
        sc_table_add_column(table_data, "Key",   (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data, "Value", (ScColOpts){0,0,0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("env"),     sc_cell("production") }, 2);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("region"),  sc_cell("eu-west-1")  }, 2);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("version"), sc_cell("2.4.1")      }, 2);

        ScColumns *inner = sc_columns_new((ScColumnsOpts){ .gap = 2, .valign = SC_VALIGN_TOP });
        sc_columns_add_str(inner, "CPU:  34 %\nMEM:  61 %\nDISK: 48 %", (ScColItem){.fixed_w=14});
        sc_columns_add_str(inner, "NET-IN:  1.2 MB/s\nNET-OUT: 0.4 MB/s\nPKTS:    12 403",
                           (ScColItem){.fixed_w=18});

        ScColumns *outer = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_table(outer, table_data, (ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .cell_pad = {0, 1, 0, 1},
        }, (ScColItem){0});
        sc_columns_add_columns(outer, inner, (ScColItem){0});
        sc_columns_print(outer);

        sc_columns_free(outer);
        sc_columns_free(inner);
        sc_table_free(table_data);
    }

    printf("\n");

    /* ── 6. min_w / max_w / fixed_w + item.align ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .sep = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE },
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
        ScTableOpts t1_opts = {
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Revenue ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        };
        ScTableData *table_data_1 = sc_table_new();
        sc_table_add_column(table_data_1, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_1, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Q1"), sc_cell("1 240 000") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Q2"), sc_cell("1 580 000") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Q3"), sc_cell("1 390 000") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Q4"), sc_cell("1 820 000") }, 2);

        ScTableOpts t2_opts = {
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Costs ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        };
        ScTableData *table_data_2 = sc_table_new();
        sc_table_add_column(table_data_2, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_2, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Q1"), sc_cell("980 000")   }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Q2"), sc_cell("1 100 000") }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Q3"), sc_cell("870 000")   }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Q4"), sc_cell("1 250 000") }, 2);

        ScTableOpts t3_opts = {
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1, .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Profit ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        };
        ScTableData *table_data_3 = sc_table_new();
        sc_table_add_column(table_data_3, "Quarter", (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_3, "EUR",     (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_3, (ScCell[]){ sc_cell("Q1"), sc_cell("260 000") }, 2);
        sc_table_add_row(table_data_3, (ScCell[]){ sc_cell("Q2"), sc_cell("480 000") }, 2);
        sc_table_add_row(table_data_3, (ScCell[]){ sc_cell("Q3"), sc_cell("520 000") }, 2);
        sc_table_add_row(table_data_3, (ScCell[]){ sc_cell("Q4"), sc_cell("570 000") }, 2);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep = { .type = SC_BORDER_DOUBLE, .color = sc_ansi_color_from_rgb(80, 80, 140) },
        });
        sc_columns_add_table(cl, table_data_1, t1_opts, (ScColItem){0});
        sc_columns_add_table(cl, table_data_2, t2_opts, (ScColItem){0});
        sc_columns_add_table(cl, table_data_3, t3_opts, (ScColItem){0});
        sc_columns_print(cl);
        sc_columns_free(cl);
        sc_table_free(table_data_1);
        sc_table_free(table_data_2);
        sc_table_free(table_data_3);
    }

    printf("\n");


}
