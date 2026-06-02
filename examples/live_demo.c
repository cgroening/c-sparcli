/*
 * live_demo.c
 *
 * Live-display dashboard demo: composes a status table, two progress bars
 * and a log line into one frame (capture API + vstack) and redraws it in
 * place a few times per second. Shows the default in-place mode first, then
 * the fullscreen alternate-screen mode.
 *
 * Build (after `make`):
 *   make run-example EX=live_demo
 * or:
 *   cc -std=c11 -Iinclude examples/live_demo.c libsparcli.a -o live_demo
 *
 * Pass `--alt-screen` to run only the fullscreen variant.
 */

#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/** Per-tick simulation state for the fake deployment the dashboard shows. */
typedef struct DemoState {
    int build_percent;
    int upload_percent;
    int tick;
} DemoState;


static void run_dashboard(ScLiveOpts opts, int ticks);
    static ScRendered *build_frame(const DemoState *state);
        static ScRendered *capture_status_table(const DemoState *state);
        static ScRendered *capture_progress(const char *label, int percent);
        static ScRendered *capture_log_line(const DemoState *state);


int main(int argc, char *argv[]) {
    bool alt_screen_only =
        argc > 1 && strcmp(argv[1], "--alt-screen") == 0;

    if (!alt_screen_only) {
        sc_markup_println("[bold]In-place live dashboard[/] "
                          "[dim](updates at the cursor position)[/]\n");
        run_dashboard((ScLiveOpts){ 0 }, 40);
        sc_markup_println("\n[dim]The final frame stays in the scrollback. "
                          "Next: fullscreen mode...[/]");
        sleep(2);
    }

    run_dashboard((ScLiveOpts){ .alt_screen = true }, 40);
    sc_markup_println("[bold green]\xe2\x9c\x94[/] Done - the previous screen "
                      "content was restored.");
    return 0;
}

/** Runs one live session for `ticks` frames of the simulated deployment. */
static void run_dashboard(ScLiveOpts opts, int ticks) {
    ScLive *live = sc_live_begin(opts);
    DemoState state = { 0 };

    for (int i = 0; i <= ticks; i++) {
        state.tick = i;
        state.build_percent = i * 100 / ticks;
        state.upload_percent = i <= ticks / 2 ? 0 : (i - ticks / 2) * 200 / ticks;

        ScRendered *frame = build_frame(&state);
        sc_live_update(live, frame);
        sc_rendered_free(frame);
        usleep(100000);
    }
    sc_live_end(live);
}

/** Composes the full dashboard frame: table + 2 progress bars + log line. */
static ScRendered *build_frame(const DemoState *state) {
    ScRendered *table_part = capture_status_table(state);
    ScRendered *build_part = capture_progress("build ", state->build_percent);
    ScRendered *upload_part = capture_progress("upload", state->upload_percent);
    ScRendered *log_part = capture_log_line(state);

    const ScRendered *parts[4] = {
        table_part, build_part, upload_part, log_part,
    };
    ScRendered *frame = sc_vstack(parts, 4, 1);

    sc_rendered_free(table_part);
    sc_rendered_free(build_part);
    sc_rendered_free(upload_part);
    sc_rendered_free(log_part);
    return frame;
}

/** The service-status table at the top of the dashboard. */
static ScRendered *capture_status_table(const DemoState *state) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Stage", (ScColOpts){ .min_width = 10 });
    sc_table_add_column(table, "Status", (ScColOpts){ .min_width = 12 });

    bool built = state->build_percent >= 100;
    bool uploaded = state->upload_percent >= 100;
    sc_table_add_row(table, (ScCell[]){
        sc_cell("build"),
        sc_cell_m(built ? "[green]\xe2\x9c\x94 done[/]"
                        : "[yellow]\xe2\x80\xa6 running[/]"),
    }, 2);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("upload"),
        sc_cell_m(uploaded         ? "[green]\xe2\x9c\x94 done[/]"
                  : state->upload_percent > 0 ? "[yellow]\xe2\x80\xa6 running[/]"
                                              : "[dim]waiting[/]"),
    }, 2);

    ScRendered *part = sc_capture_table(table, (ScTableOpts){
        .border = { .type = SC_BORDER_ROUNDED,
                    .outer_color = SC_ANSI_COLOR_CYAN },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
        .title = { .text = "Deployment", .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE,
        } },
    });
    sc_table_free(table);
    return part;
}

/** One labeled progress bar rendered into a fixed-width line. */
static ScRendered *capture_progress(const char *label, int percent) {
    // The progress bar is a stateful in-place widget; for the dashboard we
    // only need a single drawn frame, so finish() renders the final state
    // into the capture without animation control codes.
    char *buffer = NULL;
    size_t size = 0;
    FILE *mem = open_memstream(&buffer, &size);
    if (!mem) {
        return sc_capture_str(label);
    }
    FILE *saved = sc_output_stream();
    sc_output_set_stream(mem);

    ScProgressBar *bar = sc_progressbar_new((ScProgressBarOpts){
        .type = SC_PROGRESS_LINE,
        .width = 48,
        .label_width = 7,
        .thresholds = { .enabled = true },
    });
    sc_progressbar_set_label(bar, label);
    sc_progressbar_finish(bar, (double)percent, 100.0);
    sc_progressbar_free(bar);

    sc_output_set_stream(saved);
    fclose(mem);

    ScRendered *part = sc_capture_str(buffer ? buffer : "");
    free(buffer);
    return part;
}

/** A dim one-line activity log under the bars. */
static ScRendered *capture_log_line(const DemoState *state) {
    char line[96];
    snprintf(line, sizeof line, "[dim]tick %d \xc2\xb7 last event: %s[/]",
             state->tick,
             state->upload_percent >= 100 ? "upload finished"
             : state->upload_percent > 0  ? "uploading artifacts"
             : state->build_percent > 0   ? "compiling sources"
                                          : "starting");
    ScText *text = sc_markup_parse(line);
    ScRendered *part = sc_capture_text(text);
    sc_text_free(text);
    return part;
}
