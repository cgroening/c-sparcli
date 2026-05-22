#include "sparcli.h"
#include <stdio.h>


typedef struct { char text[30]; ScTextStyle options; } Example;
typedef struct { Example *items; size_t n; } ExampleGroup ;


static Example *get_basic_examples(size_t *count);
static Example *get_combination_examples(size_t *count);
static void print_example_groups(const ExampleGroup *examples, size_t n);
static void print_example_row(ExampleGroup example);
static void print_example(const Example example, const int spaces_after);


static ScTextAttributeNs attr = ScTextAttributeNs_;
static ScAnsiColorNs clr = ScAnsiColorNs_;


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
    ScTextStyle none    = { attr.NONE, clr.NONE,    clr.NONE  };
    ScTextStyle black   = { attr.NONE, clr.BLACK,   clr.WHITE };
    ScTextStyle red     = { attr.NONE, clr.RED,     clr.NONE  };
    ScTextStyle green   = { attr.NONE, clr.GREEN,   clr.NONE  };
    ScTextStyle yellow  = { attr.NONE, clr.YELLOW,  clr.NONE  };
    ScTextStyle blue    = { attr.NONE, clr.BLUE,    clr.NONE  };
    ScTextStyle magenta = { attr.NONE, clr.MAGENTA, clr.NONE  };
    ScTextStyle cyan    = { attr.NONE, clr.CYAN,    clr.NONE  };
    ScTextStyle white   = { attr.NONE, clr.WHITE,   clr.BLACK };

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
    ScTextStyle blk_on_ylw = { attr.BOLD, clr.BLACK, clr.YELLOW };
    ScTextStyle orange_rgb = {
        attr.NONE, sc_ansi_color_from_rgb(255, 192, 0), clr.NONE
    };
    ScTextStyle rgb_bg = {
        attr.ITALIC, clr.WHITE, sc_ansi_color_from_rgb(80, 0, 120)
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
