#include <stddef.h>
#include "sparcli.h"


typedef struct { char text[30]; ScOptions options; } Example;
typedef struct { Example *items; size_t n; } ExampleGroup;


static Example *get_basic_examples(size_t *count);
static Example *get_combination_examples(size_t *count);
static void print_example_groups(const ExampleGroup *examples, size_t n);
static void print_example_row(ExampleGroup example);
static void print_example(const Example example, const int spaces_after);


void test_styles(void) {
    size_t basic_examples_count;
    Example *basic_examples = get_basic_examples(&basic_examples_count);

    size_t comb_examples_count;
    Example *comb_examples = get_combination_examples(&comb_examples_count);

    ExampleGroup examples[] = {
        { basic_examples, basic_examples_count },
        { comb_examples, comb_examples_count },
    };

    print_example_groups(examples, sizeof(examples) / sizeof(examples[0]));
}

static Example *get_basic_examples(size_t *count) {
    ScOptions plain       = { SC_STYLE_NONE,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions dim         = { SC_STYLE_DIM,    SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold        = { SC_STYLE_BOLD,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions italic      = { SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions underline   = { SC_STYLE_UNDER,  SC_COLOR_NONE, SC_COLOR_NONE };

    static Example examples[5];
    examples[0] = (Example){ "Plain text", plain };
    examples[1] = (Example){ "Dim text", dim };
    examples[2] = (Example){ "Bold text", bold };
    examples[3] = (Example){ "Italic text", italic };
    examples[4] = (Example){ "Underlined text", underline };

    *count = sizeof(examples) / sizeof(examples[0]);
    return examples;
}

static Example *get_combination_examples(size_t *count) {
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

    static Example examples[3];
    examples[0] = (Example){ "Bold + Italic", bold_italic };
    examples[1] = (Example){ "Bold + Underlined", bold_under };
    examples[2] = (Example){ "Bold + Italic + Under", bold_italic_under };

    *count = sizeof(examples) / sizeof(examples[0]);
    return examples;
}

static void print_example_groups(const ExampleGroup *examples, size_t n) {
    for(size_t i = 0; i < n; i++) {
        print_example_row(examples[i]);
    }
}

static void print_example_row(ExampleGroup example) {
    int spaces_after = 0;

    for(size_t i = 0; i < example.n; i++) {
        if(i+1 < example.n) {
            spaces_after = 4;
        } else {
            spaces_after = 0;
        }
        print_example(example.items[i], spaces_after);
    }

    printf("\n");
}

static void print_example(const Example example, const int spaces_after) {
    sc_print(example.text, example.options);
    for(int i = 0; i < spaces_after; i++) {
        printf(" ");
    }
}
