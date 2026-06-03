#pragma once

#include <stdio.h>

/**
 * Shared scaffolding for the input-widget test suite.
 *
 * Pure-logic tests (`test_key_decode`, `test_line_editor`) assert with
 * `CHECK` and run anywhere. The widget tests are interactive and need a
 * real TTY; under a pipe they return immediately with SC_INPUT_ERROR.
 */

/** Running count of failed CHECKs; the runner uses it as the exit status. */
extern int g_input_failures;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            printf("  \033[32m✔\033[0m %s\n", (msg));                          \
        } else {                                                               \
            printf("  \033[31m✘\033[0m %s\n", (msg));                          \
            g_input_failures++;                                                \
        }                                                                      \
    } while (0)

/* Pure-logic tests (non-interactive). */
void test_key_decode(void);
void test_shortcut(void);
void test_select_edit(void);
void test_opts_copy(void);
void test_line_editor(void);
void test_filters(void);
void test_number_format(void);
void test_calc(void);
void test_sanitize(void);
void test_suggest(void);
void test_history(void);
void test_no_tty(void);
void test_threads(void);

/* Interactive widget tests (require a TTY). */
void test_confirm(void);
void test_text_input(void);
void test_password_input(void);
void test_number(void);
void test_textarea(void);
void test_select(void);
void test_fuzzy(void);
void test_datepicker(void);
void test_theme(void);
