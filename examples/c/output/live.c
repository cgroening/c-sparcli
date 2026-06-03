/*
 * live.c - live display: re-render a composed frame in place (dashboard).
 *
 * Run (after `make`):
 *   make run-example EX=c/output/live
 *
 * Off a TTY (e.g. piped to a file) only the final frame is printed, so the
 * same code works in scripts and CI logs.
 */

#include <sparcli.h>

#include <stdio.h>
#include <unistd.h>


/** Total animation steps (kept low so the demo finishes quickly). */
#define TOTAL_STEPS 30


static ScRendered *build_frame(int step);


int main(void) {
    sc_markup_println("[bold]Live dashboard[/] [dim](redraws in place)[/]\n");

    ScLive *live = sc_live_begin((ScLiveOpts){ 0 });

    for (int step = 0; step <= TOTAL_STEPS; step++) {
        ScRendered *frame = build_frame(step);
        sc_live_update(live, frame);
        sc_rendered_free(frame);
        usleep(60000);
    }

    // end() restores the cursor; the final frame stays in the scrollback.
    sc_live_end(live);

    sc_markup_println("\n[green]✔[/] Frame above was redrawn "
                      "in place on every update.");
    return 0;
}

/** Composes one dashboard frame: status table + activity line. */
static ScRendered *build_frame(int step) {
    int percent = step * 100 / TOTAL_STEPS;

    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Job",    (ScColOpts){ .min_width = 10 });
    sc_table_add_column(table, "Status", (ScColOpts){ .min_width = 14 });

    sc_table_add_row(table, (ScCell[]){
        sc_cell("build"),
        sc_cell_m(percent >= 50 ? "[green]✔ done[/]" : "[yellow]running[/]"),
    }, 2);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("package"),
        sc_cell_m(percent >= 100 ? "[green]✔ done[/]"
                  : percent >= 50 ? "[yellow]running[/]"
                                  : "[dim]waiting[/]"),
    }, 2);

    ScRendered *table_part = sc_capture_table(table, (ScTableOpts){
        .border = { .type = SC_BORDER_ROUNDED,
                    .outer_color = SC_ANSI_COLOR_CYAN },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
    });
    sc_table_free(table);

    // Status line below the table.
    char line[64];
    snprintf(line, sizeof line, "[dim]progress: %d %%[/]", percent);
    ScText *text = sc_markup_parse(line);
    ScRendered *line_part = sc_capture_text(text);
    sc_text_free(text);

    const ScRendered *parts[] = { table_part, line_part };
    ScRendered *frame = sc_vstack(parts, 2, 1);

    sc_rendered_free(table_part);
    sc_rendered_free(line_part);
    return frame;
}
