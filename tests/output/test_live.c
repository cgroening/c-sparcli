#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static ScRendered *build_dashboard_frame(int progress_percent);
static void print_contains(const char *label, const char *haystack,
                           const char *needle);


/**
 * Deterministic live-display checks. The suite runs with stdout redirected
 * to a file (non-TTY), so every session here exercises the buffered path:
 * updates are silent and only the final frame is printed by `sc_live_end`.
 * The escape-emission path is verified byte-wise through a memory stream
 * with `always = true`.
 */
void test_live(void) {
    printf("\n");

    /* ── 1. Non-terminal stream: only the final frame is printed ── */
    printf("--- Live 1. Buffered session: only the final frame ---\n");
    {
        ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
        sc_live_update_str(live, "Frame 1: starting...");
        sc_live_update_str(live, "Frame 2: working on it...");
        sc_live_update_str(live, "Frame 3: all done!");
        sc_live_end(live);
    }

    printf("\n");

    /* ── 2. Transient session: ends without any output ── */
    printf("--- Live 2. Transient session: no final frame ---\n");
    {
        ScLive *live = sc_live_begin((ScLiveOpts){ .transient = true });
        sc_live_update_str(live, "This frame must never appear");
        sc_live_end(live);
        printf("(nothing printed between this line and the header)\n");
    }

    printf("\n");

    /* ── 3. Composed dashboard frame (table + progress bar + vstack) ── */
    printf("--- Live 3. Composed dashboard: final state ---\n");
    {
        ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
        for (int step = 0; step <= 100; step += 50) {
            ScRendered *frame = build_dashboard_frame(step);
            sc_live_update(live, frame);
            sc_rendered_free(frame);
        }
        sc_live_end(live);   /* prints the 100% frame only */
    }

    printf("\n");

    /* ── 4. Table convenience updater ── */
    printf("--- Live 4. sc_live_update_table convenience ---\n");
    {
        ScTableData *table = sc_table_new();
        sc_table_add_column(table, "Service", (ScColOpts){ 0 });
        sc_table_add_column(table, "Status", (ScColOpts){ 0 });
        sc_table_add_row(table, (ScCell[]){
            sc_cell("api"), sc_cell("online"),
        }, 2);
        sc_table_add_row(table, (ScCell[]){
            sc_cell("worker"), sc_cell("online"),
        }, 2);

        ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
        sc_live_update_table(live, table, (ScTableOpts){
            .border = { .type = SC_BORDER_SINGLE },
            .header = { .row = true },
        });
        sc_live_end(live);
        sc_table_free(table);
    }

    printf("\n");

    /* ── 5. Escape emission (always = true) into a memory stream ── */
    printf("--- Live 5. Redraw escape codes (always = true) ---\n");
    {
        char *buffer = NULL;
        size_t size = 0;
        FILE *mem = open_memstream(&buffer, &size);
        if (!mem) {
            printf("open_memstream failed\n");
            return;
        }

        FILE *saved = sc_output_stream();
        sc_output_set_stream(mem);
        ScLive *live = sc_live_begin((ScLiveOpts){ .always = true });
        sc_live_update_str(live, "one\ntwo\nthree");   /* 3 lines */
        sc_live_update_str(live, "ONE\nTWO");          /* shrinks to 2 lines */
        sc_live_end(live);
        sc_output_set_stream(saved);
        fclose(mem);

        const char *bytes = buffer ? buffer : "";
        print_contains("cursor hidden at begin", bytes, "\033[?25l");
        print_contains("rewind to frame top (2 lines up)", bytes, "\033[2A");
        print_contains("per-line erase to end of line", bytes, "\033[K");
        print_contains("erase leftover lines below", bytes, "\033[J");
        print_contains("cursor restored at end", bytes, "\033[?25h");
        print_contains("first frame content drawn", bytes, "three");
        print_contains("second frame content drawn", bytes, "TWO");
        free(buffer);
    }

    printf("\n");

    /* ── 6. Alt-screen session re-prints the final frame on the
            normal screen ── */
    printf("--- Live 6. Alt-screen escape codes (always = true) ---\n");
    {
        char *buffer = NULL;
        size_t size = 0;
        FILE *mem = open_memstream(&buffer, &size);
        if (!mem) {
            printf("open_memstream failed\n");
            return;
        }

        FILE *saved = sc_output_stream();
        sc_output_set_stream(mem);
        ScLive *live = sc_live_begin((ScLiveOpts){
            .alt_screen = true, .always = true,
        });
        sc_live_update_str(live, "fullscreen dashboard");
        sc_live_end(live);
        sc_output_set_stream(saved);
        fclose(mem);

        const char *bytes = buffer ? buffer : "";
        print_contains("alt screen entered", bytes, "\033[?1049h");
        print_contains("alt screen left", bytes, "\033[?1049l");
        const char *after_leave = strstr(bytes, "\033[?1049l");
        print_contains("final frame re-printed after leaving",
                       after_leave ? after_leave : "",
                       "fullscreen dashboard");
        free(buffer);
    }

    printf("\n");

    /* ── 7. Reserved prompt rows (REPL dashboards) ── */
    printf("--- Live 7. Reserved prompt rows (always = true) ---\n");
    {
        char *buffer = NULL;
        size_t size = 0;
        FILE *mem = open_memstream(&buffer, &size);
        if (!mem) {
            printf("open_memstream failed\n");
            return;
        }

        FILE *saved = sc_output_stream();
        sc_output_set_stream(mem);
        ScLive *live = sc_live_begin((ScLiveOpts){
            .always = true, .prompt_rows = 1,
        });
        sc_live_update_str(live, "header\nbody");   /* 2 lines + 1 reserved */
        sc_live_update_str(live, "header\nBODY");   /* redrawn in place */
        sc_live_end(live);
        sc_output_set_stream(saved);
        fclose(mem);

        const char *bytes = buffer ? buffer : "";
        /* The park sequence after each frame: erase below, one newline into
           the reserved row, carriage return to column 0. */
        print_contains("cursor parked below the frame", bytes, "\033[J\n\r");
        /* The second update must rewind over the frame AND the reserved row
           (2 frame lines + 1 reserved - 1 = 2 lines up). */
        print_contains("rewind spans frame + reserved row", bytes, "\033[2A");
        print_contains("updated frame content drawn", bytes, "BODY");
        free(buffer);
    }

    /* ── 8. Vertical alignment on the alt screen (always = true) ── */
    printf("--- Live 8. Vertical alignment (always = true) ---\n");
    {
        char *buffer = NULL;
        size_t size = 0;
        FILE *mem = open_memstream(&buffer, &size);
        if (!mem) {
            printf("open_memstream failed\n");
            return;
        }

        FILE *saved = sc_output_stream();
        sc_output_set_stream(mem);
        ScLive *live = sc_live_begin((ScLiveOpts){
            .alt_screen = true, .always = true, .valign = SC_VALIGN_BOTTOM,
        });
        sc_live_update_str(live, "header\nbody");
        sc_live_end(live);
        sc_output_set_stream(saved);
        fclose(mem);

        const char *bytes = buffer ? buffer : "";
        /* Bottom alignment pushes the 2-line frame down with leading blank
           rows; each padding row is an erase-to-eol + newline (`\033[K\n`),
           several in a row - which never occurs in a top-anchored frame. */
        print_contains("leading blank rows for bottom valign", bytes,
                       "\033[K\n\033[K\n\033[K\n");
        print_contains("frame content still drawn", bytes, "header");
        free(buffer);
    }
}

