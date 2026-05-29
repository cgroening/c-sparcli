#include "test_input.h"
#include "sparcli.h"

#include <stdlib.h>
#include <string.h>


void test_password_input(void) {
    char *secret = NULL;
    ScInputStatus st = sc_password_input(
        "Password:", &secret,
        (ScPasswordOpts){ .placeholder = "••••••" }
    );

    if (st == SC_INPUT_OK) {
        printf("  → captured %zu bytes (not echoed)\n", strlen(secret));
        free(secret);
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }
}
