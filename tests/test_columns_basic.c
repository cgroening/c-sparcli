#include "sparcli.h"
#include <stdio.h>
#include <stddef.h>


typedef struct {
    ScText *text;
    ScTextStyle opts;
    ScHAlign h_align;
    ScVAlign v_align;
} ExampleColumn;

typedef struct {
    ScColumnsOpts opts;
    ExampleColumn *columns;

} ExampleColumnGroup;


ScTextStyle plain   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,    SC_ANSI_COLOR_NONE };
ScTextStyle green   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,   SC_ANSI_COLOR_NONE };
ScTextStyle red     = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,     SC_ANSI_COLOR_NONE };
ScTextStyle magenta = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE };


ScText *get_sample_text(const char *prefix, ScTextStyle opts, size_t lines);
void print_example_group(const ExampleColumnGroup *group, size_t column_count);


void test_columns_basic(void) {

    ExampleColumn col1 = {
        .text    = get_sample_text("Column 1, Line ", plain, 3),
        .opts    = plain,
        // .h_align = SC_ALIGN_LEFT,
        .v_align = SC_VALIGN_TOP,
    };

    ExampleColumn col2 = {
        .text    = get_sample_text("Column 2 ", green, 1),
        .opts    = green,
        // .h_align = SC_ALIGN_LEFT,
        .v_align = SC_VALIGN_TOP,
    };

    ExampleColumn col3 = {
        .text    = get_sample_text("Column 3", red, 1),
        .opts    = red,
        // .h_align = SC_ALIGN_LEFT,
        .v_align = SC_VALIGN_MIDDLE,
    };

    ExampleColumn col4 = {
        .text    = get_sample_text("Column 4", magenta, 1),
        .opts    = magenta,
        // .h_align = SC_ALIGN_LEFT,
        .v_align = SC_VALIGN_BOTTOM,
    };

    ExampleColumnGroup group = {
        .opts = {
        .sep_style = SC_BORDER_SINGLE,
        .sep_color = SC_ANSI_COLOR_MAGENTA,
        },
        .columns = (ExampleColumn[]){ col1, col2, col3, col4 },
    };



    print_example_group(&group, 4);

    printf("\n");



    /* ── 8. Per-column background colors ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap    = 2,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_str(cl, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(70, 20, 20) });
        sc_columns_add_str(cl, "Column B\nLine 2\nLine 3",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(20, 60, 20) });
        sc_columns_add_str(cl, "Column C\nLine 2",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(20, 20, 70) });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 9. Per-column background colors with bordered panels ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap    = 1,
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_panel_str(cl, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
            (ScPanelOpts){
                .border        = SC_BORDER_ROUNDED,
                .border_color  = sc_ansi_color_from_rgb(255, 100, 100),
                .border_bg     = sc_ansi_color_from_rgb(100, 25, 25),
                .bg            = sc_ansi_color_from_rgb(60, 15, 15),
                .padding = {0, 2, 0, 2},
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){ .stretch = 1, .bg = sc_ansi_color_from_rgb(60, 15, 15) });
        sc_columns_add_panel_str(cl, "Column B\nLine 2\nLine 3",
            (ScPanelOpts){
                .border        = SC_BORDER_ROUNDED,
                .border_color  = sc_ansi_color_from_rgb(100, 255, 100),
                .border_bg     = sc_ansi_color_from_rgb(25, 100, 25),
                .bg            = sc_ansi_color_from_rgb(15, 60, 15),
                .padding = {0, 2, 0, 2},
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){ .stretch = 1, .bg = sc_ansi_color_from_rgb(15, 60, 15) });
        sc_columns_add_panel_str(cl, "Column C\nLine 2",
            (ScPanelOpts){
                .border        = SC_BORDER_ROUNDED,
                .border_color  = sc_ansi_color_from_rgb(100, 100, 255),
                .border_bg     = sc_ansi_color_from_rgb(25, 25, 100),
                .bg            = sc_ansi_color_from_rgb(15, 15, 60),
                .padding = {0, 2, 0, 2},
                .content_align = SC_ALIGN_CENTER,
            },
            (ScColItem){ .stretch = 1, .bg = sc_ansi_color_from_rgb(15, 15, 60) });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }

    printf("\n");

    /* ── 10. Per-column bg + separator with background color ── */
    {
        ScColumns *cl = sc_columns_new((ScColumnsOpts){
            .gap       = 2,
            .valign    = SC_VALIGN_TOP,
            .sep_style = SC_BORDER_SINGLE,
            .sep_color = sc_ansi_color_from_rgb(200, 180, 100),
            .sep_bg    = sc_ansi_color_from_rgb(60, 50, 10),
        });
        sc_columns_add_str(cl, "Column A\nLine 2\nLine 3\nLine 4\nLine 5",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(70, 20, 20) });
        sc_columns_add_str(cl, "Column B\nLine 2\nLine 3",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(20, 60, 20) });
        sc_columns_add_str(cl, "Column C\nLine 2",
                           (ScColItem){ .fixed_w = 22, .align = SC_ALIGN_CENTER,
                                        .bg = sc_ansi_color_from_rgb(20, 20, 70) });
        sc_columns_print(cl);
        sc_columns_free(cl);
    }
}


void print_example_group(const ExampleColumnGroup *group, size_t column_count) {
    ScColumns *cols = sc_columns_new(group->opts);

    for(size_t i = 0; i < column_count; i++) {
        sc_columns_add_text(cols, group->columns[i].text, (ScColItem){ .valign_set = 1, .valign = group->columns[i].v_align });

        sc_text_free(group->columns[i].text);
    }

        sc_columns_print(cols);
        sc_columns_free(cols);
}


ScText *get_sample_text(const char *prefix, ScTextStyle opts, size_t lines) {
    ScText *t = sc_text_new();

    char buf[128];
    for (size_t i = 1; i <= lines; i++) {
        if (lines > 1)
            snprintf(buf, sizeof(buf), "%s%zu\n", prefix, i);
        else
            snprintf(buf, sizeof(buf), "%s\n", prefix);
        sc_text_append(t, buf, opts);
    }

    return t;
}
