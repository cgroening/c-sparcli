#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef struct { char name[50]; void (*function)(void); int animated; } Test;


static Test *get_all_tests(size_t *count);
static Test *get_focused_tests(size_t *count);
static void run_tests(const Test *tests, size_t count, int skip_animated);
static void print_rule(const char *title);
void test_text_attributes(void);
void test_colors(void);
void test_columns_basic(void);
void test_panels(void);
void test_tables(void);
void test_columns(void);
void test_rules(void);
void test_trees(void);
void test_lists(void);
void test_progressbar(void);
void test_progressbar_animated(void);
void test_spinner(void);
void test_spinner_animated(void);
void test_kv(void);
void test_alert(void);
void test_badge(void);
void test_util(void);
void test_pad(void);
void test_align(void);
void test_markup(void);
void test_links(void);
void test_errors(void);


/**
 * Entry point. Parses flags and runs the selected test suite.
 *
 * Supported flags (pass via `ARGS=` when using `make`):
 * @code
 * make test                               # Run all tests
 * make test ARGS=--focus                  # Run only get_focused_tests
 * make test ARGS=--no-animated            # Run all tests, skip animated
 * make test ARGS="--focus --no-animated"  # Focused, skip animated
 * @endcode
 */
int main(int argc, char *argv[]) {
    int use_focus     = 0;
    int skip_animated = 0;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--focus")       == 0) { use_focus     = 1; }
        else if (strcmp(argv[i], "--no-animated") == 0) { skip_animated = 1; }
    }

    size_t count;
    Test *tests = use_focus ? get_focused_tests(&count) : get_all_tests(&count);
    run_tests(tests, count, skip_animated);

    return 0;
}

static Test *get_all_tests(size_t *count) {
    static Test tests[] = {
        { "Text Attributes & Combinations", &test_text_attributes, 0 },
        { "Colors",                    &test_colors,               0 },
        { "Panels",                    &test_panels,               0 },
        { "Alerts",                    &test_alert,                0 },
        { "Tables",                    &test_tables,               0 },
        { "Rules",                     &test_rules,                0 },
        { "Trees",                     &test_trees,                0 },
        { "Lists",                     &test_lists,                0 },
        { "Progress Bar",              &test_progressbar,          0 },
        { "Animated Progress Bar",     &test_progressbar_animated, 1 },
        { "Spinner",                   &test_spinner,              0 },
        { "Animated Spinner",          &test_spinner_animated,     1 },
        { "Key-Value Pairs",           &test_kv,                   0 },
        { "Badges",                    &test_badge,                0 },
        { "Utilities",                 &test_util,                 0 },
        { "Padding",                   &test_pad,                  0 },
        { "Alignment",                 &test_align,                0 },
        { "Markup",                    &test_markup,               0 },
        { "Hyperlinks",                &test_links,                0 },
        { "Pretty Errors",             &test_errors,               0 },
        { "Columns (Basic)",           &test_columns_basic,        0 },
        { "Columns (Advanced)",        &test_columns,              0 },
    };

    *count = sizeof(tests) / sizeof(tests[0]);
    return tests;
}

static Test *get_focused_tests(size_t *count) {
    static Test tests[] = {
        { "Text Attributes & Combinations", &test_text_attributes, 0 },
        { "Colors",                    &test_colors,               0 },
        // { "Panels",                    &test_panels,               0 },
        // { "Alerts",                    &test_alert,                0 },
        // { "Tables",                    &test_tables,               0 },
        // { "Rules",                     &test_rules,                0 },
        // { "Trees",                     &test_trees,                0 },
        // { "Lists",                     &test_lists,                0 },
        // { "Progress Bar",              &test_progressbar,          0 },
        // { "Animated Progress Bar",     &test_progressbar_animated, 1 },
        // { "Spinner",                   &test_spinner,              0 },
        // { "Animated Spinner",          &test_spinner_animated,     1 },
        // { "Key-Value Pairs",           &test_kv,                   0 },
        // { "Badges",                    &test_badge,                0 },
        // { "Utilities",                 &test_util,                 0 },
        // { "Padding",                   &test_pad,                  0 },
        // { "Alignment",                 &test_align,                0 },
        // { "Markup",                    &test_markup,               0 },
        // { "Hyperlinks",                &test_links,                0 },
        // { "Pretty Errors",             &test_errors,               0 },
        // { "Columns (Basic)",           &test_columns_basic,        0 },
        // { "Columns (Advanced)",        &test_columns,              0 },
    };

    *count = sizeof(tests) / sizeof(tests[0]);
    return tests;
}

/**
 * Runs the provided tests, optionally skipping animated ones.
 *
 * Prints a rule before and after each test for better visual separation.
 */
static void run_tests(const Test *tests, size_t count, int skip_animated) {
    printf("\n");
    for (size_t i = 0; i < count; i++) {
        if (skip_animated && tests[i].animated) { continue; }
        print_rule(tests[i].name);
        tests[i].function();
        print_rule(NULL);
        printf("\n");
    }
}

/**
 * Prints a styled rule with the given title. If title is `NULL`, prints a
 * simple line.
 */
static void print_rule(const char *title) {
    sc_rule_str(
        title,
        (ScRuleOpts) {
            .type = SC_BORDER_DOUBLE,
            .color       = SC_ANSI_COLOR_NONE,
            .title.style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
            },
            .title.halign = SC_ALIGN_CENTER,
        }
    );
}
