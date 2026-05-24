#include "sparcli.h"
#include <stdio.h>


void test_trees(void) {
    static const ScTextStyle PLAIN = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
    static const ScTextStyle BOLD  = { SC_TEXT_ATTR_BOLD,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };

    printf("\n");

    /* ── 1. Directory structure (SINGLE, colored connectors) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTextStyle dir  = { SC_TEXT_ATTR_BOLD,   SC_ANSI_COLOR_CYAN,  SC_ANSI_COLOR_NONE };
        ScTextStyle file = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle ic_d = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_CYAN,  SC_ANSI_COLOR_NONE };
        ScTextStyle ic_f = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };

        ScTreeNode *src  = sc_tree_add_str(t, NULL, "src/",     dir,  "▸ ", ic_d);
        ScTreeNode *inc  = sc_tree_add_str(t, NULL, "include/", dir,  "▸ ", ic_d);
        ScTreeNode *tst  = sc_tree_add_str(t, NULL, "tests/",   dir,  "▸ ", ic_d);
                          sc_tree_add_str(t, NULL, "Makefile",  file, "· ", ic_f);

        sc_tree_add_str(t, src, "main.c",    file, "· ", ic_f);
        sc_tree_add_str(t, src, "panel.c",   file, "· ", ic_f);
        sc_tree_add_str(t, src, "table.c",   file, "· ", ic_f);
        sc_tree_add_str(t, src, "tree.c",    file, "· ", ic_f);

        sc_tree_add_str(t, inc, "sparcli.h", file, "· ", ic_f);

        ScTreeNode *fix = sc_tree_add_str(t, tst, "fixtures/", dir,  "▸ ", ic_d);
        sc_tree_add_str(t, tst, "test_main.c", file, "· ", ic_f);
        sc_tree_add_str(t, fix, "sample.json", file, "· ", ic_f);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 2. Category tree, multiple roots (ROUNDED) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_ROUNDED,
            .connector_color = sc_ansi_color_from_rgb(100, 100, 100),
        });
        ScTextStyle root_o = { SC_TEXT_ATTR_BOLD,  SC_ANSI_COLOR_WHITE,   SC_ANSI_COLOR_NONE };
        ScTextStyle cat_o  = { SC_TEXT_ATTR_NONE,  SC_ANSI_COLOR_YELLOW,  SC_ANSI_COLOR_NONE };
        ScTextStyle item_o = { SC_TEXT_ATTR_NONE,  SC_ANSI_COLOR_NONE,    SC_ANSI_COLOR_NONE };

        ScTreeNode *fe = sc_tree_add_str(t, NULL, "Frontend", root_o, NULL, PLAIN);
        ScTreeNode *be = sc_tree_add_str(t, NULL, "Backend",  root_o, NULL, PLAIN);
        ScTreeNode *db = sc_tree_add_str(t, NULL, "Database", root_o, NULL, PLAIN);

        ScTreeNode *react = sc_tree_add_str(t, fe, "React",   cat_o, NULL, PLAIN);
        ScTreeNode *css   = sc_tree_add_str(t, fe, "CSS",     cat_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Components", item_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Hooks",      item_o, NULL, PLAIN);
        sc_tree_add_str(t, react, "Context",    item_o, NULL, PLAIN);
        sc_tree_add_str(t, css,   "Tailwind",   item_o, NULL, PLAIN);
        sc_tree_add_str(t, css,   "Modules",    item_o, NULL, PLAIN);

        ScTreeNode *api = sc_tree_add_str(t, be, "REST API", cat_o, NULL, PLAIN);
        sc_tree_add_str(t, be, "Auth",      cat_o, NULL, PLAIN);
        sc_tree_add_str(t, api, "Routes",   item_o, NULL, PLAIN);
        sc_tree_add_str(t, api, "Middleware", item_o, NULL, PLAIN);

        sc_tree_add_str(t, db, "PostgreSQL", item_o, NULL, PLAIN);
        sc_tree_add_str(t, db, "Redis",      item_o, NULL, PLAIN);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 3. Prefix icons with color (status tree) ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = sc_ansi_color_from_rgb(80, 80, 80),
        });
        ScTextStyle ok_ic  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,  SC_ANSI_COLOR_NONE };
        ScTextStyle err_ic = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,    SC_ANSI_COLOR_NONE };
        ScTextStyle wrn_ic = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE };
        ScTextStyle ok_tx  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,   SC_ANSI_COLOR_NONE };
        ScTextStyle err_tx = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED,    SC_ANSI_COLOR_NONE };
        ScTextStyle wrn_tx = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE };

        ScTreeNode *svc = sc_tree_add_str(t, NULL, "Services",
            (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE }, NULL, PLAIN);
        ScTreeNode *web = sc_tree_add_str(t, svc, "web-server",   ok_tx,  "● ", ok_ic);
        ScTreeNode *api = sc_tree_add_str(t, svc, "api-gateway",  wrn_tx, "◆ ", wrn_ic);
                          sc_tree_add_str(t, svc, "auth-service",  err_tx, "✖ ", err_ic);

        sc_tree_add_str(t, web, "port 80   → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, web, "port 443  → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, api, "port 8080 → ok",   ok_tx,  "· ", ok_ic);
        sc_tree_add_str(t, api, "latency   → high", wrn_tx, "· ", wrn_ic);

        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 4. Different line styles ── */
    {
        ScTree *ta = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_ASCII, .connector_color = SC_ANSI_COLOR_NONE });
        ScTreeNode *a1 = sc_tree_add_str(ta, NULL, "root (ASCII)", BOLD, NULL, PLAIN);
        ScTreeNode *a2 = sc_tree_add_str(ta, a1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(ta, a1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(ta, a2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(ta);
        sc_tree_free(ta);
        printf("\n");

        ScTree *td = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_DOUBLE, .connector_color = SC_ANSI_COLOR_NONE });
        ScTreeNode *d1 = sc_tree_add_str(td, NULL, "root (DOUBLE)", BOLD, NULL, PLAIN);
        ScTreeNode *d2 = sc_tree_add_str(td, d1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(td, d1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(td, d2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(td);
        sc_tree_free(td);
        printf("\n");

        ScTree *tt = sc_tree_new((ScTreeOpts){ .style = SC_BORDER_THICK, .connector_color = sc_ansi_color_from_rgb(120, 80, 200) });
        ScTreeNode *t1 = sc_tree_add_str(tt, NULL, "root (THICK, colored)", BOLD, NULL, PLAIN);
        ScTreeNode *t2 = sc_tree_add_str(tt, t1, "child A", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(tt, t1, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(tt, t2, "leaf",    PLAIN, NULL, PLAIN);
        sc_tree_print(tt);
        sc_tree_free(tt);
    }

    printf("\n");

    /* ── 5. no_guide = 1 ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style    = SC_BORDER_SINGLE,
            .no_guide = 1,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTreeNode *r  = sc_tree_add_str(t, NULL, "root", BOLD, NULL, PLAIN);
        ScTreeNode *c1 = sc_tree_add_str(t, r, "child A", PLAIN, NULL, PLAIN);
        ScTreeNode *c2 = sc_tree_add_str(t, r, "child B", PLAIN, NULL, PLAIN);
                         sc_tree_add_str(t, r, "child C", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c1, "leaf A1", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c1, "leaf A2", PLAIN, NULL, PLAIN);
        sc_tree_add_str(t, c2, "leaf B1", PLAIN, NULL, PLAIN);
        sc_tree_print(t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 6. ScText variant with mixed spans ── */
    {
        ScTree *t = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = SC_ANSI_COLOR_CYAN,
        });

        ScText *root_t = sc_text_new();
        sc_text_append(root_t, "Deployment ", (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE });
        sc_text_append(root_t, "v2.4.1",      (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,  SC_ANSI_COLOR_NONE });
        ScTreeNode *root = sc_tree_add_text(t, NULL, root_t, NULL, PLAIN);

        ScText *ok_t = sc_text_new();
        sc_text_append(ok_t, "build   ", (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE });
        sc_text_append(ok_t, "PASSED",   (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE });

        ScText *fail_t = sc_text_new();
        sc_text_append(fail_t, "tests   ", (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
        sc_text_append(fail_t, "FAILED",   (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED,  SC_ANSI_COLOR_NONE });

        ScText *skip_t = sc_text_new();
        sc_text_append(skip_t, "deploy  ", (ScTextStyle){ SC_TEXT_ATTR_NONE,  SC_ANSI_COLOR_NONE,   SC_ANSI_COLOR_NONE });
        sc_text_append(skip_t, "SKIPPED", (ScTextStyle){ SC_TEXT_ATTR_NONE,  SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE });

        sc_tree_add_text(t, root, ok_t,   NULL, PLAIN);
        sc_tree_add_text(t, root, fail_t, NULL, PLAIN);
        sc_tree_add_text(t, root, skip_t, NULL, PLAIN);

        sc_tree_print(t);
        sc_text_free(root_t);
        sc_text_free(ok_t);
        sc_text_free(fail_t);
        sc_text_free(skip_t);
        sc_tree_free(t);
    }

    printf("\n");

    /* ── 7. Tree next to table in columns ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .style           = SC_BORDER_SINGLE,
            .connector_color = sc_ansi_color_from_rgb(80, 140, 80),
        });
        ScTextStyle dir_o  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE };
        ScTextStyle file_o = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };
        ScTextStyle ic_d   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE };
        ScTextStyle ic_f   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,  SC_ANSI_COLOR_NONE };

        ScTreeNode *app  = sc_tree_add_str(tree, NULL, "app/",       dir_o,  "▸ ", ic_d);
        ScTreeNode *lib  = sc_tree_add_str(tree, NULL, "lib/",       dir_o,  "▸ ", ic_d);
        ScTreeNode *comp = sc_tree_add_str(tree, app,  "components/",dir_o,  "▸ ", ic_d);
                           sc_tree_add_str(tree, app,  "main.ts",    file_o, "· ", ic_f);
        sc_tree_add_str(tree, comp, "Button.tsx", file_o, "· ", ic_f);
        sc_tree_add_str(tree, comp, "Input.tsx",  file_o, "· ", ic_f);
        sc_tree_add_str(tree, lib,  "utils.ts",   file_o, "· ", ic_f);
        sc_tree_add_str(tree, lib,  "api.ts",     file_o, "· ", ic_f);

        ScTable *tb = sc_table_new((ScTableOpts){
            .borders     = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row  = 1,
            .header.opts = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
            .title = {.text = " File Stats ", .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE }, .align = SC_ALIGN_CENTER, .pad = 1, .pos = SC_POSITION_TOP },
            .cell_pad = {0, 1, 0, 1},
        });
        sc_table_add_column(tb, "File",  (ScColOpts){0,0,0, SC_ALIGN_LEFT,  SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Lines", (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_column(tb, "Size",  (ScColOpts){0,0,0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE});
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Button.tsx"), sc_cell("142"), sc_cell("3.2 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("Input.tsx"),  sc_cell( "89"), sc_cell("1.8 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("main.ts"),    sc_cell( "34"), sc_cell("0.7 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("utils.ts"),   sc_cell("210"), sc_cell("4.9 KB") }, 3);
        sc_table_add_row(tb, (ScCell[]){ sc_cell("api.ts"),     sc_cell("178"), sc_cell("3.8 KB") }, 3);

        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 3,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign    = SC_VALIGN_TOP,
        });
        sc_columns_add_tree(cl, tree, (ScColItem){0});
        sc_columns_add_table(cl, tb, (ScColItem){0});
        sc_columns_print(cl);

        sc_columns_free(cl);
        sc_tree_free(tree);
        sc_table_free(tb);
    }
}
