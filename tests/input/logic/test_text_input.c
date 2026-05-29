#include "test_input.h"
#include "sparcli.h"

#include <stdlib.h>
#include <string.h>


/** Rejects empty input as a demonstration of the validator hook. */
static bool not_empty(const char *value, void *ctx, const char **err) {
    (void)ctx;
    if (value[0] == '\0') { *err = "Please enter a value"; return false; }
    return true;
}

void test_text_input(void) {
    char *name = NULL;
    ScInputStatus st = sc_text_input(
        "Your name:", &name,
        (ScTextInputOpts){
            .placeholder = "type here…",
            .validate    = not_empty,
            .value_style = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_CYAN,
                             SC_ANSI_COLOR_NONE },
        }
    );

    if (st == SC_INPUT_OK) {
        printf("  → got: \"%s\" (%zu bytes)\n", name, strlen(name));
        free(name);
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }

    /* Capped input: the counter shows count/max and typing stops at the cap. */
    char *code = NULL;
    st = sc_text_input(
        "Coupon (max 12):", &code,
        (ScTextInputOpts){ .placeholder = "ABC-123", .max_chars = 12 }
    );
    if (st == SC_INPUT_OK) {
        printf("  → got: \"%s\"\n", code);
        free(code);
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }
}
