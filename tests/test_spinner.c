#include "sparcli.h"
#include <stdio.h>
#include <unistd.h>


void test_spinner(void) {
    printf(
        "\n\n═══════════════════════════  SPINNER  "
        "═══════════════════════════\n\n"
    );

    /* ── 1. Braille ── */
    {
        printf("--- 1. Braille ---\n");
        ScSpinner *spinner = sc_spinner_new("Compiling...", (ScSpinnerOpts){
            .style = SC_SPINNER_BRAILLE,
            .color = SC_ANSI_COLOR_CYAN,
        });
        sc_spinner_tick(spinner);
        sc_spinner_tick(spinner);
        sc_spinner_finish(spinner, true, "Build complete");
        sc_spinner_free(spinner);
    }

    /* ── 2. Pipe ── */
    {
        printf("--- 2. Pipe ---\n");
        ScSpinner *spinner = sc_spinner_new("Connecting...", (ScSpinnerOpts){
            .style = SC_SPINNER_PIPE,
            .color = SC_ANSI_COLOR_YELLOW,
        });
        sc_spinner_tick(spinner);
        sc_spinner_tick(spinner);
        sc_spinner_finish(spinner, false, "Connection refused");
        sc_spinner_free(spinner);
    }

    /* ── 3. Dots ── */
    {
        printf("--- 3. Dots ---\n");
        ScSpinner *spinner = sc_spinner_new("Analyzing...", (ScSpinnerOpts){
            .style = SC_SPINNER_DOTS,
            .color = SC_ANSI_COLOR_MAGENTA,
        });
        sc_spinner_tick(spinner);
        sc_spinner_tick(spinner);
        sc_spinner_finish(spinner, true, "Analysis complete");
        sc_spinner_free(spinner);
    }

    /* ── 4. Arrow ── */
    {
        printf("--- 4. Arrow ---\n");
        ScSpinner *spinner = sc_spinner_new("Deploying...", (ScSpinnerOpts){
            .style = SC_SPINNER_ARROW,
            .color = SC_ANSI_COLOR_BLUE,
        });
        sc_spinner_tick(spinner);
        sc_spinner_tick(spinner);
        sc_spinner_finish(spinner, true, "Deployed successfully");
        sc_spinner_free(spinner);
    }
}

void test_spinner_animated(void) {
    printf("\n--- Spinner animated (~2.5s) ---\n");
    ScSpinner *spinner = sc_spinner_new("Loading...", (ScSpinnerOpts){
        .style = SC_SPINNER_BRAILLE,
        .color = SC_ANSI_COLOR_CYAN,
    });

    for (int i = 0; i < 12; i++) {
        sc_spinner_tick(spinner);
        usleep(80000);
    }

    sc_spinner_set_label(spinner, "Fetching data...");
    for (int i = 0; i < 12; i++) {
        sc_spinner_tick(spinner);
        usleep(80000);
    }

    sc_spinner_set_label(spinner, "Finalizing...");
    for (int i = 0; i < 6; i++) {
        sc_spinner_tick(spinner);
        usleep(80000);
    }

    sc_spinner_finish(spinner, true, "Done");
    sc_spinner_free(spinner);
}
