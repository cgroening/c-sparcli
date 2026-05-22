#include "sparcli.h"
#include <stdio.h>


typedef struct { char text[30]; ScTextStyle options; } Example;
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
    ScTextStyle none    = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE,    SC_ANSI_COLOR_NONE  };
    ScTextStyle black   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,   SC_ANSI_COLOR_WHITE };
    ScTextStyle red     = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,     SC_ANSI_COLOR_NONE  };
    ScTextStyle green   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,   SC_ANSI_COLOR_NONE  };
    ScTextStyle yellow  = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_YELLOW,  SC_ANSI_COLOR_NONE  };
    ScTextStyle blue    = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLUE,    SC_ANSI_COLOR_NONE  };
    ScTextStyle magenta = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE  };
    ScTextStyle cyan    = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN,    SC_ANSI_COLOR_NONE  };
    ScTextStyle white   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_WHITE,   SC_ANSI_COLOR_BLACK };

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
    ScTextStyle blk_on_ylw = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_YELLOW };
    ScTextStyle orange_rgb = {
        SC_TEXT_ATTR_NONE, sc_ansi_color_from_rgb(255, 192, 0), SC_ANSI_COLOR_NONE
    };
    ScTextStyle rgb_bg = {
        SC_TEXT_ATTR_ITALIC, SC_ANSI_COLOR_WHITE, sc_ansi_color_from_rgb(80, 0, 120)
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
