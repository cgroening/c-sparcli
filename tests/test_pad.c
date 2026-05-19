#include "sparcli.h"
#include <stdio.h>


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

    /* ── 4. Indent a panel ── */
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
