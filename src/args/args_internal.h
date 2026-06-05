#pragma once

#include "sparcli.h"

#include <stdbool.h>
#include <stddef.h>


/**
 * @file args_internal.h
 * @brief Private structures and cross-file helpers of the args module.
 *
 * The module is split by concern: `args.c` (builder/storage/getters),
 * `args_value.c` (typed value parsing), `args_suggest.c` (did-you-mean),
 * `args_parse.c` (parse loop + error reporting), `args_help.c` (help
 * renderer) and `args_complete.c` (zsh completion).
 */


/** One named option (flag or value option) on a command. */
typedef struct ArgOption {
    char      *long_name;      /**< Without `--`; owned. */
    char       short_name;     /**< Single-char alias; `0` = none. */
    ScArgType  type;           /**< Value type (ignored for flags). */
    bool       is_flag;        /**< `true` = boolean flag, no value. */
    char      *metavar;        /**< Help placeholder; owned; `NULL` for flags. */
    char      *help;           /**< Help text; owned. */
    char      *default_value;  /**< Owned; `NULL` = no default. */
    char     **choices;        /**< Owned array of owned strings; `NULL` = any. */
    size_t     choice_count;   /**< Number of choices. */
    bool       required;       /**< Parse fails when absent. */

    /* Parse results */
    bool       present;        /**< Seen on the command line. */
    char      *value_text;     /**< Matched raw value; owned. */
} ArgOption;

/** One positional argument slot on a command. */
typedef struct ArgPositional {
    char      *name;           /**< Display + lookup name; owned. */
    ScArgType  type;           /**< Value type. */
    char      *help;           /**< Help text; owned. */
    bool       required;       /**< Parse fails when unfilled. */
    bool       variadic;       /**< Collects every remaining token. */

    /* Parse results */
    char     **values;         /**< Owned array of owned strings. */
    size_t     value_count;    /**< Number of collected values. */
    size_t     value_capacity; /**< Allocated slots. */
} ArgPositional;

/** A command node: the root or a (nested) subcommand. */
struct ScArgsCmd {
    char *name;                /**< Owned; the prog name for the root. */
    char *about;               /**< Owned. */
    char *group;               /**< Help section heading; owned; `NULL` = default. */

    struct ScArgsCmd  *parent;        /**< `NULL` for the root. */
    struct ScArgsCmd **subcommands;   /**< Owned array of owned nodes. */
    size_t             subcommand_count;
    size_t             subcommand_capacity;

    ArgOption *options;
    size_t     option_count;
    size_t     option_capacity;

    ArgPositional *positionals;
    size_t         positional_count;
    size_t         positional_capacity;

    void *user_data;           /**< Borrowed; set/read via
                                    sc_args_cmd_{set_,}userdata. Never freed. */

    struct ScArgs *owner;      /**< Back-pointer for prog/version/ansi. */
};

/** The parser: metadata + root command + parse result. */
struct ScArgs {
    char      *prog;           /**< Owned. */
    char      *version;        /**< Owned; `NULL` = no `--version`. */
    char      *about;          /**< Owned. */
    ScAnsiMode ansi;           /**< Help/error rendering passthrough. */

    ScArgsCmd        root;     /**< Embedded root command. */
    const ScArgsCmd *matched;  /**< Parse result; `NULL` before parsing. */
};


/* ── args.c: tree lookups shared with the other files ──────────────────── */

/** Finds an option by long name on one command (not its ancestors). */
ArgOption *sc_args_find_option(const ScArgsCmd *cmd, const char *long_name);

/** Finds an option by long name on `leaf` and its ancestors (leaf wins). */
ArgOption *sc_args_find_option_chain(
    const ScArgsCmd *leaf, const char *long_name
);

/** Finds an option by short name on `leaf` and its ancestors. */
ArgOption *sc_args_find_option_short_chain(
    const ScArgsCmd *leaf, char short_name
);

/** Finds a direct subcommand by name. */
ScArgsCmd *sc_args_find_subcommand(const ScArgsCmd *cmd, const char *name);

/** Finds a positional by name on `leaf` and its ancestors. */
ArgPositional *sc_args_find_positional_chain(
    const ScArgsCmd *leaf, const char *name
);

/** Appends one collected value to a positional. Returns success. */
bool sc_args_positional_add_value(ArgPositional *positional, const char *value);

/** Stores the matched value text of an option (replacing any previous). */
bool sc_args_option_set_value(ArgOption *option, const char *value);


/* ── args_value.c: typed value parsing ──────────────────────────────────── */

/** Parses a full-string integer. */
bool sc_args_parse_int(const char *text, long *out);

/** Parses a full-string double. */
bool sc_args_parse_double(const char *text, double *out);

/** Parses a color: name, `#RRGGBB`, or `R,G,B`. */
bool sc_args_parse_color(const char *text, ScColor *out);

/** Index of `text` in the option's choices; `-1` = not found / no choices. */
int sc_args_find_choice(const ArgOption *option, const char *text);

/**
 * Validates `text` against the option's type and choices. On failure
 * writes a short human-readable reason into `reason` (size `reason_size`).
 */
bool sc_args_validate_value(
    const ArgOption *option, const char *text,
    char *reason, size_t reason_size
);


/* ── args_suggest.c: did-you-mean ───────────────────────────────────────── */

/** Levenshtein edit distance between two strings. */
size_t sc_args_levenshtein(const char *a, const char *b);

/** Closest long-option name in the chain; `NULL` = nothing close enough. */
const char *sc_args_suggest_option(const ScArgsCmd *leaf, const char *name);

/** Closest subcommand name under `cmd`; `NULL` = nothing close enough. */
const char *sc_args_suggest_subcommand(const ScArgsCmd *cmd, const char *name);


/* ── args_help.c: help / version rendering ──────────────────────────────── */

/** Renders the help screen for `cmd` to the current output stream. */
void sc_args_render_help(const ScArgs *args, const ScArgsCmd *cmd);

/** Renders the `prog version` line to the current output stream. */
void sc_args_render_version(const ScArgs *args);
