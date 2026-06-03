/*
 * text_password.c - single-line text input (validation, filters,
 * autocomplete, boxed mode) and masked password input.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/text_password
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static bool validate_not_empty(const char *value, void *ctx,
                               const char **error);

static void run_plain_input(void);
static void run_autocomplete_input(void);
static void run_rich_prompt(void);
static void run_boxed_input(void);
static void run_password(void);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    run_plain_input();
    run_autocomplete_input();
    run_rich_prompt();
    run_boxed_input();
    run_password();
    return 0;
}

/** Rich prompt: only part of the label styled. */
static void run_rich_prompt(void) {
    // prompt_markup parses the prompt string as markup (here the old name is
    // italic). For dynamic text, prompt_text takes a pre-built ScText instead,
    // which needs no escaping even if the text contains '['.
    char *renamed = NULL;
    ScInputStatus status = sc_text_input(
        "Rename [italic]Apple[/] to", &renamed,
        (ScTextInputOpts){ .initial = "Apple", .prompt_markup = true });
    if (status == SC_INPUT_OK) {
        printf("  -> \"%s\"\n", renamed);
        free(renamed);
    }
}

/** Validation callback: keeps the prompt open until it returns true. */
static bool validate_not_empty(const char *value, void *ctx,
                               const char **error) {
    (void)ctx;
    if (value[0] == '\0') {
        *error = "Please enter a value";
        return false;
    }
    return true;
}

/** Placeholder + validation; the result is a heap string. */
static void run_plain_input(void) {
    char *name = NULL;
    ScInputStatus status = sc_text_input("Project name", &name,
        (ScTextInputOpts){
            .placeholder = "my-project",
            .validate    = validate_not_empty,
        });

    if (status == SC_INPUT_OK) {
        printf("  -> \"%s\"\n", name);
        free(name);
    }
}

/** Suggestions in a fuzzy dropdown + a character filter. */
static void run_autocomplete_input(void) {
    static const char *const COMMANDS[] = {
        "commit", "checkout", "cherry-pick", "clone", "config",
    };

    char *command = NULL;
    ScInputStatus status = sc_text_input("Git command", &command,
        (ScTextInputOpts){
            // Reject keystrokes that would insert a space.
            .char_filter   = sc_filter_no_space,
            .suggestions   = COMMANDS,
            .n_suggestions = sizeof COMMANDS / sizeof COMMANDS[0],
            // Dropdown list below the field; arrows move, Tab/Enter accept.
            .suggest       = {
                .mode  = SC_SUGGEST_DROPDOWN,
                .match = SC_SUGGEST_MATCH_FUZZY,
            },
        });

    if (status == SC_INPUT_OK) {
        printf("  -> \"%s\"\n", command);
        free(command);
    }
}

/** Boxed mode: panel frame, character counter, length limit. */
static void run_boxed_input(void) {
    char *title = NULL;
    ScInputStatus status = sc_text_input("Title", &title,
        (ScTextInputOpts){
            .boxed       = true,
            .border      = { .type = SC_BORDER_ROUNDED },
            .width       = 44,
            .max_chars   = 32,    // also shown as "n/32" counter
            .placeholder = "Short and descriptive",
        });

    if (status == SC_INPUT_OK) {
        printf("  -> \"%s\"\n", title);
        free(title);
    }
}

/** Masked input; the value is never echoed to the terminal. */
static void run_password(void) {
    char *secret = NULL;
    ScInputStatus status = sc_password_input("Passphrase", &secret,
        (ScPasswordOpts){
            .mask      = "•",
            .max_chars = 64,
        });

    if (status == SC_INPUT_OK) {
        printf("  -> captured %zu bytes (not echoed)\n", strlen(secret));
        free(secret);
    }
}
