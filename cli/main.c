/**
 * @file main.c
 * Entry point of the `sparcli` command-line tool: dispatches subcommands
 * that expose the sparcli library's output and input widgets to the shell
 * (see docs/cli.md for the full reference).
 */
#include "cli_commands.h"
#include "cli_common.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *const PROG_NAME = "sparcli";

/** Help grouping of a subcommand. */
typedef enum ScCliGroup {
    SC_CLI_GROUP_OUTPUT,
    SC_CLI_GROUP_STREAM,
    SC_CLI_GROUP_INPUT,
} ScCliGroup;

/** One entry of the subcommand dispatch table. */
typedef struct ScCliCmd {
    const char *name;    /**< Subcommand name, e.g. "panel". */
    ScCliCmdFn  fn;      /**< Implementation. */
    ScCliGroup  group;   /**< Help section the command is listed under. */
    const char *summary; /**< One-line description for the command list. */
} ScCliCmd;

static int parse_global_flags(ScCliCtx *ctx, int argc, char **argv,
                              int *cmd_index);
static void scan_trailing_flags(ScCliCtx *ctx, int argc, char **argv);
static int dispatch(ScCliCtx *ctx, const ScCliCmd *cmd, int argc,
                    char **argv);
static const ScCliCmd *find_command(const char *name);
static void print_usage(FILE *out);
    static void print_command_group(FILE *out, ScCliGroup group,
                                    const char *heading);

