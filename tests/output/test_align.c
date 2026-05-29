#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_magenta = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_cyan = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_green = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold_yellow = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
};


void test_align(void) {
    printf("\n");

    /* ── 1. Center a string ── */
    printf("--- Align 1. Center string ---\n");
    sc_align_str("──── Centered Title ────", SC_ALIGN_CENTER, 0);

    printf("\n");

    /* ── 2. Right-align a string ── */
    printf("--- Align 2. Right-align string ---\n");
    sc_align_str("Right-aligned text", SC_ALIGN_RIGHT, 0);
    sc_align_str("Short", SC_ALIGN_RIGHT, 0);

    printf("\n");

    /* ── 3. Center-align a table ── */
    printf("--- Align 3. Center-aligned table ---\n");
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Item", (ScColOpts){
            0, 0, 0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(table, "Total", (ScColOpts){
            0, 0, 0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Passed"), sc_cell("147")
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Failed"), sc_cell("0")
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Skipped"), sc_cell("3")
        }, 2);

        ScRendered *rendered = sc_capture_table(table, (ScTableOpts){
            .border = {
                SC_BORDER_SINGLE,
                SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0
            },
            .header.row = true,
            .header.style = bold,
            .title = {
                .text = " Summary ",
                .style = bold_magenta,
                .halign = SC_ALIGN_CENTER,
                .pad = 1,
                .pos = SC_POSITION_TOP,
            },
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_align_print(rendered, SC_ALIGN_CENTER, 0);
        sc_rendered_free(rendered);
        sc_table_free(table);
    }

    printf("\n");

    /* ── 4. Same capture used twice: padded then centered ── */
    printf("--- Align 4. Same capture used twice: padded then centered ---\n");
    {
        ScRendered *rendered = sc_capture_rule_str(
            "Section Header",
            (ScRuleOpts){
                .type = SC_BORDER_SINGLE,
                .color = SC_ANSI_COLOR_YELLOW,
                .title.style = bold_yellow,
                .title.halign = SC_ALIGN_CENTER,
                .width = 40,
            }
        );
        printf("  padded (left-aligned):\n");
        sc_pad_print(rendered, (ScPadOpts){ .left = 4 });
        printf("  aligned (centered):\n");
        sc_align_print(rendered, SC_ALIGN_CENTER, 0);
        sc_rendered_free(rendered);
    }

    printf("\n");

    /* ── 5. sc_columns_add_rendered: pad left + align center ── */
    printf("--- Align 5. columns_add_rendered: pad left + align center ---\n");
    {
        ScTableData *left_table = sc_table_new();
        sc_table_add_column(left_table, "A", (ScColOpts){
            0, 0, 8, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(left_table, "B", (ScColOpts){
            0, 0, 6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_row(left_table, (ScCell[]){
            sc_cell("Alpha"), sc_cell("10")
        }, 2);
        sc_table_add_row(left_table, (ScCell[]){
            sc_cell("Beta"), sc_cell("20")
        }, 2);

        ScTableData *right_table = sc_table_new();
        sc_table_add_column(right_table, "X", (ScColOpts){
            0, 0, 8, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(right_table, "Y", (ScColOpts){
            0, 0, 6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_row(right_table, (ScCell[]){
            sc_cell("Gamma"), sc_cell("30")
        }, 2);
        sc_table_add_row(right_table, (ScCell[]){
            sc_cell("Delta"), sc_cell("40")
        }, 2);
        sc_table_add_row(right_table, (ScCell[]){
            sc_cell("Eps"), sc_cell("50")
        }, 2);

        ScRendered *left_rendered = sc_capture_table(
            left_table,
            (ScTableOpts){
                .border = {
                    SC_BORDER_SINGLE,
                    SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                    0, 0, 0
                },
                .header.row = true,
                .header.style = bold,
                .title = {
                    .text = " Left (padded) ",
                    .style = bold_cyan,
                    .halign = SC_ALIGN_CENTER,
                    .pad = 1,
                    .pos = SC_POSITION_TOP,
                },
                .cell_pad = { 0, 1, 0, 1 },
            }
        );
        ScRendered *right_rendered = sc_capture_table(
            right_table,
            (ScTableOpts){
                .border = {
                    SC_BORDER_SINGLE,
                    SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE,
                    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                    0, 0, 0
                },
                .header.row = true,
                .header.style = bold,
                .title = {
                    .text = " Right (captured) ",
                    .style = bold_green,
                    .halign = SC_ALIGN_CENTER,
                    .pad = 1,
                    .pos = SC_POSITION_TOP,
                },
                .cell_pad = { 0, 1, 0, 1 },
            }
        );

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 4,
            .sep = { .type = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE },
        });
        sc_columns_add_rendered(columns, left_rendered, (ScColItem){ 0 });
        sc_columns_add_rendered(columns, right_rendered, (ScColItem){ 0 });
        sc_columns_print(columns);
        sc_columns_free(columns);

        sc_rendered_free(left_rendered);
        sc_rendered_free(right_rendered);
        sc_table_free(left_table);
        sc_table_free(right_table);
    }

    printf("\n");

    /* ── 6. sc_vstack: two widgets stacked in one column ── */
    printf("--- Align 6. sc_vstack column (rule over rule, gap=1) ---\n");
    {
        ScRendered *top = sc_capture_rule_str("Top", (ScRuleOpts){
            .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_CYAN,
            .title.style = bold_cyan, .title.halign = SC_ALIGN_CENTER,
            .title.pad = 1, .width = 30,
        });
        ScRendered *bottom = sc_capture_rule_str("Bottom", (ScRuleOpts){
            .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_NONE,
            .title.style = bold_green, .title.halign = SC_ALIGN_CENTER,
            .title.pad = 1, .width = 30,
        });
        ScRendered *parts[] = { top, bottom };
        ScRendered *stacked = sc_vstack(
            (const ScRendered *const *)parts, 2, 1
        );

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 3,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
        });
        sc_columns_add_str     (columns, "left column", (ScColItem){ 0 });
        sc_columns_add_rendered(columns, stacked,       (ScColItem){ 0 });
        sc_columns_print(columns);
        sc_columns_free(columns);

        sc_rendered_free(stacked);
        sc_rendered_free(top);
        sc_rendered_free(bottom);
    }
}
