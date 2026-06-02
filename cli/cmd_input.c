/**
 * @file cmd_input.c
 * Interactive single-value input subcommands: `confirm`, `input`,
 * `password`, `number` and `textarea`.
 *
 * Contract shared by all input commands: the widget UI is drawn on
 * `/dev/tty`, only the raw result value is written to stdout, and the
 * exit code reports the outcome (0 = ok, 1 = cancelled/no, 2 = error or
 * no TTY). This makes them safe for command substitution:
 * `name=$(sparcli input "Name:")`.
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

enum { SC_CLI_MAX_SUGGESTIONS = 64 };

/* Prints a heap string result to stdout and frees it. */
static int finish_string_result(ScInputStatus status, char *value);


/* ── confirm ────────────────────────────────────────────────────────────── */

static const char CONFIRM_USAGE[] =
    "Usage: sparcli confirm [options] QUESTION...\n"
    "\n"
    "Ask a yes/no question. The exit code reports the answer: 0 = yes,\n"
    "1 = no or cancelled, 2 = error / no TTY. Nothing is printed unless\n"
    "--print is given.\n"
    "\n"
    "Options:\n"
    "  --default-yes              Preselect Yes (default)\n"
    "  --default-no               Preselect No\n"
    "  --yes LABEL                Label of the Yes choice\n"
    "  --no LABEL                 Label of the No choice\n"
    "  --print                    Print 'yes' or 'no' to stdout\n"
    SC_CLI_INPUT_USAGE;

int sc_cli_cmd_confirm(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_DEFAULT_YES = SC_CLI_OPT_CMD_BASE,
        OPT_DEFAULT_NO,
        OPT_YES,
        OPT_NO,
        OPT_PRINT,
    };
    static const struct option longopts[] = {
        { "default-yes", no_argument,       NULL, OPT_DEFAULT_YES },
        { "default-no",  no_argument,       NULL, OPT_DEFAULT_NO },
        { "yes",         required_argument, NULL, OPT_YES },
        { "no",          required_argument, NULL, OPT_NO },
        { "print",       no_argument,       NULL, OPT_PRINT },
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    ScConfirmOpts  opts        = { .default_yes = true };
    ScCliInputArgs input_args  = { 0 };
    bool           print_reply = false;

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_DEFAULT_YES:
            opts.default_yes = true;
            break;
        case OPT_DEFAULT_NO:
            opts.default_yes = false;
            break;
        case OPT_YES:
            opts.yes_label = optarg;
            break;
        case OPT_NO:
            opts.no_label = optarg;
            break;
        case OPT_PRINT:
            print_reply = true;
            break;
        case SC_CLI_OPT_HELP:
            fputs(CONFIRM_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &input_args)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    char *question = sc_cli_content(argc - optind, argv + optind);
    if (question == NULL || question[0] == '\0') {
        free(question);
        return sc_cli_error(ctx, "missing question (see --help)");
    }

    opts.hide_summary  = true;
    opts.prompt_markup = ctx->markup;
    opts.hint          = input_args.hint;
    if (input_args.hide_hint) {
        opts.hint_layout = SC_HINT_HIDDEN;
    }

    bool          answer = false;
    ScInputStatus status = sc_confirm(question, &answer, opts);
    free(question);

    if (status == SC_INPUT_OK && print_reply) {
        puts(answer ? "yes" : "no");
    }
    if (status == SC_INPUT_OK && !answer) {
        return SC_CLI_EXIT_CANCELLED; /* "no" maps to exit 1 */
    }
    return sc_cli_input_exit(status);
}


/* ── input ──────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli input`. */
typedef struct InputArgs {
    ScTextInputOpts opts;
    ScCliInputArgs  input;
    const char     *suggestions[SC_CLI_MAX_SUGGESTIONS];
    size_t          n_suggestions;
} InputArgs;

