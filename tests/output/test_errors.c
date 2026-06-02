#include "sparcli.h"
#include <stdio.h>


void test_errors(void) {
    printf("\n");

    /* ── 1. Message only ── */
    printf("--- Errors 1. message only ---\n");
    {
        ScError *error = sc_error_new("Connection to the server failed");
        sc_error_print(error);
        sc_error_free(error);
    }

    printf("\n");

    /* ── 2. Message + cause chain ── */
    printf("--- Errors 2. message + causes ---\n");
    {
        ScError *error = sc_error_new("Config could not be loaded");
        sc_error_add_cause(error, "file not found: ~/.config/app/config.toml");
        sc_error_add_cause(error, "directory ~/.config/app does not exist");
        sc_error_print(error);
        sc_error_free(error);
    }

    printf("\n");

    /* ── 3. Message + causes + hint ── */
    printf("--- Errors 3. message + causes + hint ---\n");
    {
        ScError *error = sc_error_new("Config could not be loaded");
        sc_error_add_cause(error, "file not found: ~/.config/app/config.toml");
        sc_error_set_hint(error, "Run 'app init' to create a default config");
        sc_error_print(error);
        sc_error_free(error);
    }

    printf("\n");

    /* ── 4. Message + hint (no causes) ── */
    printf("--- Errors 4. message + hint ---\n");
    {
        ScError *error = sc_error_new("Unknown option '--verbos'");
        sc_error_set_hint(error, "Did you mean '--verbose'?");
        sc_error_print(error);
        sc_error_free(error);
    }

    printf("\n");

    /* ── 5. Injected escape sequences are sanitized ── */
    printf("--- Errors 5. sanitized message ---\n");
    {
        ScError *error = sc_error_new("evil \033[31minjected\033[0m message");
        sc_error_add_cause(error, "cause with \033]0;title\a OSC bytes");
        sc_error_print(error);
        sc_error_free(error);
    }
}
