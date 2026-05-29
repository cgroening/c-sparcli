#include "test_input.h"
#include "sparcli.h"

#include <string.h>


int g_input_failures = 0;


typedef struct {
    char  name[40];
    void (*function)(void);
    int   interactive;  /* 1 = needs a real TTY */
} Test;


static void print_rule(const char *title) {
    sc_rule_str(title, (ScRuleOpts){
        .type        = SC_BORDER_DOUBLE,
        .title.style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
        .title.align = SC_ALIGN_CENTER,
    });
}

/**
 * Input-widget test runner.
 *
 * @code
 * make test-input                 # all tests (interactive; run in a terminal)
 * make test-input ARGS=--logic    # only the non-interactive logic tests (CI)
 * @endcode
 *
 * Returns non-zero when any logic CHECK failed, so CI can gate on it.
 */
int main(int argc, char *argv[]) {
    int logic_only = 0;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--logic") == 0) { logic_only = 1; }
    }

    static Test tests[] = {
        { "Key Decoder",    test_key_decode,     0 },
        { "Line Editor",    test_line_editor,    0 },
        { "Char Filters",   test_filters,        0 },
        { "Confirm",        test_confirm,        1 },
        { "Text Input",     test_text_input,     1 },
        { "Password Input", test_password_input, 1 },
        { "Number Input",   test_number,         1 },
        { "Textarea",       test_textarea,       1 },
        { "Select",         test_select,         1 },
        { "Fuzzy Finder",   test_fuzzy,          1 },
        { "Date Picker",    test_datepicker,     1 },
        { "Theme",          test_theme,          1 },
    };
    size_t count = sizeof tests / sizeof tests[0];

    printf("\n");
    for (size_t i = 0; i < count; i++) {
        if (logic_only && tests[i].interactive) { continue; }
        print_rule(tests[i].name);
        tests[i].function();
        print_rule(NULL);
        printf("\n");
    }

    if (g_input_failures > 0) {
        printf("\033[31m%d logic check(s) failed.\033[0m\n", g_input_failures);
        return 1;
    }
    printf("\033[32mAll logic checks passed.\033[0m\n");
    return 0;
}
