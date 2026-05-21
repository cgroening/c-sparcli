#include "sparcli.h"
#include <stdio.h>
#include <stddef.h>


typedef struct {
    ScText *text;
    ScOptions opts;
    ScAlign h_align;
    ScValign v_align;
} ExampleColumn;

typedef struct {
    ScColumnsOpts opts;
    ExampleColumn *columns;

} ExampleColumnGroup;


ScOptions plain   = { SC_STYLE_NONE, SC_COLOR_NONE,    SC_COLOR_NONE };
ScOptions green   = { SC_STYLE_NONE, SC_COLOR_GREEN,   SC_COLOR_NONE };
ScOptions red     = { SC_STYLE_NONE, SC_COLOR_RED,     SC_COLOR_NONE };
ScOptions magenta = { SC_STYLE_NONE, SC_COLOR_MAGENTA, SC_COLOR_NONE };


ScText *get_sample_text(const char *prefix, ScOptions opts, size_t lines);
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
        .sep_color = SC_COLOR_MAGENTA,
        },
        .columns = (ExampleColumn[]){ col1, col2, col3, col4 },
    };



    print_example_group(&group, 4);

    printf("\n");
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


ScText *get_sample_text(const char *prefix, ScOptions opts, size_t lines) {
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
