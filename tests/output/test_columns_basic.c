#include "sparcli.h"
#include <stdio.h>
#include <stddef.h>


/** One column for the basic column-layout demo. */
typedef struct {
    ScText *text;
    ScTextStyle style;
    ScHAlign halign;
    ScVAlign valign;
} ExampleColumn;

/** Group of columns shared by `print_example_group`. */
typedef struct {
    ScColumnsOpts opts;
    ExampleColumn *columns;
} ExampleColumnGroup;


static const ScTextStyle plain = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle green = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE
};
static const ScTextStyle red = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE
};
static const ScTextStyle magenta = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE
};


static ScText *make_sample_text(
    const char *prefix, ScTextStyle style, size_t line_count
);
static void print_example_group(
    const ExampleColumnGroup *group, size_t column_count
);


void test_columns_basic(void) {
    ExampleColumn col1 = {
        .text = make_sample_text("Column 1, Line ", plain, 3),
        .style = plain,
        .valign = SC_VALIGN_TOP,
    };
    ExampleColumn col2 = {
        .text = make_sample_text("Column 2 ", green, 1),
        .style = green,
        .valign = SC_VALIGN_TOP,
    };
    ExampleColumn col3 = {
        .text = make_sample_text("Column 3", red, 1),
        .style = red,
        .valign = SC_VALIGN_MIDDLE,
    };
    ExampleColumn col4 = {
        .text = make_sample_text("Column 4", magenta, 1),
        .style = magenta,
        .valign = SC_VALIGN_BOTTOM,
    };

    ExampleColumnGroup group = {
        .opts = {
            .sep = {
                .type = SC_BORDER_SINGLE,
                .color = SC_ANSI_COLOR_MAGENTA,
            },
        },
        .columns = (ExampleColumn[]){ col1, col2, col3, col4 },
    };
    print_example_group(&group, 4);

    printf("\n");

    /* ── 8. Per-column background colors ── */
    {
        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 2,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_str(
            columns, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(70, 20, 20),
            }
        );
        sc_columns_add_str(
            columns, "Column B\nLine 2\nLine 3",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(20, 60, 20),
            }
        );
        sc_columns_add_str(
            columns, "Column C\nLine 2",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(20, 20, 70),
            }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }

    printf("\n");

    /* ── 9. Per-column background colors with bordered panels ── */
    {
        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 1,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(
            columns, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
            (ScPanelOpts){
                .border = {
                    .type = SC_BORDER_ROUNDED,
                    .color = sc_color_from_rgb(255, 100, 100),
                    .bg = sc_color_from_rgb(100, 25, 25),
                },
                .bg = sc_color_from_rgb(60, 15, 15),
                .padding = { 0, 2, 0, 2 },
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){
                .stretch = true,
                .bg = sc_color_from_rgb(60, 15, 15),
            }
        );
        sc_columns_add_panel_str(
            columns, "Column B\nLine 2\nLine 3",
            (ScPanelOpts){
                .border = {
                    .type = SC_BORDER_ROUNDED,
                    .color = sc_color_from_rgb(100, 255, 100),
                    .bg = sc_color_from_rgb(25, 100, 25),
                },
                .bg = sc_color_from_rgb(15, 60, 15),
                .padding = { 0, 2, 0, 2 },
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){
                .stretch = true,
                .bg = sc_color_from_rgb(15, 60, 15),
            }
        );
        sc_columns_add_panel_str(
            columns, "Column C\nLine 2",
            (ScPanelOpts){
                .border = {
                    .type = SC_BORDER_ROUNDED,
                    .color = sc_color_from_rgb(100, 100, 255),
                    .bg = sc_color_from_rgb(25, 25, 100),
                },
                .bg = sc_color_from_rgb(15, 15, 60),
                .padding = { 0, 2, 0, 2 },
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){
                .stretch = true,
                .bg = sc_color_from_rgb(15, 15, 60),
            }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }

    printf("\n");

    /* ── 10. Per-column bg + separator with background color ── */
    {
        ScColumns *columns = sc_columns_new((ScColumnsOpts){
            .gap = 2,
            .valign = SC_VALIGN_TOP,
            .sep = {
                .type = SC_BORDER_SINGLE,
                .color = sc_color_from_rgb(200, 180, 100),
                .bg = sc_color_from_rgb(60, 50, 10),
            },
        });
        sc_columns_add_str(
            columns, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(70, 20, 20),
            }
        );
        sc_columns_add_str(
            columns, "Column B\nLine 2\nLine 3",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(20, 60, 20),
            }
        );
        sc_columns_add_str(
            columns, "Column C\nLine 2",
            (ScColItem){
                .fixed_w = 22,
                .halign = SC_ALIGN_CENTER,
                .bg = sc_color_from_rgb(20, 20, 70),
            }
        );
        sc_columns_print(columns);
        sc_columns_free(columns);
    }
}


/** Adds each column of `group` and prints the layout. */
static void print_example_group(
    const ExampleColumnGroup *group, size_t column_count
) {
    ScColumns *columns = sc_columns_new(group->opts);

    for (size_t i = 0; i < column_count; i++) {
        sc_columns_add_text(
            columns, group->columns[i].text,
            (ScColItem){
                .valign_set = true,
                .valign = group->columns[i].valign,
            }
        );
        sc_text_free(group->columns[i].text);
    }

    sc_columns_print(columns);
    sc_columns_free(columns);
}

/**
 * Builds a multi-line `ScText` from `line_count` copies of `prefix`.
 * When `line_count > 1`, each line is suffixed with its 1-based index;
 * otherwise the prefix is used verbatim.
 */
static ScText *make_sample_text(
    const char *prefix, ScTextStyle style, size_t line_count
) {
    ScText *text = sc_text_new();

    char buffer[128];
    for (size_t i = 1; i <= line_count; i++) {
        if (line_count > 1) {
            snprintf(buffer, sizeof(buffer), "%s%zu\n", prefix, i);
        } else {
            snprintf(buffer, sizeof(buffer), "%s\n", prefix);
        }
        sc_text_append(text, buffer, style);
    }
    return text;
}