static const char INPUT_USAGE[] =
    "Usage: sparcli input [options] PROMPT...\n"
    "\n"
    "Prompt for a line of text. The entered value is printed to stdout\n"
    "(UI goes to the terminal), so it works in command substitution:\n"
    "  name=$(sparcli input \"Name:\")\n"
    "\n"
    "Options:\n"
    "  --initial TEXT             Pre-filled value\n"
    "  --placeholder TEXT         Dim placeholder shown when empty\n"
    "  --max N                    Maximum number of characters\n"
    "  --filter FILTER            digits|decimal|alpha|alnum|no-space\n"
    "  --suggest TEXT             Autocomplete suggestion (repeatable)\n"
    "  --boxed                    Draw the field inside a panel\n"
    "  --border STYLE             Box border style (with --boxed)\n"
    "  --width N                  Field width (0 = terminal width)\n"
    SC_CLI_INPUT_USAGE;

int sc_cli_cmd_input(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_INITIAL = SC_CLI_OPT_CMD_BASE,
        OPT_PLACEHOLDER,
        OPT_MAX,
        OPT_FILTER,
        OPT_SUGGEST,
        OPT_BOXED,
        OPT_BORDER,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "initial",     required_argument, NULL, OPT_INITIAL },
        { "placeholder", required_argument, NULL, OPT_PLACEHOLDER },
        { "max",         required_argument, NULL, OPT_MAX },
        { "filter",      required_argument, NULL, OPT_FILTER },
        { "suggest",     required_argument, NULL, OPT_SUGGEST },
        { "boxed",       no_argument,       NULL, OPT_BOXED },
        { "border",      required_argument, NULL, OPT_BORDER },
        { "width",       required_argument, NULL, OPT_WIDTH },
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    InputArgs args = { 0 };
    int       opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_INITIAL:
            args.opts.initial = optarg;
            break;
        case OPT_PLACEHOLDER:
            args.opts.placeholder = optarg;
            break;
        case OPT_MAX:
            if (!sc_cli_parse_int(optarg, &args.opts.max_chars)) {
                return sc_cli_error(ctx, "invalid maximum '%s'", optarg);
            }
            break;
        case OPT_FILTER:
            if (!sc_cli_parse_filter(optarg, &args.opts.char_filter)) {
                return sc_cli_error(ctx, "invalid filter '%s'", optarg);
            }
            break;
        case OPT_SUGGEST:
            if (args.n_suggestions < SC_CLI_MAX_SUGGESTIONS) {
                args.suggestions[args.n_suggestions] = optarg;
                args.n_suggestions++;
            }
            break;
        case OPT_BOXED:
            args.opts.boxed = true;
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &args.opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(INPUT_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &args.input)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    char *prompt = sc_cli_content(argc - optind, argv + optind);
    if (prompt == NULL || prompt[0] == '\0') {
        free(prompt);
        return sc_cli_error(ctx, "missing prompt (see --help)");
    }

    args.opts.hide_summary  = true;
    args.opts.prompt_markup = ctx->markup;
    args.opts.hint          = args.input.hint;
    if (args.input.hide_hint) {
        args.opts.hint_layout = SC_HINT_HIDDEN;
    }
    if (args.n_suggestions > 0) {
        args.opts.suggestions   = args.suggestions;
        args.opts.n_suggestions = args.n_suggestions;
    }

    char         *value  = NULL;
    ScInputStatus status = sc_text_input(prompt, &value, args.opts);
    free(prompt);

    return finish_string_result(status, value);
}


/* ── password ───────────────────────────────────────────────────────────── */

static const char PASSWORD_USAGE[] =
    "Usage: sparcli password [options] PROMPT...\n"
    "\n"
    "Prompt for a masked secret and print it to stdout:\n"
    "  secret=$(sparcli password \"API key:\")\n"
    "\n"
    "Options:\n"
    "  --mask STR                 Mask character (default '*'; '' hides\n"
    "                             the length entirely)\n"
    "  --max N                    Maximum number of characters\n"
    "  --boxed                    Draw the field inside a panel\n"
    "  --border STYLE             Box border style (with --boxed)\n"
    "  --width N                  Field width (0 = terminal width)\n"
    SC_CLI_INPUT_USAGE;

