#include <stddef.h>
#include "sparcli.h"


typedef struct { char text[30]; ScOptions options; } Example;
typedef struct { Example *items; size_t n; } ExampleGroup ;


static void print_example_groups(const ExampleGroup *examples, size_t n);
static void print_example(const Example example, const int spaces_after);


void test_styles(void) {
    ScOptions plain       = { SC_STYLE_NONE,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions dim         = { SC_STYLE_DIM,    SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold        = { SC_STYLE_BOLD,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions italic      = { SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions underline   = { SC_STYLE_UNDER,  SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold_italic = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_under = {
        SC_STYLE_BOLD | SC_STYLE_UNDER, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_italic_under = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC | SC_STYLE_UNDER,
        SC_COLOR_NONE,
        SC_COLOR_NONE
    };

    Example examples_basic[] = {
        { "Plain text",             plain },
        { "Dim text",               dim },
        { "Bold text",              bold },
        { "Italic text",            italic },
        { "Underlined text",        underline },
    };

    Example examples_comb[] = {
        { "Bold + Italic",          bold_italic },
        { "Bold + Underlined",      bold_under },
        { "Bold + Italic + Under",  bold_italic_under }
    };

    ExampleGroup examples[] = {
        { examples_basic, sizeof(examples_basic) / sizeof(examples_basic[0]) },
        { examples_comb, sizeof(examples_comb) / sizeof(examples_comb[0]) }
    };

    print_example_groups(examples, sizeof(examples) / sizeof(examples[0]));

}


static void print_example_groups(const ExampleGroup *examples, size_t n) {
    int spaces_after = 0;
    for(size_t i = 0; i < n; i++) {
        for(size_t j = 0; j < examples[i].n; j++) {
            if(j+1 < examples[i].n) {
                spaces_after = 4;
            } else {
                spaces_after = 0;
            }
            print_example(examples[i].items[j], spaces_after);
        }

        printf("\n");
    }
}


static void print_example(const Example example, const int spaces_after) {
    sc_print(example.text, example.options);
    for(int i = 0; i < spaces_after; i++) { printf(" "); }
}
