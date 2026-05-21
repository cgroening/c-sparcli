#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


typedef struct { char name[30]; void (*function)(void); } Test;


static Test *get_tests(size_t *count);
static void run_tests(const Test *tests, size_t count);
static void print_rule(const char *title);
void test_styles(void);
void test_colors(void);
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


int main(void) {
    size_t test_count;
    Test *tests = get_tests(&test_count);
    run_tests(tests, test_count);

    return 0;
}

static Test *get_tests(size_t *count) {
    static Test tests[] = {
        { "Basic Styles & Combinations", &test_styles },
        { "Colors", &test_colors },
        // { "Panels", &test_panels },
        // { "Tables", &test_tables },
        // { "Columns", &test_columns },
        // { "Rules", &test_rules },
        // { "Trees", &test_trees },
        // { "Lists", &test_lists },
        // { "Progress Bar", &test_progressbar },
        // { "Animated Progress Bar", &test_progressbar_animated },
        // { "Spinner", &test_spinner },
        // { "Animated Spinner", &test_spinner_animated },
        // { "Key-Value Pairs", &test_kv },
        // { "Alerts", &test_alert },
        // { "Badges", &test_badge },
        // { "Utilities", &test_util },
        // { "Padding", &test_pad },
        // { "Alignment", &test_align },
        // { "Markup", &test_markup },
    };

    *count = sizeof(tests) / sizeof(tests[0]);
    return tests;
}

static void run_tests(const Test *tests, size_t count) {
    printf("\n");
    for(size_t i = 0; i < count; i++) {
        print_rule(tests[i].name);
        tests[i].function();
        print_rule(NULL);
        printf("\n");
    }
}

static void print_rule(const char *title) {
    sc_rule_str(
        title,
        (ScRuleOpts) {
            .style       = SC_BORDER_DOUBLE,
            .color       = SC_COLOR_NONE,
            .title_opts  = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE },
            .title_align = SC_ALIGN_CENTER,
        }
    );
}
