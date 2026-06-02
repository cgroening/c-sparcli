#pragma once

#include "core/sparcli_core.h"
#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_args.h
 * @brief Declarative argument parser: subcommands, typed options,
 *        auto-generated help, did-you-mean and zsh completion.
 *
 * A builder-style parser ("clap for C"): describe the command tree once,
 * then `sc_args_parse` handles `--help`/`--version`, validation, error
 * reporting (pretty errors with suggestions) and value conversion.
 *
 * @code
 * ScArgs *args = sc_args_new((ScArgsOpts){
 *     .prog = "mytool", .version = "1.0.0",
 *     .about = "Builds and cleans projects",
 * });
 * ScArgsCmd *root = sc_args_root(args);
 * sc_args_flag(root, "verbose", 'v', "Verbose output");
 *
 * ScArgsCmd *build = sc_args_subcommand(root, "build", "Build the project");
 * sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
 * sc_args_opt_default(build, "jobs", "4");
 * sc_args_positional(build, "TARGET", SC_ARG_STR, "Build target",
 *                    false, false);
 *
 * ScArgsStatus status;
 * const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
 * if (status != SC_ARGS_MATCHED) {
 *     sc_args_free(args);
 *     return status == SC_ARGS_HANDLED ? 0 : 2;
 * }
 * if (matched == build) {
 *     long jobs = sc_args_get_int(args, "jobs");
 *     // ...
 * }
 * sc_args_free(args);
 * @endcode
 *
 * All builder strings are **copied**; caller buffers only need to live
 * until the call returns. Every argv token is ANSI-sanitized before it is
 * stored or echoed back in error messages.
 */


/** Opaque parser handle (the command tree); create with `sc_args_new`. */
typedef struct ScArgs ScArgs;

/**
 * Opaque command node: the root command or a (nested) subcommand.
 * Borrowed from the tree - never freed by the caller.
 */
typedef struct ScArgsCmd ScArgsCmd;

/** Value type of an option or positional argument. */
typedef enum ScArgType {
    SC_ARG_STR = 0,  /**< Any string (the default). */
    SC_ARG_INT,      /**< Integer (`strtol`, full-string match). */
    SC_ARG_DOUBLE,   /**< Floating point number. */
    SC_ARG_COLOR,    /**< Color name, `#RRGGBB` or `R,G,B`. */
} ScArgType;

/** Outcome of `sc_args_parse`. */
typedef enum ScArgsStatus {
    SC_ARGS_MATCHED = 0,  /**< A command matched; read values and proceed. */
    SC_ARGS_HANDLED,      /**< `--help`/`--version` was printed; exit 0. */
    SC_ARGS_ERROR,        /**< Parse error printed to stderr; exit 2. */
} ScArgsStatus;

/** Parser-wide options for `sc_args_new`. All strings are copied. */
typedef struct ScArgsOpts {
    const char *prog;     /**< Program name (shown in usage/errors). */
    const char *version;  /**< Version string; `NULL` = no auto `--version`. */
    const char *about;    /**< One-line description for the help header. */
    ScAnsiMode  ansi;     /**< ANSI passthrough for help/error rendering. */
} ScArgsOpts;


/* ── Building the command tree ──────────────────────────────────────────── */

/**
 * Allocates a parser with an empty root command.
 *
 * @param opts  Program metadata; strings are copied.
 * @return      Heap-allocated parser; `NULL` on allocation failure.
 *              Free with `sc_args_free`.
 */
SPARCLI_EXPORT ScArgs *sc_args_new(ScArgsOpts opts);

/**
 * Frees the parser, every command node, and all parse results (including
 * the strings returned by the getters).
 *
 * @param args  Parser to free; safe to pass `NULL`.
 */
SPARCLI_EXPORT void sc_args_free(ScArgs *args);

/**
 * Returns the root command node (to attach global options/subcommands).
 *
 * @param args  Parser; returns `NULL` when `NULL`.
 */
SPARCLI_EXPORT ScArgsCmd *sc_args_root(ScArgs *args);

/**
 * Adds a subcommand under `parent` (arbitrary nesting depth).
 *
 * @param parent  Parent command node.
 * @param name    Subcommand name as typed on the command line; copied.
 * @param about   One-line description for help listings; copied.
 * @return        The new (borrowed) command node; `NULL` on failure.
 */
SPARCLI_EXPORT ScArgsCmd *sc_args_subcommand(
    ScArgsCmd *parent, const char *name, const char *about
);

/**
 * Sets the help section heading this command is listed under (e.g.
 * `"Output commands"`). Commands without a group appear under
 * `"Commands"`.
 *
 * @param cmd    Command node; no-op when `NULL`.
 * @param group  Section heading; copied.
 */
SPARCLI_EXPORT void sc_args_cmd_group(ScArgsCmd *cmd, const char *group);

/**
 * Adds a boolean flag (an option that takes no value).
 *
 * `--help`/`-h` (every command) and `--version`/`-V` (root, when a
 * version was set) are reserved and added automatically.
 *
 * @param cmd         Command the flag belongs to.
 * @param long_name   Long name without dashes (`"verbose"` → `--verbose`);
 *                    copied.
 * @param short_name  Single-character alias (`'v'` → `-v`); `0` = none.
 * @param help        Help text; copied.
 */
SPARCLI_EXPORT void sc_args_flag(
    ScArgsCmd *cmd, const char *long_name, char short_name, const char *help
);

/**
 * Adds an option that takes a typed value (`--name VALUE` or
 * `--name=VALUE`).
 *
 * @param cmd         Command the option belongs to.
 * @param long_name   Long name without dashes; copied.
 * @param short_name  Single-character alias; `0` = none.
 * @param type        Value type (validated at parse time).
 * @param metavar     Value placeholder for help (`"FILE"`, `"N"`); copied;
 *                    `NULL` = `"VALUE"`.
 * @param help        Help text; copied.
 */
