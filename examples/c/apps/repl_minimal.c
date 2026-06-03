/*
 * repl_minimal.c
 *
 * The smallest possible sparcli REPL: a prompt loop with Up/Down input
 * history. Type lines and recall them with the arrow keys; the submitted
 * line stays in the scrollback (the prompt's summary line), so the session
 * reads like a shell transcript.
 *
 * Build (after `make`):
 *   make run-example EX=repl_minimal
 * or:
 *   cc -std=c11 -Iinclude examples/c/apps/repl_minimal.c libsparcli.a -o repl_minimal
 *
 * Esc or Ctrl-C ends the loop. Type "quit" to exit as well.
 */

#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    sc_markup_println("[bold]sparcli minimal REPL[/] - "
                      "[dim]\xe2\x86\x91/\xe2\x86\x93 history, Esc quits[/]\n");

    // In-memory history: Up/Down recall previous lines; every submitted
    // line is added automatically (consecutive duplicates collapse).
    ScHistory *history = sc_history_new((ScHistoryOpts){ 0 });

    for (;;) {
        char *line = NULL;
        ScInputStatus status = sc_text_input(">", &line, (ScTextInputOpts){
            .history     = history,
            .placeholder = "type something",
        });

        if (status != SC_INPUT_OK) {
            break;   // Esc / Ctrl-C / no terminal
        }
        if (strcmp(line, "quit") == 0) {
            free(line);
            break;
        }

        // A real REPL would dispatch the line here; this one just echoes.
        if (line[0] != '\0') {
            sc_markup_print("[dim]you typed:[/] ");
            printf("%s\n", line);
        }
        free(line);
    }

    sc_markup_println("\n[green]\xe2\x9c\x94[/] Bye.");
    sc_history_free(history);
    return 0;
}
