#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};


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
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Name", (ScColOpts){ 0 });
        sc_table_add_column(table, "Score",
            (ScColOpts){ .halign = SC_ALIGN_RIGHT });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Alice"), sc_cell("9800")
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Bob"), sc_cell("7650")
        }, 2);

        ScRendered *rendered = sc_capture_table(table, (ScTableOpts){
            .border = {
                SC_BORDER_SINGLE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0
            },
            .header.row = true,
            .header.style = bold,
            .cell_pad = { 0, 1, 0, 1 },
        });
        sc_pad_print(rendered, (ScPadOpts){
            .top = 1, .bottom = 1, .left = 4
        });
        sc_rendered_free(rendered);
        sc_table_free(table);
    }

    /* ── 3. Indent a KV list with left padding ── */
    printf("--- Pad 3. KV list left=6 ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .sep = ": ",
            .key_style = bold,
        });
        sc_kv_add(kv, "Host", "localhost");
        sc_kv_add(kv, "Port", "5432");
        sc_kv_add(kv, "DB", "myapp");

        ScRendered *rendered = sc_capture_kv(kv);
        sc_pad_print(rendered, (ScPadOpts){ .left = 6 });
        sc_rendered_free(rendered);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 4. Indent a panel ── */
    printf("--- Pad 4. Panel indented by left=4 ---\n");
    {
        ScRendered *rendered = sc_capture_panel_str(
            "This panel is indented\nwithout being inside Columns.",
            (ScPanelOpts){
                .border = {
                    .type = SC_BORDER_ROUNDED,
                    .color = SC_ANSI_COLOR_CYAN,
                },
                .padding = { 0, 1, 0, 1 },
                .width = 40,
            }
        );
        sc_pad_print(rendered, (ScPadOpts){ .left = 4 });
        sc_rendered_free(rendered);
    }
}
