/*
 * history.c - input history: Up/Down recall and file persistence.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/history
 *
 * Type a few lines; Up/Down recalls earlier ones. The history is saved to
 * an XDG state file, so a second run recalls lines from the first.
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Application name used for the XDG state file. */
#define APP_NAME "sparcli-history-example"


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        return 0;
    }

    // .app -> persisted to the XDG state dir; loaded automatically here,
    // saved automatically by sc_history_free. Alternative: .file = "path".
    ScHistory *history = sc_history_new((ScHistoryOpts){
        .app         = APP_NAME,
        .max_entries = 100,
    });

    if (sc_history_count(history) > 0) {
        printf("Loaded %zu line(s) from the previous run; "
               "press Up to recall them.\n", sc_history_count(history));
    }

    // Submitted lines are added to the attached history automatically
    // (empty lines and consecutive duplicates are skipped).
    for (int round = 0; round < 3; round++) {
        char *line = NULL;
        ScInputStatus status = sc_text_input(">", &line, (ScTextInputOpts){
            .history     = history,
            .placeholder = "type, or press Up",
        });
        if (status != SC_INPUT_OK) {
            break;
        }
        free(line);
    }

    // Show what the history now contains (index 0 = oldest entry).
    printf("\nHistory contents:\n");
    for (size_t i = 0; i < sc_history_count(history); i++) {
        printf("  %2zu: %s\n", i, sc_history_get(history, i));
    }

    char *state_file = sc_path_file(SC_PATH_STATE, APP_NAME, "history");
    if (state_file) {
        printf("Saved to: %s\n", state_file);
        free(state_file);
    }

    // Saves to the state file, then frees the handle.
    sc_history_free(history);
    return 0;
}