/**
 * Animated demo (~3 s): a dashboard composed of a status table and a
 * progress bar, rebuilt and redrawn in place each tick. Run in a real
 * terminal via `make test-output`.
 */
void test_live_animated(void) {
    printf("\n--- Live dashboard animated (~3s) ---\n");

    ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
    for (int progress = 0; progress <= 100; progress += 4) {
        ScRendered *frame = build_dashboard_frame(progress);
        sc_live_update(live, frame);
        sc_rendered_free(frame);
        usleep(120000);
    }
    sc_live_end(live);
}

/**
 * Builds one dashboard frame: a status table stacked above a progress
 * line, composed with the capture API.
 */
static ScRendered *build_dashboard_frame(int progress_percent) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "Task", (ScColOpts){ 0 });
    sc_table_add_column(table, "State", (ScColOpts){ 0 });
    sc_table_add_row(table, (ScCell[]){
        sc_cell("download"),
        sc_cell(progress_percent >= 50 ? "done" : "running"),
    }, 2);
    sc_table_add_row(table, (ScCell[]){
        sc_cell("install"),
        sc_cell(progress_percent >= 100 ? "done" : "waiting"),
    }, 2);
    ScRendered *table_part = sc_capture_table(table, (ScTableOpts){
        .border = { .type = SC_BORDER_ROUNDED },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
    });
    sc_table_free(table);

    char progress_line[64];
    snprintf(progress_line, sizeof progress_line,
             "Total progress: %d%%", progress_percent);
    ScRendered *progress_part = sc_capture_str(progress_line);

    const ScRendered *parts[2] = { table_part, progress_part };
    ScRendered *frame = sc_vstack(parts, 2, 0);
    sc_rendered_free(table_part);
    sc_rendered_free(progress_part);
    return frame;
}

/** Prints a deterministic yes/NO line for a substring check (golden-diffed). */
static void print_contains(const char *label, const char *haystack,
                           const char *needle) {
    bool found = haystack && strstr(haystack, needle) != NULL;
    printf("%-45s %s\n", label, found ? "yes" : "NO");
}
