#include "test_input.h"
#include "sparcli.h"


void test_number(void) {
    double v = 0;
    ScInputStatus st = sc_number_input(
        "Quantity", &v,
        (ScNumberOpts){ .initial = 10, .min = 0, .max = 100, .step = 5 }
    );
    if (st == SC_INPUT_OK)             { printf("  → %g\n", v); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → cancelled\n"); }
    else                               { printf("  → no TTY (skipped)\n"); }

    /* Decimal variant. */
    double price = 0;
    st = sc_number_input(
        "Price", &price,
        (ScNumberOpts){ .initial = 9.99, .min = 0, .max = 1000,
                        .step = 0.5, .decimals = 2 }
    );
    if (st == SC_INPUT_OK)             { printf("  → %.2f\n", price); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → cancelled\n"); }
    else                               { printf("  → no TTY (skipped)\n"); }
}
