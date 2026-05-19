#include "sparcli.h"
#include <stdio.h>


void test_kv(void) {
    printf("\n");

    /* ── 1. Plain: default 2-space separator, auto key width ── */
    printf("--- KV 1. Plain ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){ 0 });
        sc_kv_add(kv, "Host",     "localhost");
        sc_kv_add(kv, "Port",     "8080");
        sc_kv_add(kv, "Database", "myapp_db");
        sc_kv_add(kv, "User",     "admin");
        sc_kv_add(kv, "Status",   "running");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 2. Custom separator, bold keys, green values ── */
    printf("--- KV 2. Styled with ': ' separator ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .sep      = ": ",
            .key_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .val_opts = { SC_STYLE_NONE, SC_COLOR_GREEN, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Version",  "2.4.1");
        sc_kv_add(kv, "License",  "MIT");
        sc_kv_add(kv, "Author",   "sparcli contributors");
        sc_kv_add(kv, "Language", "C11");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 3. Fixed key_width + word-wrap ── */
    printf("--- KV 3. Fixed key_width=18, word-wrap ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .key_width = 18,
            .sep       = ": ",
            .wrap_val  = 1,
            .key_opts  = { SC_STYLE_BOLD, SC_COLOR_CYAN, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Name",        "sparcli");
        sc_kv_add(kv, "Description", "A C11 terminal UI library for styled output "
                                     "with colored text, panels, tables, lists, trees, "
                                     "progress bars and spinners.");
        sc_kv_add(kv, "Build",       "make && make test");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }

    printf("\n");

    /* ── 4. Margin + item_gap ── */
    printf("--- KV 4. Margin=4, item_gap=1 ---\n");
    {
        ScKV *kv = sc_kv_new((ScKVOpts){
            .margin   = 4,
            .item_gap = 1,
            .sep      = "  \xe2\x86\x92  ", /* →  */
            .key_opts = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
        });
        sc_kv_add(kv, "Input",  "raw_data.csv");
        sc_kv_add(kv, "Output", "processed.json");
        sc_kv_add(kv, "Format", "JSON Lines");
        sc_kv_print(kv);
        sc_kv_free(kv);
    }
}
