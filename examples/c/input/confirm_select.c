/*
 * confirm_select.c - yes/no confirmation and single/multi selection lists.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/confirm_select
 *
 * Esc or Ctrl-C cancels the current prompt; the demo then moves on.
 */

#include <sparcli.h>

#include <stdio.h>


static const char *const LANGUAGES[] = {
    "C", "C++", "Rust", "Python", "Zig", "Go",
};
#define LANGUAGE_COUNT (sizeof LANGUAGES / sizeof LANGUAGES[0])


static void run_confirm(void);
static void run_single_select(void);
static void run_multi_select(void);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    run_confirm();
    run_single_select();
    run_multi_select();
    return 0;
}

/** Yes/no question with custom labels and a default answer. */
static void run_confirm(void) {
    bool deploy = false;
    ScInputStatus status = sc_confirm("Deploy to production?", &deploy,
        (ScConfirmOpts){
            .default_yes = false,
            .yes_label   = "Ship it",
            .no_label    = "Not yet",
            .accent      = SC_ANSI_COLOR_GREEN,
        });

    if (status == SC_INPUT_OK) {
        sc_markup_println(deploy ? "  [green]-> deploying[/]"
                                 : "  [yellow]-> skipped[/]");
    } else {
        sc_markup_println("  [dim]-> cancelled[/]");
    }
}

/** Single selection: arrow keys / j/k move, Enter picks. */
static void run_single_select(void) {
    ScSelect *select = sc_select_new((ScSelectOpts){
        .prompt = "Primary language",
        .accent = SC_ANSI_COLOR_CYAN,
    });
    for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
        sc_select_add(select, LANGUAGES[i]);
    }

    size_t picked  = 0;
    size_t count   = 1;
    ScInputStatus status = sc_select_run(select, &picked, &count);
    if (status == SC_INPUT_OK && count > 0) {
        printf("  -> %s\n", LANGUAGES[picked]);
    }
    sc_select_free(select);
}

/** Multi selection: Space toggles checkboxes, Enter submits. */
static void run_multi_select(void) {
    ScSelect *select = sc_select_new((ScSelectOpts){
        .prompt      = "Languages you use (Space toggles)",
        .multi       = true,
        .max_visible = 5,    // longer lists scroll inside a viewport
        .accent      = SC_ANSI_COLOR_MAGENTA,
    });
    for (size_t i = 0; i < LANGUAGE_COUNT; i++) {
        sc_select_add(select, LANGUAGES[i]);
    }

    // Pre-check the first entry and start the cursor on the second one.
    sc_select_set_checked(select, 0, true);
    sc_select_set_cursor(select, 1);

    size_t picked[LANGUAGE_COUNT] = { 0 };
    size_t count = LANGUAGE_COUNT;
    ScInputStatus status = sc_select_run(select, picked, &count);
    if (status == SC_INPUT_OK) {
        printf("  -> %zu selected:", count);
        for (size_t i = 0; i < count; i++) {
            printf(" %s", LANGUAGES[picked[i]]);
        }
        printf("\n");
    }
    sc_select_free(select);
}
