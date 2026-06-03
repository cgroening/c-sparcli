/*
 * repl_demo.c
 *
 * A complete REPL built from the args module + input history: a tiny task
 * manager with shell-like commands. Shows the full loop:
 *
 *   read   - sc_text_input with persistent Up/Down history
 *   split  - sc_args_split turns the line into argv (quotes, escapes)
 *   parse  - sc_args_parse against a declarative command tree (the tree is
 *            built once; every parse implicitly resets the previous results)
 *   print  - tables / alerts / pretty parse errors, scrolling like a shell
 *
 * Build (after `make`):
 *   make run-example EX=repl_demo
 * or:
 *   cc -std=c11 -Iinclude examples/c/apps/repl_demo.c libsparcli.a -o repl_demo
 *
 * Commands (type `help` for the generated overview):
 *   add "Buy milk" --due friday      add a task (quotes group words)
 *   list [--all]                     list open (or all) tasks
 *   done ID                          mark a task as completed
 *   help                             rendered by the args module
 *   quit                             exit
 *
 * The input history persists across runs in
 * ~/.local/state/sparcli-repl-demo/history.
 */

#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** One task in the in-memory task list. */
typedef struct Task {
    char text[128];
    char due[64];
    bool completed;
} Task;

enum { MAX_TASKS = 64 };

static Task g_tasks[MAX_TASKS];
static size_t g_task_count;


// Forward declarations indented to reflect call hierarchy
static ScArgs *build_command_tree(void);
static bool dispatch(ScArgs *args, const ScArgsCmd *matched);
    static void cmd_add(ScArgs *args);
    static void cmd_list(ScArgs *args);
    static void cmd_done(ScArgs *args);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this in a real terminal (not under a pipe).");
        return 0;
    }

    ScText *welcome = sc_markup_parse(
        "A tiny task manager driven by REPL commands.\n"
        "Type [bold]help[/] for the command overview, [bold]quit[/] to exit."
    );
    sc_panel_text(welcome, (ScPanelOpts){
        .border = { .type = SC_BORDER_ROUNDED },
        .title  = { .text = "sparcli REPL demo",
                    .style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                               SC_ANSI_COLOR_NONE } },
        .padding = { 0, 1, 0, 1 },
    });
    sc_text_free(welcome);
    printf("\n");

    // The command tree is built once and reused for every input line:
    // sc_args_parse resets the previous results automatically.
    ScArgs *args = build_command_tree();

    // Up/Down history, persisted across runs in the XDG state directory.
    ScHistory *history = sc_history_new((ScHistoryOpts){
        .app = "sparcli-repl-demo",
    });

    bool running = true;
    while (running) {
        char *line = NULL;
        ScInputStatus status = sc_text_input("tasks>", &line,
            (ScTextInputOpts){
                .history      = history,
                .placeholder  = "add \"Buy milk\" --due friday",
                .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                                  SC_ANSI_COLOR_NONE },
            });
        if (status != SC_INPUT_OK) {
            break;   // Esc / Ctrl-C
        }

        // Tokenize the line shell-style: quotes group, backslash escapes.
        int argc = 0;
        char err[64];
        char **argv = sc_args_split("tasks", line, &argc, err, sizeof err);
        free(line);
        if (!argv) {
            sc_alert_error(err);
            continue;
        }
        if (argc <= 1) {
            sc_args_split_free(argv);   // empty line
            continue;
        }

        // Parse against the command tree. Errors (unknown command, missing
        // value, ...) are rendered as pretty panels with did-you-mean - the
        // loop just continues.
        ScArgsStatus parse_status;
        const ScArgsCmd *matched =
            sc_args_parse(args, argc, argv, &parse_status);
        if (parse_status == SC_ARGS_MATCHED) {
            running = dispatch(args, matched);
        }
        sc_args_split_free(argv);
        printf("\n");
    }

    sc_markup_println("[green]\xe2\x9c\x94[/] Bye - your input history "
                      "was saved.");
    sc_history_free(history);   // persists the history file
    sc_args_free(args);
    return 0;
}


/* ── The declarative command tree ────────────────────────────────────────── */

