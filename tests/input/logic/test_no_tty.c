#include "test_input.h"
#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>


/** The environment override under test. */
static const char *const NO_TTY_ENV = "SPARCLI_NO_TTY";


// Forward declarations indented to reflect call hierarchy
static void check_disabled_reports_unavailable(void);
static void check_disabled_blocks_prompts(void);
static void check_disabled_makes_pager_noop(void);
static void check_non_disabling_values(void);


/**
 * SPARCLI_NO_TTY environment override: forces the no-TTY behavior so test
 * suites (e.g. the language-binding smoke tests) can run from an interactive
 * terminal without prompts grabbing the real keyboard or the pager spawning
 * a real `less`.
 */
void test_no_tty(void) {
    // Preserve the runner's value; getenv pointers may be invalidated by
    // later setenv calls, so take a copy.
    char saved[64] = "";
    bool had_value = false;
    const char *current = getenv(NO_TTY_ENV);
    if (current) {
        had_value = true;
        snprintf(saved, sizeof saved, "%s", current);
    }

    check_disabled_reports_unavailable();
    check_disabled_blocks_prompts();
    check_disabled_makes_pager_noop();
    check_non_disabling_values();

    if (had_value) {
        setenv(NO_TTY_ENV, saved, 1);
    } else {
        unsetenv(NO_TTY_ENV);
    }
}


/* ── Disabled: no terminal is reported or touched ────────────────────────── */

static void check_disabled_reports_unavailable(void) {
    setenv(NO_TTY_ENV, "1", 1);
    CHECK(!sc_input_available(),
          "no-tty: SPARCLI_NO_TTY=1 reports input as unavailable");
}

static void check_disabled_blocks_prompts(void) {
    setenv(NO_TTY_ENV, "1", 1);
    bool answer = false;
    ScInputStatus status = sc_confirm("Proceed?", &answer, (ScConfirmOpts){
        .hide_summary = true,
    });
    CHECK(status == SC_INPUT_ERROR,
          "no-tty: prompts fail cleanly instead of opening /dev/tty");
}

static void check_disabled_makes_pager_noop(void) {
    // PAGER points at a command that exits non-zero, so a pager process
    // accidentally spawned despite the override shows up in the exit status
    // (meaningful when this suite runs interactively on a real terminal).
    char saved_pager[128] = "";
    bool had_pager = false;
    const char *current_pager = getenv("PAGER");
    if (current_pager) {
        had_pager = true;
        snprintf(saved_pager, sizeof saved_pager, "%s", current_pager);
    }

    setenv(NO_TTY_ENV, "1", 1);
    setenv("PAGER", "false", 1);
    ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });
    int status = sc_pager_end(pager);
    CHECK(status == 0, "no-tty: pager session is a no-op (no child spawned)");

    if (had_pager) {
        setenv("PAGER", saved_pager, 1);
    } else {
        unsetenv("PAGER");
    }
}


/* ── Not disabled: "0" and empty keep the normal detection ──────────────── */

static void check_non_disabling_values(void) {
    // The expected result depends on the environment (true on a real TTY,
    // false when piped), so compare against the unset baseline.
    unsetenv(NO_TTY_ENV);
    bool baseline = sc_input_available();

    setenv(NO_TTY_ENV, "0", 1);
    CHECK(sc_input_available() == baseline,
          "no-tty: SPARCLI_NO_TTY=0 does not disable the terminal");

    setenv(NO_TTY_ENV, "", 1);
    CHECK(sc_input_available() == baseline,
          "no-tty: an empty SPARCLI_NO_TTY does not disable the terminal");
}
