#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_number_frame */

#include <stdlib.h>
#include <string.h>


/** Strips ANSI codes from `line` and checks it contains `needle`. */
static bool line_contains(const char *line, const char *needle) {
    char *plain = sc_strip_ansi(line);
    bool found = plain && strstr(plain, needle) != NULL;
    free(plain);
    return found;
}

/** Non-interactive: display formatting via the frame builder. */
void test_number_format(void) {
    /* Comma separator: value and range both render with ','. */
    ScRendered *frame = sc_number_frame(
        "Price", 12.99,
        (ScNumberOpts){ .min = 0, .max = 100, .step = 0.5, .decimals = 2,
                        .decimal_sep = ',' }
    );
    CHECK(frame != NULL && frame->line_count > 0, "comma frame renders");
    if (frame && frame->line_count > 0) {
        CHECK(line_contains(frame->lines[0], "12,99"),
              "decimal_sep ',': value shows 12,99");
        CHECK(line_contains(frame->lines[0], "[0,00-100,00]"),
              "decimal_sep ',': range shows [0,00-100,00]");
    }
    sc_rendered_free(frame);

    /* Default separator: value renders with '.'. */
    frame = sc_number_frame(
        "Price", 12.99, (ScNumberOpts){ .decimals = 2 }
    );
    CHECK(frame != NULL && frame->line_count > 0, "default frame renders");
    if (frame && frame->line_count > 0) {
        CHECK(line_contains(frame->lines[0], "12.99"),
              "default separator: value shows 12.99");
    }
    sc_rendered_free(frame);

    /* start_empty: the field renders empty instead of the formatted value. */
    frame = sc_number_frame(
        "Amount", 0,
        (ScNumberOpts){ .decimals = 2, .start_empty = true }
    );
    CHECK(frame != NULL && frame->line_count > 0, "start_empty frame renders");
    if (frame && frame->line_count > 0) {
        CHECK(!line_contains(frame->lines[0], "0.00"),
              "start_empty: no pre-filled 0.00 in the field");
        CHECK(line_contains(frame->lines[0], "Amount"),
              "start_empty: prompt still shown");
    }
    sc_rendered_free(frame);
}

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
