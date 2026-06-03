/*
 * args_repl.c - reuse one parser tree for interactive input lines:
 * sc_args_split (tokenizer) + implicit reset on every parse.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/args_repl
 *
 * This example feeds fixed lines through the parser so it runs anywhere.
 * For a fully interactive REPL (prompt + history + live dashboard) see
 * examples/c/apps/repl_demo.c and examples/c/apps/repl_dashboard.c.
 */

#include <sparcli.h>

#include <stdio.h>
#include <string.h>


static ScArgs *build_parser(void);
static void run_line(ScArgs *args, const char *line);


int main(void) {
    ScArgs *args = build_parser();
    if (!args) {
        sc_die_msg(1, "Out of memory", NULL);
    }

    // Each line goes through the same tree; sc_args_parse resets the
    // previous results automatically, so no state leaks between lines.
    const char *const lines[] = {
        "add \"buy milk\" --priority high",
        "add 'read the docs'",
        "list --done",
        "add",                          // error: missing positional
        "lst",                          // error with did-you-mean
    };

    for (size_t i = 0; i < sizeof lines / sizeof lines[0]; i++) {
        run_line(args, lines[i]);
    }

    sc_args_free(args);
    return 0;
}

/** A small task-manager command tree. */
static ScArgs *build_parser(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog  = "tasks",
        .about = "REPL-style task manager",
    });
    if (!args) {
        return NULL;
    }

    ScArgsCmd *root = sc_args_root(args);

    ScArgsCmd *add = sc_args_subcommand(root, "add", "Add a task");
    sc_args_positional(add, "TITLE", SC_ARG_STR, "Task title", true, false);
    sc_args_opt(add, "priority", 'p', SC_ARG_STR, "LEVEL", "Priority");
    sc_args_opt_choices(add, "priority",
                        (const char *[]){ "low", "normal", "high", NULL });
    sc_args_opt_default(add, "priority", "normal");

    ScArgsCmd *list = sc_args_subcommand(root, "list", "List tasks");
    sc_args_flag(list, "done", 'd', "Include finished tasks");

    return args;
}

/** Tokenizes one input line and runs it through the parser. */
static void run_line(ScArgs *args, const char *line) {
    sc_markup_print("[bold cyan]tasks>[/] ");
    printf("%s\n", line);
    // Parse errors go to stderr; flush stdout so the order stays intact
    // when both streams are piped into one file.
    fflush(stdout);

    // The tokenizer understands quotes and escapes:
    //   "..." with \ escapes, '...' literal, adjacent runs concatenate.
    int line_argc = 0;
    char error[128];
    char **line_argv = sc_args_split("tasks", line, &line_argc,
                                     error, sizeof error);
    if (!line_argv) {
        printf("  tokenizer error: %s\n", error);
        return;
    }

    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, line_argc, line_argv,
                                             &status);
    sc_args_split_free(line_argv);

    if (status != SC_ARGS_MATCHED) {
        // Parse errors were already rendered as a pretty error report.
        return;
    }

    const char *command = sc_args_cmd_name(matched);
    if (strcmp(command, "add") == 0) {
        printf("  added \"%s\" (priority: %s)\n",
               sc_args_get_str(args, "TITLE"),
               sc_args_get_str(args, "priority"));
    } else if (strcmp(command, "list") == 0) {
        printf("  listing tasks (done included: %s)\n",
               sc_args_get_flag(args, "done") ? "yes" : "no");
    }
}
