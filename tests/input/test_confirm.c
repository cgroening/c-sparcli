#include "test_input.h"
#include "sparcli.h"


void test_confirm(void) {
    bool answer = false;
    ScInputStatus st = sc_confirm(
        "Deploy to production?", &answer,
        (ScConfirmOpts){ .default_yes = false, .accent = SC_ANSI_COLOR_GREEN }
    );

    if (st == SC_INPUT_OK) {
        printf("  → result: %s\n", answer ? "yes" : "no");
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }
}
