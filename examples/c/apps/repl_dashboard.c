/*
 * repl_dashboard.c
 *
 * A dashboard-style REPL: a fixed header and a task table are redrawn
 * IN PLACE after every action (no scrolling), with an interactive prompt
 * and a shortcut bar pinned below. This is the live-display + prompt
 * composition pattern:
 *
 *   - sc_live_begin with `prompt_rows` reserves the prompt region below
 *     the frame; every sc_live_update parks the cursor there.
 *   - sc_text_input runs in that region with `hide_summary = true`, so it
 *     erases itself completely and the cursor lands back where the live
 *     display expects it.
 *   - The shortcut bar is the prompt's own shortcut footer: RETURN-mode
 *     shortcuts end the prompt and report an action id, the loop applies
 *     the action and the dashboard updates in place.
 *
 * Build (after `make`):
 *   make run-example EX=repl_dashboard
 * or:
 *   cc -std=c11 -Iinclude examples/c/apps/repl_dashboard.c libsparcli.a -o repl_dash
 *
 * Commands:   add <text...>   done <n>   del <n>   quit
 * Shortcuts:  F2 toggle help  ·  Ctrl-L clear completed  ·  Ctrl-X quit
 * History:    Up/Down recall previous commands
 */

#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** One task on the dashboard. */
typedef struct Task {
    char text[128];
    bool completed;
} Task;

enum { MAX_TASKS = 32 };

/** Everything the dashboard renders. */
typedef struct AppState {
    Task tasks[MAX_TASKS];
    size_t task_count;
    char status[160];        /**< One-line feedback under the table. */
    bool show_help;          /**< F2 toggles the help panel. */
    bool quit_requested;     /**< Set by the `quit` command. */
} AppState;

/** Ids reported by the RETURN-mode shortcuts. */
enum { ACTION_HELP = 1, ACTION_CLEAR = 2, ACTION_QUIT = 3 };

/* The prompt draws 2 rows: the input line + the shortcut bar (the widget's
 * own hint footer is hidden). prompt_rows must cover them so the dashboard
 * never scrolls the terminal. */
enum { PROMPT_ROWS = 2 };


// Forward declarations indented to reflect call hierarchy
static void seed_demo_tasks(AppState *state);
static ScRendered *build_frame(const AppState *state);
    static ScRendered *capture_header(void);
    static ScRendered *capture_task_table(const AppState *state);
    static ScRendered *capture_help(void);
static void dispatch(AppState *state, const char *line);
    static void cmd_add(AppState *state, char **argv, int argc);
    static void cmd_done(AppState *state, char **argv, int argc, bool del);
static void clear_completed(AppState *state);
static void set_status(AppState *state, const char *prefix, const char *arg);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    AppState state = { 0 };
    seed_demo_tasks(&state);
    set_status(&state, "Welcome - type a command or press F2 for help.", "");

    // Command history (in-memory; pass .app for persistence across runs)
    ScHistory *history = sc_history_new((ScHistoryOpts){ 0 });

    ScShortcut shortcuts[] = {
        { .chord = sc_key_fn(2),      .id = ACTION_HELP,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "help" },
        { .chord = sc_key_ctrl('l'),  .id = ACTION_CLEAR,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "clear done" },
        { .chord = sc_key_ctrl('x'),  .id = ACTION_QUIT,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "quit" },
    };

    // The live session owns the dashboard region; the reserved rows below
    // it are where the prompt runs.
    ScLive *live = sc_live_begin((ScLiveOpts){ .prompt_rows = PROMPT_ROWS });

    bool running = true;
    while (running) {
        // 1. Redraw the dashboard in place (cursor parks below it).
        ScRendered *frame = build_frame(&state);
        sc_live_update(live, frame);
        sc_rendered_free(frame);

        // 2. Run the prompt in the reserved region.
        int fired = -1;
        char *line = NULL;
        ScInputStatus status = sc_text_input("cmd>", &line,
            (ScTextInputOpts){
                .history         = history,
                .placeholder     = "add <text> / done <n> / del <n>",
                .prompt_style    = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                                     SC_ANSI_COLOR_NONE },
                .hide_summary    = true,            // required: no leftovers
                .hint_layout     = SC_HINT_HIDDEN,  // shortcut bar suffices
                .shortcuts       = shortcuts,
                .n_shortcuts     = 3,
                .out_shortcut_id = &fired,
            });
        if (status != SC_INPUT_OK) {
            break;   // Esc / Ctrl-C
        }

        // 3. Apply the action; the next loop iteration redraws the frame.
        if (fired == ACTION_QUIT) {
            running = false;
        } else if (fired == ACTION_HELP) {
            state.show_help = !state.show_help;
        } else if (fired == ACTION_CLEAR) {
            clear_completed(&state);
        } else if (line[0] != '\0') {
            dispatch(&state, line);
            running = !state.quit_requested;
        }
        free(line);
    }

    sc_live_end(live);   // the final dashboard stays on screen
    sc_markup_println("\n[green]\xe2\x9c\x94[/] Bye.");
    sc_history_free(history);
    return 0;
}


