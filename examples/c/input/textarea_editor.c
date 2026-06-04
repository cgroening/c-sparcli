/*
 * textarea_editor.c - multi-line text entry and the external-editor hook.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/textarea_editor
 *
 * Keys: Enter inserts a newline, Ctrl-D submits, Ctrl-G opens $EDITOR.
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static void run_textarea(void);
static void run_editor_input(void);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    run_textarea();
    run_editor_input();
    return 0;
}

/** Boxed multi-line editor with soft-wrapped long lines. */
static void run_textarea(void) {
    char *notes = NULL;
    ScInputStatus status = sc_textarea(
        "Release notes (Ctrl-D submits)", &notes,
        (ScTextareaOpts){
            .placeholder = "What changed?",
            .box.enabled = true,
            .box.border  = { .type = SC_BORDER_ROUNDED },
            .box.width   = 52,
        });

    if (status == SC_INPUT_OK) {
        // Count the submitted lines.
        size_t lines = 1;
        for (const char *c = notes; *c; c++) {
            if (*c == '\n') {
                lines++;
            }
        }
        printf("  -> %zu line(s), %zu bytes\n", lines, strlen(notes));
        free(notes);
    }
}

/**
 * External editor: Ctrl-G suspends the prompt, opens $EDITOR (or the
 * configured command) on a temp file, and reads the result back. For the
 * single-line text input, newlines are collapsed to spaces.
 */
static void run_editor_input(void) {
    char *commit_message = NULL;
    ScInputStatus status = sc_text_input(
        "Commit message (Ctrl-G opens the editor)", &commit_message,
        (ScTextInputOpts){
            .external_editor = true,
            // .editor = "nano",        // override $EDITOR explicitly
            // .editor_key = sc_key_ctrl('e'),   // rebind the shortcut
        });

    if (status == SC_INPUT_OK) {
        printf("  -> \"%s\"\n", commit_message);
        free(commit_message);
    }
}
