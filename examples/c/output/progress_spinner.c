/*
 * progress_spinner.c - animated progress bars and spinners.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/progress_spinner
 */

#include <sparcli.h>

#include <stdio.h>
#include <unistd.h>


/** Microseconds between animation frames (kept short for the demo). */
#define FRAME_DELAY_US 12000


static void run_progress_bars(void);
static void run_spinner(void);
static void run_transient_line(void);


int main(void) {
    run_progress_bars();
    printf("\n");
    run_spinner();
    printf("\n");
    run_transient_line();
    return 0;
}

/** sc_clear_line overwrites the current line in place (\r + spaces + \r). */
static void run_transient_line(void) {
    // Useful for a transient status that should leave no trace afterwards.
    printf("Preparing...");
    fflush(stdout);
    usleep(500000);
    sc_clear_line();
    printf("Ready.\n");
}

/** Two progress bars: a styled block bar and a threshold-colored line bar. */
static void run_progress_bars(void) {
    // Block bar with brackets, label column and value display.
    ScProgressBar *download = sc_progressbar_new((ScProgressBarOpts){
        .type        = SC_PROGRESS_BLOCK,
        .left_cap    = "[",
        .right_cap   = "]",
        .fill_color  = SC_ANSI_COLOR_CYAN,
        .show_value  = true,
        .width       = 60,
        .label_width = 10,
        .label_style = { .attr = SC_TEXT_ATTR_BOLD },
    });
    sc_progressbar_set_label(download, "download");
    for (int step = 0; step <= 200; step++) {
        sc_progressbar_draw(download, (double)step, 200.0);
        usleep(FRAME_DELAY_US);
    }
    sc_progressbar_finish(download, 200.0, 200.0);
    sc_progressbar_free(download);

    // Line bar with threshold colors: red -> yellow -> green as it fills.
    ScProgressBar *deploy = sc_progressbar_new((ScProgressBarOpts){
        .type       = SC_PROGRESS_LINE,
        .width      = 60,
        .label_width = 10,
        .thresholds = {
            .enabled    = true,
            .mid        = 0.4,
            .high       = 0.8,
            .color_low  = SC_ANSI_COLOR_RED,
            .color_mid  = SC_ANSI_COLOR_YELLOW,
            .color_high = SC_ANSI_COLOR_GREEN,
        },
    });
    sc_progressbar_set_label(deploy, "deploy");
    for (int step = 0; step <= 100; step++) {
        // max == 0: the value is already a ratio between 0.0 and 1.0.
        sc_progressbar_draw(deploy, (double)step / 100.0, 0.0);
        usleep(FRAME_DELAY_US);
    }
    sc_progressbar_finish(deploy, 1.0, 0.0);
    sc_progressbar_free(deploy);
}

/** A spinner whose label changes while it runs. */
static void run_spinner(void) {
    ScSpinner *spinner = sc_spinner_new("Connecting...", (ScSpinnerOpts){
        .type        = SC_SPINNER_BRAILLE,
        .color       = SC_ANSI_COLOR_CYAN,
        .label_style = { .attr = SC_TEXT_ATTR_DIM },
    });

    for (int frame = 0; frame < 25; frame++) {
        sc_spinner_tick(spinner);
        usleep(40000);
    }
    sc_spinner_set_label(spinner, "Fetching data...");
    for (int frame = 0; frame < 25; frame++) {
        sc_spinner_tick(spinner);
        usleep(40000);
    }

    // finish() clears the line and prints a green check / red cross summary.
    sc_spinner_finish(spinner, true, "Done");
    sc_spinner_free(spinner);
}
