/**
 * @file cli_common.h
 * Shared context, exit codes and helpers for the sparcli CLI subcommands.
 */
#pragma once

#include "sparcli.h"
#include "platform/sc_compat.h"

#include <stdbool.h>
#include <stdio.h>

/** Element count of a fixed-size array. */
#ifndef SC_CLI_TABLE_SIZE
#define SC_CLI_TABLE_SIZE(table) (sizeof(table) / sizeof((table)[0]))
#endif

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
    SC_CLI_EXIT_BACK      = 3, /**< `--arrow-nav` back key (Left) pressed. */
};

/**
 * `getopt_long` values for long-only options shared by every subcommand.
 *
 * Command-specific long options must use values >= `SC_CLI_OPT_CMD_BASE`
 * so they never collide with these.
 */
enum {
    SC_CLI_OPT_NO_MARKUP   = 900,  /**< `--no-markup` */
    SC_CLI_OPT_NO_COLOR    = 901,  /**< `--no-color` */
    SC_CLI_OPT_HELP        = 902,  /**< `--help` */
    SC_CLI_OPT_ALLOW_ANSI  = 903,  /**< `--allow-ansi` */
    SC_CLI_OPT_STYLE       = 904,  /**< `--style ELEMENT=SPEC` (all commands) */
    SC_CLI_OPT_ACCENT      = 910,  /**< `--accent` (input commands) */
    SC_CLI_OPT_HINT        = 911,  /**< `--hint` (input commands) */
    SC_CLI_OPT_NO_HINT     = 912,  /**< `--no-hint` (input commands) */
    SC_CLI_OPT_HINT_LAYOUT = 913,  /**< `--hint-layout` (input commands) */
    SC_CLI_OPT_HINT_POS    = 914,  /**< `--hint-pos` (input commands) */
    SC_CLI_OPT_BOXED       = 920,  /**< `--boxed` (boxed input widgets) */
    SC_CLI_OPT_BORDER      = 921,  /**< `--border` (boxed input widgets) */
    SC_CLI_OPT_BORDER_COLOR = 922, /**< `--border-color` (boxed input) */
    SC_CLI_OPT_BORDER_BG   = 923,  /**< `--border-bg` (boxed input) */
    SC_CLI_OPT_WIDTH       = 924,  /**< `--width` (boxed input widgets) */
    SC_CLI_OPT_BG          = 925,  /**< `--bg` (content background) */
    SC_CLI_OPT_PADDING     = 926,  /**< `--padding` (inner padding) */
    SC_CLI_OPT_MARGIN      = 927,  /**< `--margin` (outer margin) */
    SC_CLI_OPT_MIN_WIDTH   = 928,  /**< `--min-width` (content clamp) */
    SC_CLI_OPT_MAX_WIDTH   = 929,  /**< `--max-width` (content clamp) */
    SC_CLI_OPT_BG_EXTENT   = 930,  /**< `--bg-extent` (text | widget) */
    SC_CLI_OPT_CMD_BASE    = 1000, /**< First command-specific option value. */
};

/**
 * Long-option table entries every subcommand appends to its own table.
 * Must appear before the terminating `{ 0 }` entry.
 */
#define SC_CLI_COMMON_LONGOPTS                                          \
    { "no-markup",  no_argument,       NULL, SC_CLI_OPT_NO_MARKUP },     \
    { "no-color",   no_argument,       NULL, SC_CLI_OPT_NO_COLOR },      \
    { "allow-ansi", no_argument,       NULL, SC_CLI_OPT_ALLOW_ANSI },    \
    { "style",      required_argument, NULL, SC_CLI_OPT_STYLE },         \
    { "help",       no_argument,       NULL, SC_CLI_OPT_HELP }

/** Usage-text lines for the common options (appended to every usage). */
#define SC_CLI_COMMON_USAGE                                            \
    "  --style ELEM=SPEC          Style an element (e.g. 'bold red on\n" \
    "                             white'); repeatable, see below\n"    \
    "  --no-markup                Treat [tag] markup as literal\n"     \
    "  --no-color                 Strip ANSI colors from output\n"     \
    "  --allow-ansi               Pass ANSI escapes in input through\n" \
    "  --help                     Show this help\n"

/**
 * Long-option table entries shared by every interactive input command
 * (in addition to `SC_CLI_COMMON_LONGOPTS`, which is included here).
 */
#define SC_CLI_INPUT_LONGOPTS                                            \
    { "accent",      required_argument, NULL, SC_CLI_OPT_ACCENT },       \
    { "hint",        required_argument, NULL, SC_CLI_OPT_HINT },         \
    { "no-hint",     no_argument,       NULL, SC_CLI_OPT_NO_HINT },      \
    { "hint-layout", required_argument, NULL, SC_CLI_OPT_HINT_LAYOUT },  \
    { "hint-pos",    required_argument, NULL, SC_CLI_OPT_HINT_POS },     \
    SC_CLI_COMMON_LONGOPTS

/**
 * Long-option entries for the "boxed widget" block shared by `input`,
 * `password`, `number` and `textarea`. The commands handle the option values
 * with explicit cases (precise error messages).
 */
