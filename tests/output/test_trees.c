#include "sparcli.h"
#include <stdio.h>


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle bold = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};


void test_trees(void) {
    printf("\n");

    /* ── 1. Directory structure (SINGLE, colored connectors) ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_SINGLE,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTextStyle dir_text = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
        };
        ScTextStyle file_text = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };
        ScTextStyle dir_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
        };
        ScTextStyle file_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };

        ScTreeNode *src = sc_tree_add_str(
            tree, NULL, "src/", dir_text, "▸ ", dir_icon
        );
        ScTreeNode *include = sc_tree_add_str(
            tree, NULL, "include/", dir_text, "▸ ", dir_icon
        );
        ScTreeNode *tests = sc_tree_add_str(
            tree, NULL, "tests/", dir_text, "▸ ", dir_icon
        );
        sc_tree_add_str(tree, NULL, "Makefile", file_text, "· ", file_icon);

        sc_tree_add_str(tree, src, "main.c",  file_text, "· ", file_icon);
        sc_tree_add_str(tree, src, "panel.c", file_text, "· ", file_icon);
        sc_tree_add_str(tree, src, "table.c", file_text, "· ", file_icon);
        sc_tree_add_str(tree, src, "tree.c",  file_text, "· ", file_icon);

        sc_tree_add_str(
            tree, include, "sparcli.h", file_text, "· ", file_icon
        );

        ScTreeNode *fixtures = sc_tree_add_str(
            tree, tests, "fixtures/", dir_text, "▸ ", dir_icon
        );
        sc_tree_add_str(
            tree, tests, "test_main.c", file_text, "· ", file_icon
        );
        sc_tree_add_str(
            tree, fixtures, "sample.json", file_text, "· ", file_icon
        );

        sc_tree_print(tree);
        sc_tree_free(tree);
    }

    printf("\n");

    /* ── 2. Category tree, multiple roots (ROUNDED) ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_ROUNDED,
            .connector_color = sc_ansi_color_from_rgb(100, 100, 100),
        });
        ScTextStyle root_style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_NONE
        };
        ScTextStyle category_style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
        };
        ScTextStyle item_style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };

        ScTreeNode *frontend = sc_tree_add_str(
            tree, NULL, "Frontend", root_style, NULL, plain
        );
        ScTreeNode *backend = sc_tree_add_str(
            tree, NULL, "Backend", root_style, NULL, plain
        );
        ScTreeNode *database = sc_tree_add_str(
            tree, NULL, "Database", root_style, NULL, plain
        );

        ScTreeNode *react = sc_tree_add_str(
            tree, frontend, "React", category_style, NULL, plain
        );
        ScTreeNode *css = sc_tree_add_str(
            tree, frontend, "CSS", category_style, NULL, plain
        );
        sc_tree_add_str(tree, react, "Components", item_style, NULL, plain);
        sc_tree_add_str(tree, react, "Hooks", item_style, NULL, plain);
        sc_tree_add_str(tree, react, "Context", item_style, NULL, plain);
        sc_tree_add_str(tree, css, "Tailwind", item_style, NULL, plain);
        sc_tree_add_str(tree, css, "Modules", item_style, NULL, plain);

        ScTreeNode *api = sc_tree_add_str(
            tree, backend, "REST API", category_style, NULL, plain
        );
        sc_tree_add_str(tree, backend, "Auth", category_style, NULL, plain);
        sc_tree_add_str(tree, api, "Routes", item_style, NULL, plain);
        sc_tree_add_str(tree, api, "Middleware", item_style, NULL, plain);

        sc_tree_add_str(tree, database, "PostgreSQL", item_style, NULL, plain);
        sc_tree_add_str(tree, database, "Redis", item_style, NULL, plain);

        sc_tree_print(tree);
        sc_tree_free(tree);
    }

    printf("\n");

    /* ── 3. Prefix icons with color (status tree) ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_SINGLE,
            .connector_color = sc_ansi_color_from_rgb(80, 80, 80),
        });
        ScTextStyle ok_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        };
        ScTextStyle error_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
        };
        ScTextStyle warning_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
        };
        ScTextStyle ok_text = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };
        ScTextStyle error_text = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
        };
        ScTextStyle warning_text = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
        };
        ScTextStyle services_style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };

        ScTreeNode *services = sc_tree_add_str(
            tree, NULL, "Services", services_style, NULL, plain
        );
        ScTreeNode *web = sc_tree_add_str(
            tree, services, "web-server", ok_text, "● ", ok_icon
        );
        ScTreeNode *api = sc_tree_add_str(
            tree, services, "api-gateway", warning_text, "◆ ", warning_icon
        );
        sc_tree_add_str(
            tree, services, "auth-service", error_text, "✖ ", error_icon
        );

        sc_tree_add_str(
            tree, web, "port 80   → ok", ok_text, "· ", ok_icon
        );
        sc_tree_add_str(
            tree, web, "port 443  → ok", ok_text, "· ", ok_icon
        );
        sc_tree_add_str(
            tree, api, "port 8080 → ok", ok_text, "· ", ok_icon
        );
        sc_tree_add_str(
            tree, api, "latency   → high", warning_text, "· ", warning_icon
        );

        sc_tree_print(tree);
        sc_tree_free(tree);
    }

    printf("\n");

    /* ── 4. Different line styles ── */
    {
        ScTree *ascii_tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_ASCII,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTreeNode *ascii_root = sc_tree_add_str(
            ascii_tree, NULL, "root (ASCII)", bold, NULL, plain
        );
        ScTreeNode *ascii_child = sc_tree_add_str(
            ascii_tree, ascii_root, "child A", plain, NULL, plain
        );
        sc_tree_add_str(
            ascii_tree, ascii_root, "child B", plain, NULL, plain
        );
        sc_tree_add_str(
            ascii_tree, ascii_child, "leaf", plain, NULL, plain
        );
        sc_tree_print(ascii_tree);
        sc_tree_free(ascii_tree);
        printf("\n");

        ScTree *double_tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_DOUBLE,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTreeNode *double_root = sc_tree_add_str(
            double_tree, NULL, "root (DOUBLE)", bold, NULL, plain
        );
        ScTreeNode *double_child = sc_tree_add_str(
            double_tree, double_root, "child A", plain, NULL, plain
        );
        sc_tree_add_str(
            double_tree, double_root, "child B", plain, NULL, plain
        );
        sc_tree_add_str(
            double_tree, double_child, "leaf", plain, NULL, plain
        );
        sc_tree_print(double_tree);
        sc_tree_free(double_tree);
        printf("\n");

        ScTree *thick_tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_THICK,
            .connector_color = sc_ansi_color_from_rgb(120, 80, 200),
        });
        ScTreeNode *thick_root = sc_tree_add_str(
            thick_tree, NULL, "root (THICK, colored)", bold, NULL, plain
        );
        ScTreeNode *thick_child = sc_tree_add_str(
            thick_tree, thick_root, "child A", plain, NULL, plain
        );
        sc_tree_add_str(
            thick_tree, thick_root, "child B", plain, NULL, plain
        );
        sc_tree_add_str(
            thick_tree, thick_child, "leaf", plain, NULL, plain
        );
        sc_tree_print(thick_tree);
        sc_tree_free(thick_tree);
    }

    printf("\n");

    /* ── 5. no_guide = true ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_SINGLE,
            .no_guide = true,
            .connector_color = SC_ANSI_COLOR_NONE,
        });
        ScTreeNode *root = sc_tree_add_str(
            tree, NULL, "root", bold, NULL, plain
        );
        ScTreeNode *child_a = sc_tree_add_str(
            tree, root, "child A", plain, NULL, plain
        );
        ScTreeNode *child_b = sc_tree_add_str(
            tree, root, "child B", plain, NULL, plain
        );
        sc_tree_add_str(tree, root, "child C", plain, NULL, plain);
        sc_tree_add_str(tree, child_a, "leaf A1", plain, NULL, plain);
        sc_tree_add_str(tree, child_a, "leaf A2", plain, NULL, plain);
        sc_tree_add_str(tree, child_b, "leaf B1", plain, NULL, plain);
        sc_tree_print(tree);
        sc_tree_free(tree);
    }

    printf("\n");

    /* ── 6. ScText variant with mixed spans ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_SINGLE,
            .connector_color = SC_ANSI_COLOR_CYAN,
        });

        ScText *root_text = sc_text_new();
        sc_text_append(root_text, "Deployment ", (ScTextStyle){
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        });
        sc_text_append(root_text, "v2.4.1", (ScTextStyle){
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
        });
        ScTreeNode *root = sc_tree_add_text(tree, NULL, root_text, NULL, plain);

        ScText *passed_text = sc_text_new();
        sc_text_append(passed_text, "build   ", (ScTextStyle){
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        });
        sc_text_append(passed_text, "PASSED", (ScTextStyle){
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        });

        ScText *failed_text = sc_text_new();
        sc_text_append(failed_text, "tests   ", (ScTextStyle){
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        });
        sc_text_append(failed_text, "FAILED", (ScTextStyle){
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
        });

        ScText *skipped_text = sc_text_new();
        sc_text_append(skipped_text, "deploy  ", (ScTextStyle){
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        });
        sc_text_append(skipped_text, "SKIPPED", (ScTextStyle){
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE
        });

        sc_tree_add_text(tree, root, passed_text, NULL, plain);
        sc_tree_add_text(tree, root, failed_text, NULL, plain);
        sc_tree_add_text(tree, root, skipped_text, NULL, plain);

        sc_tree_print(tree);
        sc_text_free(root_text);
        sc_text_free(passed_text);
        sc_text_free(failed_text);
        sc_text_free(skipped_text);
        sc_tree_free(tree);
    }

    printf("\n");

    /* ── 7. Tree next to table in columns ── */
    {
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type = SC_BORDER_SINGLE,
            .connector_color = sc_ansi_color_from_rgb(80, 140, 80),
        });
        ScTextStyle dir_style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        };
        ScTextStyle file_style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };
        ScTextStyle dir_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
        };
        ScTextStyle file_icon = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
        };

        ScTreeNode *app = sc_tree_add_str(
            tree, NULL, "app/", dir_style, "▸ ", dir_icon
        );
        ScTreeNode *lib = sc_tree_add_str(
            tree, NULL, "lib/", dir_style, "▸ ", dir_icon
        );
        ScTreeNode *components = sc_tree_add_str(
            tree, app, "components/", dir_style, "▸ ", dir_icon
        );
        sc_tree_add_str(tree, app, "main.ts", file_style, "· ", file_icon);
        sc_tree_add_str(
            tree, components, "Button.tsx", file_style, "· ", file_icon
        );
        sc_tree_add_str(
            tree, components, "Input.tsx", file_style, "· ", file_icon
        );
        sc_tree_add_str(tree, lib, "utils.ts", file_style, "· ", file_icon);
        sc_tree_add_str(tree, lib, "api.ts", file_style, "· ", file_icon);

        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "File", (ScColOpts){
            0, 0, 0, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(table, "Lines", (ScColOpts){
            0, 0, 0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(table, "Size", (ScColOpts){
            0, 0, 0, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Button.tsx"), sc_cell("142"), sc_cell("3.2 KB")
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("Input.tsx"), sc_cell("89"), sc_cell("1.8 KB")
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("main.ts"), sc_cell("34"), sc_cell("0.7 KB")
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("utils.ts"), sc_cell("210"), sc_cell("4.9 KB")
        }, 3);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("api.ts"), sc_cell("178"), sc_cell("3.8 KB")
        }, 3);

        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 3,
            .sep = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_tree(columns, tree, (ScColItem){0});
        sc_columns_add_table(columns, table, (ScTableOpts){
            .border = {
                SC_BORDER_SINGLE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                0, 0, 0
            },
            .header.row = true,
            .header.style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
            },
            .title = {
                .text = " File Stats ",
                .style = {
                    SC_TEXT_ATTR_BOLD,
                    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
                },
                .align = SC_ALIGN_CENTER,
                .pad = 1,
                .pos = SC_POSITION_TOP,
            },
            .cell_pad = {0, 1, 0, 1},
        }, (ScColItem){0});
        sc_columns_print(columns);

        sc_columns_free(columns);
        sc_tree_free(tree);
        sc_table_free(table);
    }
}
