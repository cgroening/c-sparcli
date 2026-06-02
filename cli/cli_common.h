/**
 * @file cli_common.h
 * Shared context, exit codes and helpers for the sparcli CLI subcommands.
 */
#pragma once

#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>

/**
 * Process exit codes shared by all subcommands.
 *
 * Output commands use OK/ERROR; input commands additionally use CANCELLED
 * so shell scripts can distinguish "user said no / pressed Esc" (1) from
 * "something went wrong / no TTY" (2).
 */
enum {
    SC_CLI_EXIT_OK        = 0, /**< Success / confirmed. */
    SC_CLI_EXIT_CANCELLED = 1, /**< Input cancelled or answered "no". */
    SC_CLI_EXIT_ERROR     = 2, /**< Usage error, bad data or no TTY. */
};

/**
 * `getopt_long` values for long-only options shared by every subcommand.
 *
 * Command-specific long options must use values >= `SC_CLI_OPT_CMD_BASE`
 * so they never collide with these.
 */
enum {
    SC_CLI_OPT_NO_MARKUP = 900,  /**< `--no-markup` */
    SC_CLI_OPT_NO_COLOR  = 901,  /**< `--no-color` */
    SC_CLI_OPT_HELP      = 902,  /**< `--help` */
    SC_CLI_OPT_ACCENT    = 910,  /**< `--accent` (input commands) */
    SC_CLI_OPT_HINT      = 911,  /**< `--hint` (input commands) */
    SC_CLI_OPT_NO_HINT   = 912,  /**< `--no-hint` (input commands) */
    SC_CLI_OPT_CMD_BASE  = 1000, /**< First command-specific option value. */
};

/**
 * Long-option table entries every subcommand appends to its own table.
 * Must appear before the terminating `{ 0 }` entry.
 */
#define SC_CLI_COMMON_LONGOPTS                                     \
    { "no-markup", no_argument, NULL, SC_CLI_OPT_NO_MARKUP },      \
    { "no-color",  no_argument, NULL, SC_CLI_OPT_NO_COLOR },       \
    { "help",      no_argument, NULL, SC_CLI_OPT_HELP }

/** Usage-text lines for the common options (appended to every usage). */
#define SC_CLI_COMMON_USAGE                                          \
    "  --no-markup                Treat [tag] markup as literal\n"   \
    "  --no-color                 Strip ANSI colors from output\n"   \
    "  --help                     Show this help\n"

/**
 * Long-option table entries shared by every interactive input command
 * (in addition to `SC_CLI_COMMON_LONGOPTS`, which is included here).
 */
#define SC_CLI_INPUT_LONGOPTS                                      \
    { "accent",  required_argument, NULL, SC_CLI_OPT_ACCENT },     \
    { "hint",    required_argument, NULL, SC_CLI_OPT_HINT },       \
    { "no-hint", no_argument,       NULL, SC_CLI_OPT_NO_HINT },    \
    SC_CLI_COMMON_LONGOPTS

/** Usage-text lines for the shared input options. */
#define SC_CLI_INPUT_USAGE                                            \
    "  --accent COLOR             Accent color of the widget\n"       \
    "  --hint TEXT                Custom key-hint footer text\n"      \
    "  --no-hint                  Hide the key-hint footer\n"         \
    SC_CLI_COMMON_USAGE

/** Parsed values of the shared input options. */
typedef struct ScCliInputArgs {
    const char *hint;      /**< Custom hint footer (`NULL` = default). */
    bool        hide_hint; /**< Hide the hint footer entirely. */
} ScCliInputArgs;

/** Global options shared by every subcommand. */
typedef struct ScCliCtx {
    bool        markup; /**< Parse `[tag]` markup in text arguments. */
    bool        color;  /**< Emit ANSI colors (false = strip them). */
    const char *prog;   /**< Program name for error messages. */
} ScCliCtx;

