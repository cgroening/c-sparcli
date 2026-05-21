#include "sparcli.h"
#include <stdio.h>


typedef struct { char text[30]; ScOptions options; } Example;
typedef struct { Example *items; size_t n; } ExampleGroup ;


static Example *get_basic_examples(size_t *count);
static Example *get_combination_examples(size_t *count);
static void print_example_groups(const ExampleGroup *examples, size_t n);
static void print_example_row(ExampleGroup example);
static void print_example(const Example example, const int spaces_after);


void test_colors(void) {
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
    ScOptions none    = { SC_STYLE_NONE, SC_COLOR_NONE,    SC_COLOR_NONE  };
    ScOptions black   = { SC_STYLE_NONE, SC_COLOR_BLACK,   SC_COLOR_WHITE };
    ScOptions red     = { SC_STYLE_NONE, SC_COLOR_RED,     SC_COLOR_NONE  };
    ScOptions green   = { SC_STYLE_NONE, SC_COLOR_GREEN,   SC_COLOR_NONE  };
    ScOptions yellow  = { SC_STYLE_NONE, SC_COLOR_YELLOW,  SC_COLOR_NONE  };
    ScOptions blue    = { SC_STYLE_NONE, SC_COLOR_BLUE,    SC_COLOR_NONE  };
    ScOptions magenta = { SC_STYLE_NONE, SC_COLOR_MAGENTA, SC_COLOR_NONE  };
    ScOptions cyan    = { SC_STYLE_NONE, SC_COLOR_CYAN,    SC_COLOR_NONE  };
    ScOptions white   = { SC_STYLE_NONE, SC_COLOR_WHITE,   SC_COLOR_BLACK };

    static Example examples[9];
    examples[0] = (Example){ "Default", none    };
    examples[1] = (Example){ "Black",   black   };
    examples[2] = (Example){ "Red",     red     };
    examples[3] = (Example){ "Green",   green   };
    examples[4] = (Example){ "Yellow",  yellow  };
    examples[5] = (Example){ "Blue",    blue    };
    examples[6] = (Example){ "Magenta", magenta };
    examples[7] = (Example){ "Cyan",    cyan    };
    examples[8] = (Example){ "White",   white   };

    *count = sizeof(examples) / sizeof(examples[0]);
    return examples;
}

static Example *get_combination_examples(size_t *count) {
    ScOptions blk_on_ylw = { SC_STYLE_BOLD, SC_COLOR_BLACK, SC_COLOR_YELLOW };
    ScOptions orange_rgb = {
        SC_STYLE_NONE, sc_color_from_rgb(255, 192, 0), SC_COLOR_NONE
    };
    ScOptions rgb_bg = {
        SC_STYLE_ITALIC, SC_COLOR_WHITE, sc_color_from_rgb(80, 0, 120)
    };

    static Example examples[3];
    examples[0] = (Example){ "Black on yellow",        blk_on_ylw };
    examples[1] = (Example){ "RGB orange text",        orange_rgb };
    examples[2] = (Example){ "Italic white on purple", rgb_bg     };

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
            spaces_after = 2;
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
