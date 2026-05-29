#include "test_input.h"
#include "sparcli.h"


/* Demonstrates a global theme: widgets inherit accent/styles with zero-init opts. */
void test_theme(void) {
    sc_input_set_theme(&(ScInputTheme){
        .accent       = SC_ANSI_COLOR_MAGENTA,
        .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE },
        .cursor_marker = "\xe2\x86\x92 ",  /* → */
        .hint_style   = { SC_TEXT_ATTR_DIM | SC_TEXT_ATTR_ITALIC,
                          SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE },
    });

    bool ok = false;
    ScInputStatus st = sc_confirm("Themed confirm?", &ok, (ScConfirmOpts){ 0 });
    if (st == SC_INPUT_ERROR) { printf("  → no TTY (skipped)\n"); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → cancelled\n"); }
    else { printf("  → %s\n", ok ? "yes" : "no"); }

    ScSelect *s = sc_select_new((ScSelectOpts){ .prompt = "Themed select" });
    sc_select_add(s, "Alpha");
    sc_select_add(s, "Beta");
    sc_select_add(s, "Gamma");
    size_t idx[3]; size_t n = 3;
    st = sc_select_run(s, idx, &n);
    if (st == SC_INPUT_OK) { printf("  → %s\n", "selected"); }
    sc_select_free(s);

    sc_input_set_theme(NULL);  /* reset for the rest of the suite */
}