/* ── Dashboard frame ─────────────────────────────────────────────────────── */

static void seed_demo_tasks(AppState *state) {
    static const char *const SEED[] = {
        "Read the sparcli REPL docs", "Try the shortcut bar (F2)",
        "Build something with it",
    };
    for (size_t i = 0; i < sizeof SEED / sizeof SEED[0]; i++) {
        Task *task = &state->tasks[state->task_count++];
        snprintf(task->text, sizeof task->text, "%s", SEED[i]);
        task->completed = false;
    }
}

/** Header + task table + status line (+ optional help), vstack'd. */
static ScRendered *build_frame(const AppState *state) {
    ScRendered *header = capture_header();
    ScRendered *table = capture_task_table(state);

    ScText *status_text = sc_text_new();
    sc_text_append(status_text, state->status, (ScTextStyle){
        SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
    });
    ScRendered *status_line = sc_capture_text(status_text);
    sc_text_free(status_text);

    ScRendered *help = state->show_help ? capture_help() : NULL;

    const ScRendered *parts[4] = { header, table, status_line, help };
    size_t part_count = help ? 4 : 3;
    ScRendered *frame = sc_vstack(parts, part_count, 0);

    sc_rendered_free(header);
    sc_rendered_free(table);
    sc_rendered_free(status_line);
    sc_rendered_free(help);
    return frame;
}

/** The fixed title bar at the top of the dashboard. */
static ScRendered *capture_header(void) {
    ScText *title = sc_markup_parse(
        "[bold cyan]Task Dashboard[/]  [dim]live REPL demo[/]"
    );
    ScRendered *header = sc_capture_panel_text(title, (ScPanelOpts){
        .border  = { .type = SC_BORDER_ROUNDED },
        .padding = { 0, 1, 0, 1 },
    });
    sc_text_free(title);
    return header;
}

/** The task table (the dynamic core of the dashboard). */
static ScRendered *capture_task_table(const AppState *state) {
    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "#", (ScColOpts){ .halign = SC_ALIGN_RIGHT });
    sc_table_add_column(table, "Task", (ScColOpts){ .min_width = 32 });
    sc_table_add_column(table, "State", (ScColOpts){ 0 });

    // Cell strings are borrowed until capture, so the id buffers must
    // outlive the loop (one slot per row, not one reused stack variable).
    char ids[MAX_TASKS][16];
    for (size_t i = 0; i < state->task_count; i++) {
        snprintf(ids[i], sizeof ids[i], "%zu", i + 1);
        sc_table_add_row(table, (ScCell[]){
            sc_cell(ids[i]),
            sc_cell(state->tasks[i].text),
            sc_cell_m(state->tasks[i].completed
                          ? "[green]done[/]" : "[yellow]open[/]"),
        }, 3);
    }

    ScRendered *rendered = sc_capture_table(table, (ScTableOpts){
        .border = { .type = SC_BORDER_SINGLE },
        .header = { .row = true, .style = {
            SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
        } },
    });
    sc_table_free(table);
    return rendered;
}

