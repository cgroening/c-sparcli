#include <stddef.h>
#include "sparcli.h"


typedef struct { char text[30]; ScTextStyle options; } Example;
typedef struct { Example *items; size_t n; } ExampleGroup;


static Example *get_basic_examples(size_t *count);
static Example *get_combination_examples(size_t *count);
static void print_example_groups(const ExampleGroup *examples, size_t n);
static void print_example_row(ExampleGroup example);
static void print_example(const Example example, const int spaces_after);


/*
 * `ScTextAttributeNs_` and `ScAnsiColorNs_` are now `extern const`, so we
 * use pointer aliases at file scope and dereference inside expressions.
 */
static const ScTextAttributeNs *attr_ns = &ScTextAttributeNs_;
static const ScAnsiColorNs *clr_ns = &ScAnsiColorNs_;
#define attr (*attr_ns)
#define clr (*clr_ns)


/**
 * Prints all text attribute examples: basic attributes and combinations.
 */
void test_text_attributes(void) {
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

/**
 * Returns the five basic attribute examples: plain, dim, bold, italic
 * and underline.
 */
static Example *get_basic_examples(size_t *count) {
    ScTextStyle plain     = { attr.BOLD,   clr.NONE, clr.NONE };
    ScTextStyle dim       = { attr.DIM,    clr.NONE, clr.NONE };
    ScTextStyle bold      = { attr.BOLD,   clr.NONE, clr.NONE };
    ScTextStyle italic    = { attr.ITALIC, clr.NONE, clr.NONE };
    ScTextStyle underline = { attr.UNDER,  clr.NONE, clr.NONE };

    static Example examples[5];
    examples[0] = (Example){ "Plain text", plain };
    examples[1] = (Example){ "Dim text", dim };
    examples[2] = (Example){ "Bold text", bold };
    examples[3] = (Example){ "Italic text", italic };
    examples[4] = (Example){ "Underlined text", underline };

    *count = sizeof(examples) / sizeof(examples[0]);
    return examples;
}

/**
 * Returns examples combining multiple attributes: bold+italic, bold+underline,
 * and bold+italic+underline.
 */
static Example *get_combination_examples(size_t *count) {
    ScTextStyle bold_italic = { attr.BOLD | attr.ITALIC, clr.NONE, clr.NONE };
    ScTextStyle bold_under = { attr.BOLD | attr.UNDER, clr.NONE, clr.NONE };
    ScTextStyle bold_italic_under = {
        attr.BOLD | attr.ITALIC | attr.UNDER, clr.NONE, clr.NONE
    };

    static Example examples[3];
    examples[0] = (Example){ "Bold + Italic", bold_italic };
    examples[1] = (Example){ "Bold + Underlined", bold_under };
    examples[2] = (Example){ "Bold + Italic + Under", bold_italic_under };

    *count = sizeof(examples) / sizeof(examples[0]);
    return examples;
}

/**
 * Prints each example group as a row.
 */
static void print_example_groups(const ExampleGroup *examples, size_t n) {
    for(size_t i = 0; i < n; i++) {
        print_example_row(examples[i]);
    }
}

/**
 * Prints all examples in a group on one line, separated by 4 spaces.
 */
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

/**
 * Prints a single styled example followed by spaces_after spaces.
 */
static void print_example(const Example example, const int spaces_after) {
    sc_print(example.text, example.options);
    for(int i = 0; i < spaces_after; i++) {
        printf(" ");
    }
}
