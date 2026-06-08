#include "test_input.h"
#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>


/** The environment override that forces no-TTY behavior. */
static const char *const NO_TTY_ENV = "SPARCLI_NO_TTY";


// Forward declarations indented to reflect call hierarchy
static void check_no_tty_guard(void);
static void check_null_path(void);


/**
 * sc_edit_file: the public, tty-inheriting file editor. The interactive launch
 * needs a real terminal, so the headless logic suite only covers the guards
 * (no-TTY refusal and argument validation) - the happy path is exercised by the
 * PTY suite.
 */
void test_editor(void) {
    // Preserve the runner's value; getenv pointers may be invalidated by a
    // later setenv, so take a copy.
    char saved[64] = "";
    bool had_value = false;
    const char *current = getenv(NO_TTY_ENV);
    if (current) {
        had_value = true;
        snprintf(saved, sizeof saved, "%s", current);
    }

    check_no_tty_guard();
    check_null_path();

    if (had_value) {
        setenv(NO_TTY_ENV, saved, 1);
    } else {
        unsetenv(NO_TTY_ENV);
    }
}


static void check_no_tty_guard(void) {
    setenv(NO_TTY_ENV, "1", 1);
    int rc = sc_edit_file("true", "/tmp/sparcli-edit-test-should-not-open");
    CHECK(rc == -1,
          "edit_file: refuses to launch (-1) when no TTY is available");
}

static void check_null_path(void) {
    unsetenv(NO_TTY_ENV);
    CHECK(sc_edit_file("true", NULL) == -1,
          "edit_file: NULL path returns -1");
    CHECK(sc_edit_file("true", "") == -1,
          "edit_file: empty path returns -1");
}