int sc_cli_cmd_password(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_MASK = SC_CLI_OPT_CMD_BASE,
        OPT_MAX,
        OPT_BOXED,
        OPT_BORDER,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "mask",   required_argument, NULL, OPT_MASK },
        { "max",    required_argument, NULL, OPT_MAX },
        { "boxed",  no_argument,       NULL, OPT_BOXED },
        { "border", required_argument, NULL, OPT_BORDER },
        { "width",  required_argument, NULL, OPT_WIDTH },
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    ScPasswordOpts opts       = { 0 };
    ScCliInputArgs input_args = { 0 };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_MASK:
            opts.mask = optarg;
            break;
        case OPT_MAX:
            if (!sc_cli_parse_int(optarg, &opts.max_chars)) {
                return sc_cli_error(ctx, "invalid maximum '%s'", optarg);
            }
            break;
        case OPT_BOXED:
            opts.boxed = true;
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(PASSWORD_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &input_args)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    char *prompt = sc_cli_content(argc - optind, argv + optind);
    if (prompt == NULL || prompt[0] == '\0') {
        free(prompt);
        return sc_cli_error(ctx, "missing prompt (see --help)");
    }

    opts.hide_summary  = true;
    opts.prompt_markup = ctx->markup;
    opts.hint          = input_args.hint;
    if (input_args.hide_hint) {
        opts.hint_layout = SC_HINT_HIDDEN;
    }

    char         *value  = NULL;
    ScInputStatus status = sc_password_input(prompt, &value, opts);
    free(prompt);

    return finish_string_result(status, value);
}


/* ── number ─────────────────────────────────────────────────────────────── */

static const char NUMBER_USAGE[] =
    "Usage: sparcli number [options] PROMPT...\n"
    "\n"
    "Prompt for a number (up/down arrows step the value) and print the\n"
    "exact entered value to stdout:\n"
    "  amount=$(sparcli number \"Amount:\" --decimals 2)\n"
    "\n"
    "Options:\n"
    "  --initial N                Initial value\n"
    "  --start-empty              Start with an empty field\n"
    "  --min N                    Minimum value\n"
    "  --max N                    Maximum value\n"
    "  --step N                   Up/down arrow step size\n"
    "  --decimals N               Number of decimal places\n"
    "  --decimal-sep CHAR         Decimal separator shown ('.' or ',')\n"
    "  --boxed                    Draw the field inside a panel\n"
    "  --border STYLE             Box border style (with --boxed)\n"
    "  --width N                  Field width (0 = terminal width)\n"
    SC_CLI_INPUT_USAGE;

int sc_cli_cmd_number(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_INITIAL = SC_CLI_OPT_CMD_BASE,
        OPT_START_EMPTY,
        OPT_MIN,
        OPT_MAX,
        OPT_STEP,
        OPT_DECIMALS,
        OPT_DECIMAL_SEP,
        OPT_BOXED,
        OPT_BORDER,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "initial",     required_argument, NULL, OPT_INITIAL },
        { "start-empty", no_argument,       NULL, OPT_START_EMPTY },
        { "min",         required_argument, NULL, OPT_MIN },
        { "max",         required_argument, NULL, OPT_MAX },
        { "step",        required_argument, NULL, OPT_STEP },
        { "decimals",    required_argument, NULL, OPT_DECIMALS },
        { "decimal-sep", required_argument, NULL, OPT_DECIMAL_SEP },
        { "boxed",       no_argument,       NULL, OPT_BOXED },
        { "border",      required_argument, NULL, OPT_BORDER },
        { "width",       required_argument, NULL, OPT_WIDTH },
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    ScNumberOpts   opts       = { 0 };
    ScCliInputArgs input_args = { 0 };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_INITIAL:
            if (!sc_cli_parse_double(optarg, &opts.initial)) {
                return sc_cli_error(ctx, "invalid number '%s'", optarg);
            }
            break;
        case OPT_START_EMPTY:
            opts.start_empty = true;
            break;
        case OPT_MIN:
            if (!sc_cli_parse_double(optarg, &opts.min)) {
                return sc_cli_error(ctx, "invalid number '%s'", optarg);
            }
            break;
        case OPT_MAX:
            if (!sc_cli_parse_double(optarg, &opts.max)) {
                return sc_cli_error(ctx, "invalid number '%s'", optarg);
            }
            break;
        case OPT_STEP:
            if (!sc_cli_parse_double(optarg, &opts.step)) {
                return sc_cli_error(ctx, "invalid number '%s'", optarg);
            }
            break;
        case OPT_DECIMALS:
            if (!sc_cli_parse_int(optarg, &opts.decimals)) {
                return sc_cli_error(ctx, "invalid decimals '%s'", optarg);
            }
            break;
        case OPT_DECIMAL_SEP:
            opts.decimal_sep = optarg[0];
            break;
        case OPT_BOXED:
            opts.boxed = true;
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(NUMBER_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &input_args)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    char *prompt = sc_cli_content(argc - optind, argv + optind);
    if (prompt == NULL || prompt[0] == '\0') {
        free(prompt);
        return sc_cli_error(ctx, "missing prompt (see --help)");
    }

    /* out_text receives the exact value as typed/clamped, so scripts get
       a precise decimal string instead of a float round-trip. */
    char *value_text   = NULL;
    opts.out_text      = &value_text;
    opts.hide_summary  = true;
    opts.prompt_markup = ctx->markup;
    opts.hint          = input_args.hint;
    if (input_args.hide_hint) {
        opts.hint_layout = SC_HINT_HIDDEN;
    }

    double        value  = 0.0;
    ScInputStatus status = sc_number_input(prompt, &value, opts);
    free(prompt);

    if (status == SC_INPUT_OK && value_text == NULL) {
        /* Fallback: format the double when no exact text is available. */
        printf("%g\n", value);
        return SC_CLI_EXIT_OK;
    }
    return finish_string_result(status, value_text);
}


