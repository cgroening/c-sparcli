#include "test_input.h"
#include "sparcli.h"


static const char *FRUITS[] = {
    "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grape"
};

void test_select(void) {
    /* Single-select. */
    ScSelect *single = sc_select_new((ScSelectOpts){
        .prompt = "Pick a fruit", .accent = SC_ANSI_COLOR_MAGENTA });
    for (size_t i = 0; i < sizeof FRUITS / sizeof FRUITS[0]; i++) {
        sc_select_add(single, FRUITS[i]);
    }
    size_t idx[8];
    size_t n = 8;
    ScInputStatus st = sc_select_run(single, idx, &n);
    if (st == SC_INPUT_OK) { printf("  → single: index %zu (%s)\n", idx[0], FRUITS[idx[0]]); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → single: cancelled\n"); }
    else { printf("  → no TTY (skipped)\n"); }
    sc_select_free(single);

    /* Multi-select. */
    ScSelect *multi = sc_select_new((ScSelectOpts){
        .prompt = "Pick toppings (space to toggle)", .multi = true,
        .accent = SC_ANSI_COLOR_YELLOW });
    for (size_t i = 0; i < sizeof FRUITS / sizeof FRUITS[0]; i++) {
        sc_select_add(multi, FRUITS[i]);
    }
    sc_select_set_checked(multi, 0, true);  /* pre-check Apple */
    sc_select_set_checked(multi, 2, true);  /* pre-check Cherry */
    sc_select_set_cursor(multi, 2);         /* start the cursor on Cherry */
    n = 8;
    st = sc_select_run(multi, idx, &n);
    if (st == SC_INPUT_OK) {
        printf("  → multi: %zu chosen:", n);
        for (size_t i = 0; i < n; i++) { printf(" %s", FRUITS[idx[i]]); }
        printf("\n");
    } else if (st == SC_INPUT_CANCELLED) {
        printf("  → multi: cancelled\n");
    }
    sc_select_free(multi);
}