/* Subcommand registry: output widgets, streaming, then input widgets. */
static const ScCliCmd CLI_COMMANDS[] = {
    { .name = "print", .fn = sc_cli_cmd_print, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Print markup-styled text" },
    { .name = "panel", .fn = sc_cli_cmd_panel, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Draw a bordered panel around text" },
    { .name = "rule", .fn = sc_cli_cmd_rule, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Draw a horizontal rule with optional title" },
    { .name = "table", .fn = sc_cli_cmd_table, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Render CSV/TSV data as a table" },
    { .name = "list", .fn = sc_cli_cmd_list, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Render a bulleted or numbered list" },
    { .name = "tree", .fn = sc_cli_cmd_tree, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Render indented lines as a tree" },
    { .name = "kv", .fn = sc_cli_cmd_kv, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Render key/value lines as an aligned list" },
    { .name = "alert", .fn = sc_cli_cmd_alert, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Show an info/warning/error/success panel" },
    { .name = "badge", .fn = sc_cli_cmd_badge, .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Print an inline styled badge" },
    { .name = "columns", .fn = sc_cli_cmd_columns,
      .group = SC_CLI_GROUP_OUTPUT,
      .summary = "Lay out items side by side in columns" },

    { .name = "spin", .fn = sc_cli_cmd_spin, .group = SC_CLI_GROUP_STREAM,
      .summary = "Show a spinner while a command runs" },
    { .name = "progress", .fn = sc_cli_cmd_progress,
      .group = SC_CLI_GROUP_STREAM,
      .summary = "Draw a progress bar fed by stdin values" },

    { .name = "confirm", .fn = sc_cli_cmd_confirm,
      .group = SC_CLI_GROUP_INPUT,
      .summary = "Ask a yes/no question (exit code = answer)" },
    { .name = "input", .fn = sc_cli_cmd_input, .group = SC_CLI_GROUP_INPUT,
      .summary = "Prompt for a line of text" },
    { .name = "password", .fn = sc_cli_cmd_password,
      .group = SC_CLI_GROUP_INPUT,
      .summary = "Prompt for a masked secret" },
    { .name = "number", .fn = sc_cli_cmd_number, .group = SC_CLI_GROUP_INPUT,
      .summary = "Prompt for a number" },
    { .name = "textarea", .fn = sc_cli_cmd_textarea,
      .group = SC_CLI_GROUP_INPUT,
      .summary = "Prompt for multi-line text" },
    { .name = "select", .fn = sc_cli_cmd_select, .group = SC_CLI_GROUP_INPUT,
      .summary = "Choose one or more items from a list" },
    { .name = "fuzzy", .fn = sc_cli_cmd_fuzzy, .group = SC_CLI_GROUP_INPUT,
      .summary = "Fuzzy-search and pick an item" },
    { .name = "date", .fn = sc_cli_cmd_date, .group = SC_CLI_GROUP_INPUT,
      .summary = "Pick a date from a calendar" },
};

enum { CLI_COMMAND_COUNT = sizeof(CLI_COMMANDS) / sizeof(CLI_COMMANDS[0]) };

int main(int argc, char **argv) {
    ScCliCtx ctx = sc_cli_ctx_init(PROG_NAME);
    opterr       = 0; /* commands report unknown options themselves */

    int cmd_index = 0;
    int status    = parse_global_flags(&ctx, argc, argv, &cmd_index);
    if (status >= 0) {
        return status;
    }

    const ScCliCmd *cmd = find_command(argv[cmd_index]);
    if (cmd == NULL) {
        fprintf(stderr, "%s: unknown command '%s' (see %s --help)\n",
                PROG_NAME, argv[cmd_index], PROG_NAME);
        return SC_CLI_EXIT_ERROR;
    }

    scan_trailing_flags(&ctx, argc - cmd_index, argv + cmd_index);
    return dispatch(&ctx, cmd, argc - cmd_index, argv + cmd_index);
}

/* Consumes global flags before the subcommand. Returns an exit code when
   the program should stop (help/version/error), or -1 to continue with
   `*cmd_index` pointing at the subcommand. */
static int parse_global_flags(ScCliCtx *ctx, int argc, char **argv,
                              int *cmd_index) {
    int index = 1;

    while (index < argc && argv[index][0] == '-') {
        const char *flag = argv[index];
        if (strcmp(flag, "--help") == 0 || strcmp(flag, "-h") == 0) {
            print_usage(stdout);
            return SC_CLI_EXIT_OK;
        }
        if (strcmp(flag, "--version") == 0 || strcmp(flag, "-V") == 0) {
            printf("%s %s\n", PROG_NAME, sc_version_string());
            return SC_CLI_EXIT_OK;
        }
        if (strcmp(flag, "--no-color") == 0) {
            ctx->color = false;
        } else if (strcmp(flag, "--no-markup") == 0) {
            ctx->markup = false;
        } else if (strcmp(flag, "--allow-ansi") == 0) {
            ctx->allow_ansi = true;
        } else {
            fprintf(stderr, "%s: unknown option '%s' (see %s --help)\n",
                    PROG_NAME, flag, PROG_NAME);
            return SC_CLI_EXIT_ERROR;
        }
        index++;
    }

    if (index >= argc) {
        print_usage(stderr);
        return SC_CLI_EXIT_ERROR;
    }

    *cmd_index = index;
    return -1;
}

/* Applies --no-color / --no-markup found anywhere in the command's argv so
   the output capture can be set up before the command renders. Stops at
   "--" so pass-through arguments (e.g. `spin -- cmd --no-color`) are not
   affected. The commands accept the same flags via getopt, which is then
   a harmless no-op. */
static void scan_trailing_flags(ScCliCtx *ctx, int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--") == 0) {
            return;
        }
        if (strcmp(argv[i], "--no-color") == 0) {
            ctx->color = false;
        } else if (strcmp(argv[i], "--no-markup") == 0) {
            ctx->markup = false;
        } else if (strcmp(argv[i], "--allow-ansi") == 0) {
            ctx->allow_ansi = true;
        }
    }
}

/* Runs the command, capturing and ANSI-stripping its output when colors
   are disabled. */
static int dispatch(ScCliCtx *ctx, const ScCliCmd *cmd, int argc,
                    char **argv) {
    /* Apply the ANSI passthrough setting before the command renders. */
    sc_set_allow_ansi(ctx->allow_ansi);

    FILE *capture = sc_cli_capture_begin(ctx);

    int status         = cmd->fn(ctx, argc, argv);
    int capture_status = sc_cli_capture_end(capture);

    return (status != SC_CLI_EXIT_OK) ? status : capture_status;
}

static const ScCliCmd *find_command(const char *name) {
    for (size_t i = 0; i < CLI_COMMAND_COUNT; i++) {
        if (strcmp(CLI_COMMANDS[i].name, name) == 0) {
            return &CLI_COMMANDS[i];
        }
    }
    return NULL;
}

static void print_usage(FILE *out) {
    fprintf(out,
            "Usage: %s [global options] <command> [options] [arguments]\n"
            "\n"
            "Styled terminal output and interactive prompts for the shell,\n"
            "powered by the sparcli library.\n"
            "\n"
            "Global options:\n"
            "  -h, --help        Show this help and exit\n"
            "  -V, --version     Show the version and exit\n"
            "      --no-color    Strip ANSI colors from the output\n"
            "      --no-markup   Treat [tag] markup as literal text\n"
            "      --allow-ansi  Pass ANSI escapes in input text through\n",
            PROG_NAME);

    print_command_group(out, SC_CLI_GROUP_OUTPUT, "Output commands");
    print_command_group(out, SC_CLI_GROUP_STREAM, "Streaming commands");
    print_command_group(out, SC_CLI_GROUP_INPUT, "Input commands");

    fprintf(out,
            "\n"
            "Exit codes:\n"
            "  0  success / confirmed\n"
            "  1  input cancelled / answered no\n"
            "  2  error, bad usage or no TTY\n"
            "\n"
            "Run '%s <command> --help' for the options of a command.\n"
            "Full reference: docs/cli.md\n",
            PROG_NAME);
}

static void print_command_group(FILE *out, ScCliGroup group,
                                const char *heading) {
    fprintf(out, "\n%s:\n", heading);
    for (size_t i = 0; i < CLI_COMMAND_COUNT; i++) {
        if (CLI_COMMANDS[i].group != group) {
            continue;
        }
        fprintf(out, "  %-10s %s\n", CLI_COMMANDS[i].name,
                CLI_COMMANDS[i].summary);
    }
}