/** Signature of a subcommand implementation. */
typedef int (*ScCliCmdFn)(ScCliCtx *ctx, int argc, char **argv);

/**
 * Returns a context initialized with defaults: markup on, color on unless
 * the `NO_COLOR` environment variable is set (https://no-color.org).
 *
 * @param prog  Program name used as prefix in error messages.
 * @return      Initialized context.
 */
ScCliCtx sc_cli_ctx_init(const char *prog);

/**
 * Prints `prog: <message>` to stderr and returns `SC_CLI_EXIT_ERROR`,
 * so commands can write `return sc_cli_error(ctx, "...", ...);`.
 *
 * @param ctx  CLI context (for the program name).
 * @param fmt  printf-style format string (never pass user input here).
 * @return     Always `SC_CLI_EXIT_ERROR`.
 */
int sc_cli_error(const ScCliCtx *ctx, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

/**
 * Handles a common option value returned by `getopt_long`.
 *
 * Updates `ctx` for `--no-markup` / `--no-color`. `--help` and unknown
 * options are NOT handled here (they need command-specific behavior).
 *
 * @param ctx  CLI context to update.
 * @param opt  Option value returned by `getopt_long`.
 * @return     `true` when the option was consumed.
 */
bool sc_cli_common_opt(ScCliCtx *ctx, int opt);

/**
 * Handles a shared input option value returned by `getopt_long`.
 *
 * `--accent` is applied immediately as the process-wide input theme accent
 * (so it works for every widget); `--hint` / `--no-hint` are stored in
 * `args` for the command to wire into its opts. Falls back to
 * `sc_cli_common_opt` for the common options.
 *
 * @param ctx   CLI context.
 * @param opt   Option value returned by `getopt_long`.
 * @param args  Receives hint settings.
 * @return      `true` when the option was consumed.
 */
bool sc_cli_input_opt(ScCliCtx *ctx, int opt, ScCliInputArgs *args);

/**
 * Maps a widget's `ScInputStatus` to the CLI exit code convention
 * (OK = 0, CANCELLED = 1, ERROR = 2).
 *
 * @param status  Status returned by an input widget.
 * @return        Matching `SC_CLI_EXIT_*` code.
 */
int sc_cli_input_exit(ScInputStatus status);

/**
 * Builds an `ScText` from `arg`: markup-parsed when enabled in `ctx`,
 * a single plain span otherwise.
 *
 * @param ctx  CLI context (markup flag).
 * @param arg  Source text; must not be `NULL`.
 * @return     New text; caller frees with `sc_text_free`. `NULL` only on
 *             allocation failure.
 */
ScText *sc_cli_text(const ScCliCtx *ctx, const char *arg);

/**
 * Returns a command's text content: the positional arguments joined with
 * single spaces, or - when there are none - the whole of stdin (with one
 * trailing newline stripped).
 *
 * @param argc  Number of positional arguments.
 * @param argv  Positional arguments (no command name, no options).
 * @return      Heap string (caller frees), or `NULL` on read/alloc failure.
 */
char *sc_cli_content(int argc, char **argv);

/**
 * Starts capturing library output into a temp stream when colors are
 * disabled, so it can be ANSI-stripped afterwards.
 *
 * @param ctx  CLI context (color flag).
 * @return     The capture stream, or `NULL` when no capture is needed
 *             (colors enabled) - in that case output goes to stdout.
 */
FILE *sc_cli_capture_begin(const ScCliCtx *ctx);

/**
 * Ends a capture started by `sc_cli_capture_begin`: strips all ANSI escape
 * sequences from the captured bytes and writes the result to stdout.
 *
 * @param capture  Stream returned by `sc_cli_capture_begin`; `NULL` is a
 *                 no-op returning `SC_CLI_EXIT_OK`.
 * @return         `SC_CLI_EXIT_OK`, or `SC_CLI_EXIT_ERROR` on alloc/IO
 *                 failure.
 */
int sc_cli_capture_end(FILE *capture);
