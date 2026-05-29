#include "test_input.h"
#include "sparcli.h"

#include <stdlib.h>
#include <string.h>


void test_textarea(void) {
    char *text = NULL;
    ScInputStatus st = sc_textarea(
        "Notes", &text,
        (ScTextareaOpts){ .placeholder = "Type multiple lines…",
                          .initial = "first line\nsecond line" }
    );
    if (st == SC_INPUT_OK) {
        printf("  → %zu bytes\n", strlen(text));
        free(text);
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }
}