/* ── textarea ───────────────────────────────────────────────────────────── */

static const char TEXTAREA_USAGE[] =
    "Usage: sparcli textarea [options] PROMPT...\n"
    "\n"
    "Prompt for multi-line text (Enter inserts a newline, Ctrl-D\n"
    "submits) and print it to stdout.\n"
    "\n"
    "Options:\n"
    "  --initial TEXT             Pre-filled value\n"
    "  --placeholder TEXT         Dim placeholder shown when empty\n"
    "  --external-editor          Allow editing in $VISUAL/$EDITOR (Ctrl-G)\n"
    "  --editor CMD               External editor command\n"
    "  --boxed                    Draw the editor inside a panel\n"
    "  --border STYLE             Box border style (with --boxed)\n"
    "  --width N                  Editor width (0 = terminal width)\n"
    SC_CLI_INPUT_USAGE;

int sc_cli_cmd_textarea(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_INITIAL = SC_CLI_OPT_CMD_BASE,
        OPT_PLACEHOLDER,
        OPT_EXTERNAL_EDITOR,
        OPT_EDITOR,
        OPT_BOXED,
        OPT_BORDER,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "initial",         required_argument, NULL, OPT_INITIAL },
        { "placeholder",     required_argument, NULL, OPT_PLACEHOLDER },
        { "external-editor", no_argument,       NULL, OPT_EXTERNAL_EDITOR },
        { "editor",          required_argument, NULL, OPT_EDITOR },
        { "boxed",           no_argument,       NULL, OPT_BOXED },
        { "border",          required_argument, NULL, OPT_BORDER },
        { "width",           required_argument, NULL, OPT_WIDTH },
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    ScTextareaOpts opts       = { 0 };
    ScCliInputArgs input_args = { 0 };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_INITIAL:
            opts.initial = optarg;
            break;
        case OPT_PLACEHOLDER:
            opts.placeholder = optarg;
            break;
        case OPT_EXTERNAL_EDITOR:
            opts.external_editor = true;
            break;
        case OPT_EDITOR:
            opts.editor          = optarg;
            opts.external_editor = true;
            break;
        case OPT_BOXED:
            opts.boxed = true;
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(TEXTAREA_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &input_args)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    char *prompt = sc_cli_content(argc - optind, argv + optind);
    if (prompt == NULL || prompt[0] == '\0') {
        free(prompt);
        return sc_cli_error(ctx, "missing prompt (see --help)");
    }

    opts.hide_summary  = true;
    opts.prompt_markup = ctx->markup;
    opts.hint          = input_args.hint;
    if (input_args.hide_hint) {
        opts.hint_layout = SC_HINT_HIDDEN;
    }

    char         *value  = NULL;
    ScInputStatus status = sc_textarea(prompt, &value, opts);
    free(prompt);

    return finish_string_result(status, value);
}


/* ── shared result handling ─────────────────────────────────────────────── */

static int finish_string_result(ScInputStatus status, char *value) {
    if (status == SC_INPUT_OK && value != NULL) {
        printf("%s\n", value);
    }
    free(value);
    return sc_cli_input_exit(status);
}
