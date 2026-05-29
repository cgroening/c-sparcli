#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_cyan = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_magenta = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_yellow = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_green = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
};

static const ScColOpts col_left = {
    0, 0, 0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
};
static const ScColOpts col_right = {
    0, 0, 0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
};


/** Returns the common table border used by every demo table. */
static ScTableBorder default_table_border(void) {
    return (ScTableBorder){
        SC_BORDER_SINGLE,
        SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        0, 0, 0,
    };
}

/** Returns a centered top title with the given text and style. */
static ScTitle title_centered_top(const char *text, ScTextStyle style) {
    return (ScTitle){
        .text = text,
        .style = style,
        .halign = SC_ALIGN_CENTER,
        .pad = 1,
        .pos = SC_POSITION_TOP,
    };
}


void test_columns(void) {
    printf("\n");

    /* ── 1. Two tables side by side ── */
    {
        ScTableData *team_a = sc_table_new();
        sc_table_add_column(team_a, "Name", col_left);
        sc_table_add_column(team_a, "Score", col_right);
        sc_table_add_row(team_a, (ScCell[]){
            sc_cell("Alice"), sc_cell("9800")
        }, 2);
        sc_table_add_row(team_a, (ScCell[]){
            sc_cell("Bob"), sc_cell("7650")
        }, 2);
        sc_table_add_row(team_a, (ScCell[]){
            sc_cell("Carol"), sc_cell("6100")
        }, 2);

        ScTableData *team_b = sc_table_new();
        sc_table_add_column(team_b, "Name", col_left);
        sc_table_add_column(team_b, "Score", col_right);
        sc_table_add_row(team_b, (ScCell[]){
            sc_cell("Dave"), sc_cell("8200")
        }, 2);
        sc_table_add_row(team_b, (ScCell[]){
            sc_cell("Eve"), sc_cell("7100")
        }, 2);
        sc_table_add_row(team_b, (ScCell[]){
            sc_cell("Frank"), sc_cell("5400")
        }, 2);

        ScTableOpts team_a_opts = {
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Team A ", bold_cyan),
            .cell_pad = { 0, 1, 0, 1 },
        };
        ScTableOpts team_b_opts = {
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Team B ", bold_magenta),
            .cell_pad = { 0, 1, 0, 1 },
        };

        ScColumns *columns = sc_columns_new((ScColumnsOpts){ .gap = 3 });
        sc_columns_add_table(columns, team_a, team_a_opts, (ScColItem){ 0 });
        sc_columns_add_table(columns, team_b, team_b_opts, (ScColItem){ 0 });
        sc_columns_print(columns);
        sc_columns_free(columns);
        sc_table_free(team_a);
        sc_table_free(team_b);
    }

    printf("\n");

    /* ── 2. Panel + table with separator ── */
    {
        ScTableData *ports = sc_table_new();
        sc_table_add_column(ports, "Port", col_right);
        sc_table_add_column(ports, "Service", col_left);
        sc_table_add_column(ports, "State", col_left);
        sc_table_add_row(ports, (ScCell[]){
            sc_cell("22"), sc_cell("ssh"), sc_cell("open")
        }, 3);
        sc_table_add_row(ports, (ScCell[]){
            sc_cell("80"), sc_cell("http"), sc_cell("open")
        }, 3);
        sc_table_add_row(ports, (ScCell[]){
            sc_cell("443"), sc_cell("https"), sc_cell("open")
        }, 3);
        sc_table_add_row(ports, (ScCell[]){
            sc_cell("3306"), sc_cell("mysql"), sc_cell("closed")
        }, 3);

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 2,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(columns,
            "Host: 192.168.1.1\nOS: Linux 6.8\nUptime: 42 days\n"
            "CPU: 4 cores\nRAM: 16 GB",
            (ScPanelOpts){
                .border = {
                    .type = SC_BORDER_ROUNDED,
                    .color = SC_ANSI_COLOR_GREEN,
                },
                .title = title_centered_top(" System Info ", bold_green),
                .padding = { 0, 2, 0, 2 },
            },
            (ScColItem){ 0 }
        );
        sc_columns_add_table(columns, ports, (ScTableOpts){
            .border = {
                SC_BORDER_ROUNDED,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0
            },
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        }, (ScColItem){ 0 });
        sc_columns_print(columns);
        sc_columns_free(columns);
        sc_table_free(ports);
    }

    printf("\n");

    /* ── 3. valign: three panels of different heights ── */
    {
        ScPanelOpts top_panel = {
            .border = {
                .type = SC_BORDER_SINGLE,
                .color = SC_ANSI_COLOR_NONE,
            },
            .title = title_centered_top(" Top ", bold),
            .padding = { 0, 1, 0, 1 },
        };
        ScPanelOpts middle_panel = top_panel;
        middle_panel.title.text = " Middle ";
        ScPanelOpts bottom_panel = top_panel;
        bottom_panel.title.text = " Bottom ";

        const char *long_lines = "Line A\nLine B\nLine C\nLine D\nLine E";
        const char *medium_lines = "Line A\nLine B\nLine C";
        const char *short_line = "Line A";

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 4,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(
            columns, long_lines, top_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, medium_lines, middle_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, short_line, bottom_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);

        printf("\n");

        columns = sc_columns_new((ScColumnsOpts){
            .gap = 4,
            .valign = SC_VALIGN_MIDDLE,
        });
        sc_columns_add_panel_str(
            columns, long_lines, top_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, medium_lines, middle_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, short_line, bottom_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);

        printf("\n");

        columns = sc_columns_new((ScColumnsOpts){
            .gap = 4,
            .valign = SC_VALIGN_BOTTOM,
        });
        sc_columns_add_panel_str(
            columns, long_lines, top_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, medium_lines, middle_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_add_panel_str(
            columns, short_line, bottom_panel, (ScColItem){ .fixed_w = 18 }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }

    printf("\n");

    /* ── 4. total_width distributed across three flex columns ── */
    {
        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 1,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .total_width = 60,
        });
        sc_columns_add_str(
            columns, "Left column\nwith two lines",
            (ScColItem){ .halign = SC_ALIGN_LEFT }
        );
        sc_columns_add_str(
            columns, "Centered column",
            (ScColItem){ .halign = SC_ALIGN_CENTER }
        );
        sc_columns_add_str(
            columns, "Right column\nalso two lines",
            (ScColItem){ .halign = SC_ALIGN_RIGHT }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }

    printf("\n");

    /* ── 5. Nested columns ── */
    {
        ScTableData *config = sc_table_new();
        sc_table_add_column(config, "Key", col_left);
        sc_table_add_column(config, "Value", col_left);
        sc_table_add_row(config, (ScCell[]){
            sc_cell("env"), sc_cell("production")
        }, 2);
        sc_table_add_row(config, (ScCell[]){
            sc_cell("region"), sc_cell("eu-west-1")
        }, 2);
        sc_table_add_row(config, (ScCell[]){
            sc_cell("version"), sc_cell("2.4.1")
        }, 2);

        ScColumns *inner = sc_columns_new((ScColumnsOpts){
            .gap = 2,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_str(
            inner, "CPU:  34 %\nMEM:  61 %\nDISK: 48 %",
            (ScColItem){ .fixed_w = 14 }
        );
        sc_columns_add_str(
            inner,
            "NET-IN:  1.2 MB/s\nNET-OUT: 0.4 MB/s\nPKTS:    12 403",
            (ScColItem){ .fixed_w = 18 }
        );

        ScColumns *outer = sc_columns_new((ScColumnsOpts){
            .gap = 3,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_table(outer, config, (ScTableOpts){
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        }, (ScColItem){ 0 });
        sc_columns_add_columns(outer, inner, (ScColItem){ 0 });
        sc_columns_print(outer);

        sc_columns_free(outer);
        sc_columns_free(inner);
        sc_table_free(config);
    }

    printf("\n");

    /* ── 6. min_w / max_w / fixed_w + item.halign ── */
    {
        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 2,
            .sep = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE },
        });
        sc_columns_add_str(
            columns, "fixed=20\nshort",
            (ScColItem){ .fixed_w = 20, .halign = SC_ALIGN_LEFT }
        );
        sc_columns_add_str(
            columns, "min=12, auto-width",
            (ScColItem){ .min_w = 12, .halign = SC_ALIGN_CENTER }
        );
        sc_columns_add_str(
            columns, "max=10, this text is longer than the limit",
            (ScColItem){ .max_w = 10, .halign = SC_ALIGN_RIGHT }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }

    printf("\n");

    /* ── 7. Separator: double border, colored ── */
    {
        ScTableData *revenue = sc_table_new();
        sc_table_add_column(revenue, "Quarter", col_left);
        sc_table_add_column(revenue, "EUR", col_right);
        sc_table_add_row(revenue, (ScCell[]){
            sc_cell("Q1"), sc_cell("1 240 000")
        }, 2);
        sc_table_add_row(revenue, (ScCell[]){
            sc_cell("Q2"), sc_cell("1 580 000")
        }, 2);
        sc_table_add_row(revenue, (ScCell[]){
            sc_cell("Q3"), sc_cell("1 390 000")
        }, 2);
        sc_table_add_row(revenue, (ScCell[]){
            sc_cell("Q4"), sc_cell("1 820 000")
        }, 2);

        ScTableData *costs = sc_table_new();
        sc_table_add_column(costs, "Quarter", col_left);
        sc_table_add_column(costs, "EUR", col_right);
        sc_table_add_row(costs, (ScCell[]){
            sc_cell("Q1"), sc_cell("980 000")
        }, 2);
        sc_table_add_row(costs, (ScCell[]){
            sc_cell("Q2"), sc_cell("1 100 000")
        }, 2);
        sc_table_add_row(costs, (ScCell[]){
            sc_cell("Q3"), sc_cell("870 000")
        }, 2);
        sc_table_add_row(costs, (ScCell[]){
            sc_cell("Q4"), sc_cell("1 250 000")
        }, 2);

        ScTableData *profit = sc_table_new();
        sc_table_add_column(profit, "Quarter", col_left);
        sc_table_add_column(profit, "EUR", col_right);
        sc_table_add_row(profit, (ScCell[]){
            sc_cell("Q1"), sc_cell("260 000")
        }, 2);
        sc_table_add_row(profit, (ScCell[]){
            sc_cell("Q2"), sc_cell("480 000")
        }, 2);
        sc_table_add_row(profit, (ScCell[]){
            sc_cell("Q3"), sc_cell("520 000")
        }, 2);
        sc_table_add_row(profit, (ScCell[]){
            sc_cell("Q4"), sc_cell("570 000")
        }, 2);

        ScTableOpts revenue_opts = {
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Revenue ", bold_cyan),
            .cell_pad = { 0, 1, 0, 1 },
        };
        ScTableOpts costs_opts = {
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Costs ", bold_yellow),
            .cell_pad = { 0, 1, 0, 1 },
        };
        ScTableOpts profit_opts = {
            .border = default_table_border(),
            .header.row = true,
            .header.style = bold,
            .title = title_centered_top(" Profit ", bold_green),
            .cell_pad = { 0, 1, 0, 1 },
        };

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 3,
            .sep = {
                .type = SC_BORDER_DOUBLE,
                .color = sc_color_from_rgb(80, 80, 140),
            },
        });
        sc_columns_add_table(columns, revenue, revenue_opts, (ScColItem){ 0 });
        sc_columns_add_table(columns, costs, costs_opts, (ScColItem){ 0 });
        sc_columns_add_table(columns, profit, profit_opts, (ScColItem){ 0 });
        sc_columns_print(columns);
        sc_columns_free(columns);
        sc_table_free(revenue);
        sc_table_free(costs);
        sc_table_free(profit);
    }

    printf("\n");
}