/** The F2 help panel (toggled into the frame). */
static ScRendered *capture_help(void) {
    ScText *help = sc_markup_parse(
        "[bold]add[/] <text...>   add a task\n"
        "[bold]done[/] <n>        mark task n as completed\n"
        "[bold]del[/] <n>         remove task n\n"
        "[bold]quit[/]            exit (also Ctrl-X)"
    );
    ScRendered *panel = sc_capture_panel_text(help, (ScPanelOpts){
        .border  = { .type = SC_BORDER_ROUNDED },
        .title   = { .text = "Commands" },
        .padding = { 0, 1, 0, 1 },
    });
    sc_text_free(help);
    return panel;
}


/* ── Command dispatch ────────────────────────────────────────────────────── */

static void dispatch(AppState *state, const char *line) {
    int argc = 0;
    char err[64];
    char **argv = sc_args_split("dash", line, &argc, err, sizeof err);
    if (!argv) {
        set_status(state, "Input error: ", err);
        return;
    }
    if (argc <= 1) {
        sc_args_split_free(argv);
        return;
    }

    const char *cmd = argv[1];
    if (strcmp(cmd, "add") == 0) {
        cmd_add(state, argv, argc);
    } else if (strcmp(cmd, "done") == 0) {
        cmd_done(state, argv, argc, false);
    } else if (strcmp(cmd, "del") == 0) {
        cmd_done(state, argv, argc, true);
    } else if (strcmp(cmd, "quit") == 0) {
        state->quit_requested = true;
    } else {
        set_status(state, "Unknown command (F2 shows help): ", cmd);
    }
    sc_args_split_free(argv);
}

/** `add <text...>`: joins the remaining tokens into one task. */
static void cmd_add(AppState *state, char **argv, int argc) {
    if (argc < 3) {
        set_status(state, "Usage: add <text...>", "");
        return;
    }
    if (state->task_count >= MAX_TASKS) {
        set_status(state, "The task list is full.", "");
        return;
    }

    Task *task = &state->tasks[state->task_count++];
    task->text[0] = '\0';
    task->completed = false;
    for (int i = 2; i < argc; i++) {
        size_t used = strlen(task->text);
        snprintf(task->text + used, sizeof task->text - used, "%s%s",
                 used > 0 ? " " : "", argv[i]);
    }
    set_status(state, "Added: ", task->text);
}

/** `done <n>` / `del <n>`. */
static void cmd_done(AppState *state, char **argv, int argc, bool del) {
    long id = argc > 2 ? strtol(argv[2], NULL, 10) : 0;
    if (id < 1 || (size_t)id > state->task_count) {
        set_status(state, "Expected a valid task number after ",
                   del ? "del" : "done");
        return;
    }

    size_t index = (size_t)id - 1;
    if (del) {
        set_status(state, "Removed: ", state->tasks[index].text);
        for (size_t i = index; i + 1 < state->task_count; i++) {
            state->tasks[i] = state->tasks[i + 1];
        }
        state->task_count--;
    } else {
        state->tasks[index].completed = true;
        set_status(state, "Completed: ", state->tasks[index].text);
    }
}

/** Ctrl-L: drops every completed task. */
static void clear_completed(AppState *state) {
    size_t kept = 0;
    for (size_t i = 0; i < state->task_count; i++) {
        if (!state->tasks[i].completed) {
            state->tasks[kept++] = state->tasks[i];
        }
    }
    size_t removed = state->task_count - kept;
    state->task_count = kept;

    char count[32];
    snprintf(count, sizeof count, "%zu completed task(s).", removed);
    set_status(state, "Cleared ", count);
}

/** Sets the one-line status feedback to `prefix` + `arg`. */
static void set_status(AppState *state, const char *prefix, const char *arg) {
    snprintf(state->status, sizeof state->status, "%s%s", prefix, arg);
}
