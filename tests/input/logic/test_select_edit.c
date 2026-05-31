#include "test_input.h"
#include "sparcli.h"

#include <string.h>


/**
 * Non-interactive checks for the in-place label edit APIs (no TTY needed):
 * sc_select_label / sc_select_set_label, including out-of-range no-ops.
 */
void test_select_edit(void) {
    ScSelect *s = sc_select_new((ScSelectOpts){ 0 });
    sc_select_add(s, "Apple");
    sc_select_add(s, "Banana");

    const char *l0 = sc_select_label(s, 0);
    CHECK(l0 && strcmp(l0, "Apple") == 0, "label() returns the initial text");

    sc_select_set_label(s, 1, "Blueberry");
    const char *l1 = sc_select_label(s, 1);
    CHECK(l1 && strcmp(l1, "Blueberry") == 0, "set_label() updates the text");

    sc_select_set_label(s, 9, "ignored");   /* out of range → no-op */
    CHECK(sc_select_label(s, 9) == NULL, "label() out of range is NULL");
    const char *still = sc_select_label(s, 0);
    CHECK(still && strcmp(still, "Apple") == 0,
          "out-of-range set_label() leaves others intact");

    sc_select_free(s);
}
