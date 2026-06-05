#include "test_serde.h"


int g_serde_failures = 0;


typedef struct Test {
    char name[40];
    void (*function)(void);
} Test;


/** Prints a separator rule with an optional bold title. */
static void print_rule(const char *title) {
    sc_rule_str(
        title,
        (ScRuleOpts){
            .type = SC_BORDER_DOUBLE,
            .color = SC_ANSI_COLOR_NONE,
            .title.style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
            },
            .title.halign = SC_ALIGN_CENTER,
        }
    );
}

int main(void) {
    static const Test tests[] = {
        { "Value model", test_serde_value },
        { "JSON read/write", test_serde_json },
        { "CSV read/write", test_serde_csv },
    };
    size_t count = sizeof tests / sizeof tests[0];

    printf("\n");
    for (size_t i = 0; i < count; i++) {
        print_rule(tests[i].name);
        tests[i].function();
        print_rule(NULL);
        printf("\n");
    }

    if (g_serde_failures > 0) {
        printf("\033[31m%d serde check(s) failed.\033[0m\n", g_serde_failures);
        return 1;
    }
    printf("\033[32mAll serde checks passed.\033[0m\n");
    return 0;
}
