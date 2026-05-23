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
        ScTable *t = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = { .text = " Summary ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE }, .pos = SC_TITLE_TOP, .align = SC_ALIGN_CENTER, .pad = 1 },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(t, "Item",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(t, "Total", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Passed"),  SC_CELL("147") }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Failed"),  SC_CELL("0")   }, 2);
        sc_table_add_row(t, (ScCell[]){ SC_CELL("Skipped"), SC_CELL("3")   }, 2);

        ScRendered *r = sc_capture_table(t);
        sc_align_print(r, SC_ALIGN_CENTER, 0);
        sc_rendered_free(r);
        sc_table_free(t);
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
                .title_style  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE },
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

    /* ── 5. sc_columns_add_rendered: pad left + align center ── */
    printf("--- Align 5. columns_add_rendered: pad left + align center ---\n");
    {
        ScTable *ta = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = { .text = " Left (padded) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE }, .pos = SC_TITLE_TOP, .align = SC_ALIGN_CENTER, .pad = 1 },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(ta, "A", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(ta, "B", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(ta, (ScCell[]){ SC_CELL("Alpha"), SC_CELL("10") }, 2);
        sc_table_add_row(ta, (ScCell[]){ SC_CELL("Beta"),  SC_CELL("20") }, 2);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders    = { SC_BORDER_SINGLE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE,
                            SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header_row = 1,
            .header_opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = { .text = " Right (captured) ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE }, .pos = SC_TITLE_TOP, .align = SC_ALIGN_CENTER, .pad = 1 },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_col(tb, "X", (ScColOpts){0,0,8, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_col(tb, "Y", (ScColOpts){0,0,6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Gamma"), SC_CELL("30") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Delta"), SC_CELL("40") }, 2);
        sc_table_add_row(tb, (ScCell[]){ SC_CELL("Eps"),   SC_CELL("50") }, 2);

        ScRendered *ra = sc_capture_table(ta);
        ScRendered *rb = sc_capture_table(tb);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 4,
            .sep = { .style = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE },
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
