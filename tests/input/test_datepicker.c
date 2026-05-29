#include "test_input.h"
#include "sparcli.h"

#include <time.h>


void test_datepicker(void) {
    struct tm picked = { 0 };  /* zeroed → seeds today */
    ScInputStatus st = sc_datepicker(
        &picked,
        (ScDatePickerOpts){ .prompt = "Pick a date", .week_start = 1,
                            .accent = SC_ANSI_COLOR_BLUE }
    );

    if (st == SC_INPUT_OK) {
        char buf[64];
        strftime(buf, sizeof buf, "%A, %d %B %Y", &picked);
        printf("  → picked: %s\n", buf);
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → cancelled\n");
    } else {
        printf("  → no TTY (skipped)\n");
    }
}