SPARCLI_EXPORT void sc_args_opt(
    ScArgsCmd *cmd, const char *long_name, char short_name, ScArgType type,
    const char *metavar, const char *help
);

/**
 * Sets the default value of an option (used when absent; shown in help).
 *
 * @param cmd            Command the option was added to.
 * @param long_name      Option to modify.
 * @param default_value  Default as text (parsed like a command-line
 *                       value); copied.
 */
SPARCLI_EXPORT void sc_args_opt_default(
    ScArgsCmd *cmd, const char *long_name, const char *default_value
);

/**
 * Restricts an option to a fixed set of values (rendered in help; invalid
 * values produce an error listing the choices).
 *
 * @param cmd        Command the option was added to.
 * @param long_name  Option to modify.
 * @param choices    `NULL`-terminated array of allowed values; copied.
 */
SPARCLI_EXPORT void sc_args_opt_choices(
    ScArgsCmd *cmd, const char *long_name, const char *const *choices
);

/**
 * Marks an option as required (parse fails when absent).
 *
 * @param cmd        Command the option was added to.
 * @param long_name  Option to modify.
 */
SPARCLI_EXPORT void sc_args_opt_required(
    ScArgsCmd *cmd, const char *long_name
);

/**
 * Adds a positional argument slot. Positionals are filled in declaration
 * order; a variadic positional collects every remaining token and must be
 * the last one.
 *
 * @param cmd       Command the positional belongs to.
 * @param name      Display name (`"FILE"`); also the lookup key; copied.
 * @param type      Value type.
 * @param help      Help text; copied.
 * @param required  Parse fails when no value is supplied.
 * @param variadic  Collects all remaining arguments.
 */
SPARCLI_EXPORT void sc_args_positional(
    ScArgsCmd *cmd, const char *name, ScArgType type, const char *help,
    bool required, bool variadic
);


/* ── Parsing ────────────────────────────────────────────────────────────── */

/**
 * Parses `argv` against the command tree.
 *
 * Handles `--opt value`, `--opt=value`, combined short flags (`-abc`),
 * the `--` terminator, subcommand descent, positionals, `--help` (every
 * level) and `--version` (root). Parse errors are rendered to stderr as
 * pretty errors with did-you-mean suggestions.
 *
 * @param args    Parser.
 * @param argc    Argument count (from `main`).
 * @param argv    Argument vector (from `main`); tokens are sanitized.
 * @param status  Receives the outcome; may be `NULL`.
 * @return        The matched (deepest) command node on success, `NULL`
 *                when help/version was handled or an error occurred.
 */
SPARCLI_EXPORT const ScArgsCmd *sc_args_parse(
    ScArgs *args, int argc, char **argv, ScArgsStatus *status
);


/* ── Reading results (after sc_args_parse) ──────────────────────────────── */

/**
 * Returns the string value of an option or positional, searching the
 * matched command and its ancestors.
 *
 * @param args  Parser (after parse).
 * @param name  Option long name or positional name.
 * @return      Borrowed string (valid until `sc_args_free`); the default
 *              value when absent; `NULL` when never supplied and no
 *              default exists.
 */
SPARCLI_EXPORT const char *sc_args_get_str(const ScArgs *args, const char *name);

/** Integer value of `name` (`0` when absent/unparseable). */
SPARCLI_EXPORT long sc_args_get_int(const ScArgs *args, const char *name);

/** Double value of `name` (`0.0` when absent/unparseable). */
SPARCLI_EXPORT double sc_args_get_double(const ScArgs *args, const char *name);

/** `true` when the flag `name` was given. */
SPARCLI_EXPORT bool sc_args_get_flag(const ScArgs *args, const char *name);

/**
 * Index of the value of `name` within its `choices` array
 * (`-1` = absent or no choices configured).
 */
SPARCLI_EXPORT int sc_args_get_enum(const ScArgs *args, const char *name);

/** Color value of `name` (zero-init "no color" when absent/invalid). */
SPARCLI_EXPORT ScColor sc_args_get_color(const ScArgs *args, const char *name);

/**
 * Values collected by a (variadic) positional.
 *
 * @param args   Parser (after parse).
 * @param name   Positional name.
 * @param count  Receives the number of values; required.
 * @return       Borrowed array of borrowed strings; `NULL` when empty.
 */
SPARCLI_EXPORT const char *const *sc_args_get_many(
    const ScArgs *args, const char *name, size_t *count
);

/** `true` when the option/positional `name` was supplied on the command line. */
SPARCLI_EXPORT bool sc_args_present(const ScArgs *args, const char *name);

/**
 * Name of the matched (deepest) command; the program name when no
 * subcommand was selected. `NULL` before parsing.
 */
SPARCLI_EXPORT const char *sc_args_selected_command(const ScArgs *args);

/** Name of a command node (borrowed). */
SPARCLI_EXPORT const char *sc_args_cmd_name(const ScArgsCmd *cmd);


/* ── Help / completion output ───────────────────────────────────────────── */

/**
 * Renders the help screen for `cmd` (or the root when `NULL`) to the
 * current output stream, using sparcli widgets.
 *
 * @param args  Parser.
 * @param cmd   Command to document; `NULL` = root.
 */
SPARCLI_EXPORT void sc_args_print_help(const ScArgs *args, const ScArgsCmd *cmd);

/**
 * Emits a zsh completion script (`#compdef`) for the whole command tree
 * to the current output stream. Install it as `_<prog>` in `$fpath`.
 *
 * @param args  Parser.
 */
SPARCLI_EXPORT void sc_args_print_zsh_completion(const ScArgs *args);

SPARCLI_END_DECLS