static ScArgs *build_command_tree(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog    = "tasks",
        .version = "1.0",
        .about   = "A tiny task-manager REPL",
    });
    ScArgsCmd *root = sc_args_root(args);

    ScArgsCmd *add = sc_args_subcommand(root, "add", "Add a new task");
    sc_args_positional(add, "TEXT", SC_ARG_STR, "Task description",
                       true, false);
    sc_args_opt(add, "due", 'd', SC_ARG_STR, "WHEN", "Due date");

    ScArgsCmd *list = sc_args_subcommand(root, "list", "List open tasks");
    sc_args_flag(list, "all", 'a', "Include completed tasks");

    ScArgsCmd *done = sc_args_subcommand(root, "done",
                                         "Mark a task as completed");
    sc_args_positional(done, "ID", SC_ARG_INT, "Task number from `list`",
                       true, false);

    sc_args_subcommand(root, "help", "Show the command overview");
    sc_args_subcommand(root, "quit", "Exit the REPL");
    return args;
}


/* ── Command dispatch ────────────────────────────────────────────────────── */

/** Runs the matched command. Returns false when the REPL should exit. */
static bool dispatch(ScArgs *args, const ScArgsCmd *matched) {
    const char *name = sc_args_cmd_name(matched);

    if (strcmp(name, "add") == 0) {
        cmd_add(args);
    } else if (strcmp(name, "list") == 0) {
        cmd_list(args);
    } else if (strcmp(name, "done") == 0) {
        cmd_done(args);
    } else if (strcmp(name, "help") == 0) {
        sc_args_print_help(args, NULL);
    } else if (strcmp(name, "quit") == 0) {
        return false;
    } else {
        // Bare input matched the root (e.g. "--version" was already
        // handled); nothing to do.
        sc_markup_println("[dim]Type `help` for the available commands.[/]");
    }
    return true;
}

/** `add TEXT [--due WHEN]` */
static void cmd_add(ScArgs *args) {
    if (g_task_count >= MAX_TASKS) {
        sc_alert_error("The task list is full.");
        return;
    }

    const char *text = sc_args_get_str(args, "TEXT");
    const char *due = sc_args_get_str(args, "due");

    Task *task = &g_tasks[g_task_count++];
    snprintf(task->text, sizeof task->text, "%s", text ? text : "");
    snprintf(task->due, sizeof task->due, "%s", due ? due : "");
    task->completed = false;

    char message[192];
    snprintf(message, sizeof message, "Added task %zu: %s",
             g_task_count, task->text);
    sc_alert_success(message);
}

/** `list [--all]` */
static void cmd_list(ScArgs *args) {
    bool show_all = sc_args_get_flag(args, "all");

    ScTableData *table = sc_table_new();
    sc_table_add_column(table, "#", (ScColOpts){ .halign = SC_ALIGN_RIGHT });
    sc_table_add_column(table, "Task", (ScColOpts){ .min_width = 24 });
    sc_table_add_column(table, "Due", (ScColOpts){ 0 });
    sc_table_add_column(table, "State", (ScColOpts){ 0 });

    // Cell strings are borrowed until print, so the id buffers must outlive
    // the loop (one slot per row, not one reused stack variable).
    char ids[MAX_TASKS][16];
    size_t shown = 0;
    for (size_t i = 0; i < g_task_count; i++) {
        const Task *task = &g_tasks[i];
        if (task->completed && !show_all) {
            continue;
        }
        snprintf(ids[i], sizeof ids[i], "%zu", i + 1);
        sc_table_add_row(table, (ScCell[]){
            sc_cell(ids[i]),
            sc_cell(task->text),
            sc_cell(task->due[0] ? task->due : "-"),
            sc_cell_m(task->completed ? "[green]done[/]" : "[yellow]open[/]"),
        }, 4);
        shown++;
    }

    if (shown == 0) {
        sc_markup_println(show_all
            ? "[dim]No tasks yet - `add \"...\"` creates one.[/]"
            : "[dim]No open tasks - try `list --all`.[/]");
    } else {
        sc_table_print(table, (ScTableOpts){
            .border = { .type = SC_BORDER_ROUNDED },
            .header = { .row = true, .style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
            } },
        });
    }
    sc_table_free(table);
}

/** `done ID` */
static void cmd_done(ScArgs *args) {
    long id = sc_args_get_int(args, "ID");
    if (id < 1 || (size_t)id > g_task_count) {
        sc_alert_error("No task with that number - see `list`.");
        return;
    }

    g_tasks[id - 1].completed = true;
    char message[192];
    snprintf(message, sizeof message, "Completed: %s", g_tasks[id - 1].text);
    sc_alert_success(message);
}