#define SC_CLI_BOX_LONGOPTS                                                  \
    { "boxed",        no_argument,       NULL, SC_CLI_OPT_BOXED },           \
    { "border",       required_argument, NULL, SC_CLI_OPT_BORDER },          \
    { "border-color", required_argument, NULL, SC_CLI_OPT_BORDER_COLOR },    \
    { "border-bg",    required_argument, NULL, SC_CLI_OPT_BORDER_BG },       \
    { "bg",           required_argument, NULL, SC_CLI_OPT_BG },              \
    { "padding",      required_argument, NULL, SC_CLI_OPT_PADDING },         \
    { "margin",       required_argument, NULL, SC_CLI_OPT_MARGIN },          \
    { "min-width",    required_argument, NULL, SC_CLI_OPT_MIN_WIDTH },       \
    { "max-width",    required_argument, NULL, SC_CLI_OPT_MAX_WIDTH },       \
    { "bg-extent",    required_argument, NULL, SC_CLI_OPT_BG_EXTENT },       \
    { "width",        required_argument, NULL, SC_CLI_OPT_WIDTH }

/** Usage-text lines for the boxed-widget block. */
#define SC_CLI_BOX_USAGE                                                     \
    "  --boxed                    Draw the widget inside a panel\n"          \
    "  --border STYLE             Box border style\n"                        \
    "  --border-color COLOR       Box border color\n"                        \
    "  --border-bg COLOR          Box border background\n"                   \
    "  --bg COLOR                 Content background color\n"                \
    "  --padding EDGES            Inner padding, CSS order\n"                \
    "  --margin EDGES             Outer margin, CSS order\n"                 \
    "  --width content|full|N     Width mode (lists default 'content')\n"    \
    "  --min-width N              Min content width (--width content)\n"     \
    "  --max-width N              Max content width (--width content)\n"     \
    "  --bg-extent text|widget    Background reach (default 'widget')\n"

/** Usage-text lines for the shared input options. */
#define SC_CLI_INPUT_USAGE                                                  \
    "  --accent COLOR             Accent color of the widget\n"             \
    "  --hint TEXT                Custom key-hint footer text\n"            \
    "  --no-hint                  Hide the key-hint footer\n"               \
    "  --hint-layout LAYOUT       Hint layout: inline|stacked|hidden\n"     \
    "  --hint-pos POS             Hint position: top|bottom|left|right\n"   \
    SC_CLI_COMMON_USAGE

/** Parsed values of the shared input options. */
typedef struct ScCliInputArgs {
    const char    *hint;          /**< Custom hint footer (`NULL` = default). */
    bool           hide_hint;     /**< Hide the hint footer entirely. */
    ScHintLayout   hint_layout;   /**< `--hint-layout` (0 = unset = default). */
    ScHintPosition hint_pos;      /**< `--hint-pos` (0 = unset = default). */
} ScCliInputArgs;

/**
 * Maps a `--style ELEMENT=SPEC` element name to the `ScTextStyle` field it
 * sets. A command builds a small table of these over its own opts struct.
 */
typedef struct ScCliStyleSlot {
    const char  *name;  /**< Element name accepted on the command line. */
    ScTextStyle *field; /**< Style field the spec is parsed into. */
} ScCliStyleSlot;

/** Collects the raw `--style ELEMENT=SPEC` arguments seen while parsing. */
typedef struct ScCliStyleArgs {
    const char *raw[32]; /**< Each entry is an `ELEMENT=SPEC` optarg. */
    size_t      count;   /**< Number of entries (extras are ignored). */
} ScCliStyleArgs;

/** Global options shared by every subcommand. */
typedef struct ScCliCtx {
    bool        markup;     /**< Parse `[tag]` markup in text arguments. */
    bool        color;      /**< Emit ANSI colors (false = strip them). */
    bool        allow_ansi; /**< Pass ANSI escapes in input text through. */
    const char *prog;       /**< Program name for error messages. */
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
    SC_ATTR_FORMAT(2, 3);

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
 * Handles a boxed-widget option value (`SC_CLI_OPT_BOXED` … `SC_CLI_OPT_MARGIN`)
 * by writing it into `box`: `--boxed` enables the frame; the others set the
 * border/background/padding/margin/width (they take effect only once the frame
 * is enabled). On a bad value it reports the error via `sc_cli_error` and
 * returns `false`. Shared by every boxed input command.
 *
 * @param ctx    CLI context (for error messages).
 * @param opt    Option value returned by `getopt_long` (a box option).
 * @param value  The option argument (`NULL` for `--boxed`).
 * @param box    Box style to update.
 * @return       `true` on success, `false` on a reported parse error.
 */
bool sc_cli_box_opt(
    const ScCliCtx *ctx, int opt, const char *value, ScBoxStyle *box
);

/**
 * Records a `--style ELEMENT=SPEC` argument. Called from a command's option
 * loop for `SC_CLI_OPT_STYLE`; the values are resolved later by
 * `sc_cli_apply_styles`.
 *
 * @param styles  Collector (zero-initialized before the loop).
 * @param arg     The option argument (`ELEMENT=SPEC`); ignored when full.
 */
void sc_cli_style_collect(ScCliStyleArgs *styles, const char *arg);

/**
 * Resolves every collected `--style` argument against `slots`: splits
 * `ELEMENT=SPEC`, finds the matching slot by name, and parses the spec into
 * its style field (`sc_cli_parse_style`).
 *
 * @param ctx       CLI context (for error messages).
 * @param styles    Collected `--style` arguments.
 * @param slots     Element-name → style-field table.
 * @param n_slots   Number of entries in `slots`.
 * @return          `true` on success; on a missing `=`, unknown element name
 *                  or bad spec it prints an error and returns `false`.
 */
bool sc_cli_apply_styles(const ScCliCtx *ctx, const ScCliStyleArgs *styles,
                         const ScCliStyleSlot *slots, size_t n_slots);

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
