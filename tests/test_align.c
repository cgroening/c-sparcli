#include "sparcli.h"
#include <stdio.h>


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
        ScTableData *table_data = sc_table_new();
        sc_table_add_column(table_data, "Item",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data, "Total", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("Passed"),  sc_cell("147") }, 2);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("Failed"),  sc_cell("0")   }, 2);
        sc_table_add_row(table_data, (ScCell[]){ sc_cell("Skipped"), sc_cell("3")   }, 2);

        ScRendered *r = sc_capture_table(table_data, (ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Summary ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_align_print(r, SC_ALIGN_CENTER, 0);
        sc_rendered_free(r);
        sc_table_free(table_data);
    }

    printf("\n");

    /* ── 4. Same capture used twice: padded then centered ── */
    printf("--- Align 4. Same capture used twice: padded then centered ---\n");
    {
        ScRendered *r = sc_capture_rule_str(
            "Section Header",
            (ScRuleOpts){
                .style       = SC_BORDER_SINGLE,
                .color       = SC_ANSI_COLOR_YELLOW,
                .title.style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE },
                .title.align = SC_ALIGN_CENTER,
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

    /* ── 5. sc_columns_add_rendered: pad left + align center ── */
    printf("--- Align 5. columns_add_rendered: pad left + align center ---\n");
    {
        ScTableData *table_data_1 = sc_table_new();
        sc_table_add_column(table_data_1, "A", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_1, "B", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Alpha"), sc_cell("10") }, 2);
        sc_table_add_row(table_data_1, (ScCell[]){ sc_cell("Beta"),  sc_cell("20") }, 2);

        ScTableData *table_data_2 = sc_table_new();
        sc_table_add_column(table_data_2, "X", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(table_data_2, "Y", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Gamma"), sc_cell("30") }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Delta"), sc_cell("40") }, 2);
        sc_table_add_row(table_data_2, (ScCell[]){ sc_cell("Eps"),   sc_cell("50") }, 2);

        ScRendered *ra = sc_capture_table(table_data_1, (ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Left (padded) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        });
        ScRendered *rb = sc_capture_table(table_data_2, (ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row = 1,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " Right (captured) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE },
                      .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        });

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 4,
            .sep = { .type = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE },
        });
        sc_columns_add_rendered(cl, ra, (ScColItem){ 0 });
        sc_columns_add_rendered(cl, rb, (ScColItem){ 0 });
        sc_columns_print(cl);
        sc_columns_free(cl);

        sc_rendered_free(ra);
        sc_rendered_free(rb);
        sc_table_free(table_data_1);
        sc_table_free(table_data_2);
    }
}
