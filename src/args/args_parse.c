#include "sparcli.h"
#include "args/args_internal.h"
#include "core/sanitize_internal.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Size of the buffers used to compose error messages and hints. */
#define PARSE_MESSAGE_BUFFER 512

/** Exit code suggested by parse errors (matches the CLI convention). */
#define PARSE_ERROR_EXIT_CODE 2


/** Mutable state carried through one parse run. */
typedef struct ParseState {
    ScArgs       *args;              /**< The parser. */
    ScArgsCmd    *current;           /**< Command currently descended into. */
    int           argc;              /**< Argument count. */
    char        **argv;              /**< Raw argument vector. */
    int           index;             /**< Current argv index. */
    size_t        positional_index;  /**< Next positional slot to fill. */
    bool          only_positionals;  /**< `--` seen: no more options. */
    ScArgsStatus  status;            /**< Outcome. */
} ParseState;


// Forward declarations indented to reflect call hierarchy
static bool parse_one_token(ParseState *state, const char *token);
    static bool handle_long_option(ParseState *state, const char *token);
        static bool handle_reserved_long(ParseState *state, const char *name);
        static bool bind_option_value(
            ParseState *state, ArgOption *option, const char *display_name,
            const char *inline_value
        );
    static bool handle_short_options(ParseState *state, const char *token);
        static bool handle_reserved_short(ParseState *state, char short_name);
    static bool handle_bare_token(ParseState *state, const char *token);
        static bool bind_positional(ParseState *state, const char *token);
static bool check_required(ParseState *state);
static char *next_argv_value(ParseState *state);
static bool is_negative_number(const char *token);
static void report_error(
    const ScArgs *args, const char *message, const char *hint
);
static void set_status(ScArgsStatus *out, ScArgsStatus value);


const ScArgsCmd *sc_args_parse(
    ScArgs *args, int argc, char **argv, ScArgsStatus *status
) {
    if (!args || argc < 0 || (argc > 0 && !argv)) {
        set_status(status, SC_ARGS_ERROR);
        return NULL;
    }

    ParseState state = {
        .args = args,
        .current = &args->root,
        .argc = argc,
        .argv = argv,
        .index = 1,
        .status = SC_ARGS_MATCHED,
    };
    // Clear any previous run's results so re-parsing the same tree (e.g.
    // once per REPL line) never sees stale values.
    sc_args_reset(args);

    for (; state.index < state.argc; state.index++) {
        // Every argv token is untrusted input: sanitize before use
        char *token = sc_sanitize_copy(state.argv[state.index], false);
        if (!token) {
            set_status(status, SC_ARGS_ERROR);
            return NULL;
        }

        bool keep_going = parse_one_token(&state, token);
        free(token);
        if (!keep_going) {
            set_status(status, state.status);
            return NULL;
        }
    }

    if (!check_required(&state)) {
        set_status(status, SC_ARGS_ERROR);
        return NULL;
    }

    args->matched = state.current;
    set_status(status, SC_ARGS_MATCHED);
    return state.current;
}

/**
 * Dispatches one sanitized token. Returns `false` to stop parsing (help/
 * version handled or error reported; `state->status` holds the outcome).
 */
static bool parse_one_token(ParseState *state, const char *token) {
    if (state->only_positionals) {
        return bind_positional(state, token);
    }

    if (strcmp(token, "--") == 0) {
        state->only_positionals = true;
        return true;
    }

    if (strncmp(token, "--", 2) == 0) {
        return handle_long_option(state, token);
    }

    bool looks_like_short_option = token[0] == '-' && token[1] != '\0'
        && !is_negative_number(token);
    if (looks_like_short_option) {
        return handle_short_options(state, token);
    }

    return handle_bare_token(state, token);
}

/** Handles `--name`, `--name=value` and `--name value`. */
static bool handle_long_option(ParseState *state, const char *token) {
    // Split an inline "=value" off the option name
    char *name = strdup(token + 2);
    if (!name) {
        state->status = SC_ARGS_ERROR;
        return false;
    }
    char *equals = strchr(name, '=');
    const char *inline_value = NULL;
    if (equals) {
        *equals = '\0';
        inline_value = equals + 1;
    }

    bool keep_going = false;
    char message[PARSE_MESSAGE_BUFFER];
    char hint[PARSE_MESSAGE_BUFFER];

    if (handle_reserved_long(state, name)) {
        free(name);
        return false;   // --help / --version handled
    }

    ArgOption *option = sc_args_find_option_chain(state->current, name);
    if (!option) {
        snprintf(message, sizeof message, "Unknown option '--%s'", name);
        const char *suggestion = sc_args_suggest_option(state->current, name);
        if (suggestion) {
            snprintf(hint, sizeof hint, "Did you mean '--%s'?", suggestion);
        } else {
            snprintf(
                hint, sizeof hint, "Run '%s --help' to see all options",
                state->args->prog
            );
        }
        report_error(state->args, message, hint);
        state->status = SC_ARGS_ERROR;
    } else if (option->is_flag) {
        if (inline_value) {
            snprintf(
                message, sizeof message,
                "Option '--%s' does not take a value", name
            );
            report_error(state->args, message, NULL);
            state->status = SC_ARGS_ERROR;
        } else {
            option->present = true;
            keep_going = true;
        }
    } else {
        keep_going = bind_option_value(state, option, name, inline_value);
    }

    free(name);
    return keep_going;
}

/** Recognizes the reserved `--help` / `--version` long options. */
static bool handle_reserved_long(ParseState *state, const char *name) {
    if (strcmp(name, "help") == 0) {
        sc_args_render_help(state->args, state->current);
        state->status = SC_ARGS_HANDLED;
        return true;
    }
    bool at_root = state->current == &state->args->root;
    if (strcmp(name, "version") == 0 && at_root && state->args->version) {
        sc_args_render_version(state->args);
        state->status = SC_ARGS_HANDLED;
        return true;
    }
    return false;
}

/**
 * Resolves and validates the value of a non-flag option, taking it from
 * the inline `=value` part or the next argv token.
 */
static bool bind_option_value(
    ParseState *state, ArgOption *option, const char *display_name,
    const char *inline_value
) {
    char message[PARSE_MESSAGE_BUFFER];
    char hint[PARSE_MESSAGE_BUFFER];

    char *consumed = NULL;
    const char *value = inline_value;
    if (!value) {
        consumed = next_argv_value(state);
        if (!consumed) {
            snprintf(
                message, sizeof message,
                "Option '--%s' requires a value", display_name
            );
            snprintf(
                hint, sizeof hint, "Usage: --%s <%s>",
                display_name, option->metavar ? option->metavar : "VALUE"
            );
            report_error(state->args, message, hint);
            state->status = SC_ARGS_ERROR;
            return false;
        }
        value = consumed;
    }

    char reason[PARSE_MESSAGE_BUFFER];
    if (!sc_args_validate_value(option, value, reason, sizeof reason)) {
        snprintf(
            message, sizeof message,
            "Invalid value '%s' for option '--%s'", value, display_name
        );
        report_error(state->args, message, reason[0] ? reason : NULL);
        state->status = SC_ARGS_ERROR;
        free(consumed);
        return false;
    }

    bool stored = sc_args_option_set_value(option, value);
    option->present = stored;
    free(consumed);
    if (!stored) {
        state->status = SC_ARGS_ERROR;
        return false;
    }
    return true;
}

/** Handles combined short options (`-v`, `-abc`, `-j4`). */
static bool handle_short_options(ParseState *state, const char *token) {
    char message[PARSE_MESSAGE_BUFFER];

    for (size_t position = 1; token[position]; position++) {
        char short_name = token[position];

        if (handle_reserved_short(state, short_name)) {
            return false;   // -h / -V handled
        }

        ArgOption *option = sc_args_find_option_short_chain(
            state->current, short_name
        );
        if (!option) {
            snprintf(
                message, sizeof message, "Unknown option '-%c'", short_name
            );
            report_error(state->args, message, NULL);
            state->status = SC_ARGS_ERROR;
            return false;
        }

        if (option->is_flag) {
            option->present = true;
            continue;
        }

        // A value-taking option consumes the rest of the token ("-j4")
        // or, when nothing follows, the next argv token ("-j 4")
        const char *rest = token + position + 1;
        const char *inline_value = rest[0] ? rest : NULL;
        return bind_option_value(
            state, option, option->long_name, inline_value
        );
    }
    return true;
}

/** Recognizes the reserved `-h` / `-V` short options. */
static bool handle_reserved_short(ParseState *state, char short_name) {
    if (short_name == 'h'
        && !sc_args_find_option_short_chain(state->current, 'h')) {
        sc_args_render_help(state->args, state->current);
        state->status = SC_ARGS_HANDLED;
        return true;
    }
    bool at_root = state->current == &state->args->root;
    if (short_name == 'V' && at_root && state->args->version
        && !sc_args_find_option_short_chain(state->current, 'V')) {
        sc_args_render_version(state->args);
        state->status = SC_ARGS_HANDLED;
        return true;
    }
    return false;
}

/** A bare token is either a subcommand name or a positional value. */
static bool handle_bare_token(ParseState *state, const char *token) {
    // Subcommands only match before any positional was bound
    if (state->positional_index == 0) {
        ScArgsCmd *subcommand = sc_args_find_subcommand(
            state->current, token
        );
        if (subcommand) {
            state->current = subcommand;
            state->positional_index = 0;
            return true;
        }

        // Commands without positionals: an unknown bare word is a typo
        bool expects_positionals = state->current->positional_count > 0;
        if (!expects_positionals && state->current->subcommand_count > 0) {
            char message[PARSE_MESSAGE_BUFFER];
            char hint[PARSE_MESSAGE_BUFFER];
            snprintf(message, sizeof message, "Unknown command '%s'", token);
            const char *suggestion = sc_args_suggest_subcommand(
                state->current, token
            );
            if (suggestion) {
                snprintf(hint, sizeof hint, "Did you mean '%s'?", suggestion);
            } else {
                snprintf(
                    hint, sizeof hint, "Run '%s --help' to see all commands",
                    state->args->prog
                );
            }
            report_error(state->args, message, hint);
            state->status = SC_ARGS_ERROR;
            return false;
        }
    }

    return bind_positional(state, token);
}

/** Binds a token to the next positional slot (or the variadic collector). */
static bool bind_positional(ParseState *state, const char *token) {
    ScArgsCmd *cmd = state->current;
    char message[PARSE_MESSAGE_BUFFER];

    // Past the last slot: only a trailing variadic positional can absorb it
    if (state->positional_index >= cmd->positional_count) {
        bool has_variadic_tail = cmd->positional_count > 0
            && cmd->positionals[cmd->positional_count - 1].variadic;
        if (!has_variadic_tail) {
            snprintf(message, sizeof message, "Unexpected argument '%s'", token);
            report_error(state->args, message, NULL);
            state->status = SC_ARGS_ERROR;
            return false;
        }
        state->positional_index = cmd->positional_count - 1;
    }

    ArgPositional *positional = &cmd->positionals[state->positional_index];

    // Type validation (positionals have no choices, only types)
    ArgOption type_probe = { .type = positional->type };
    char reason[PARSE_MESSAGE_BUFFER];
    if (!sc_args_validate_value(&type_probe, token, reason, sizeof reason)) {
        snprintf(
            message, sizeof message, "Invalid value '%s' for argument '%s'",
            token, positional->name
        );
        report_error(state->args, message, reason[0] ? reason : NULL);
        state->status = SC_ARGS_ERROR;
        return false;
    }

    if (!sc_args_positional_add_value(positional, token)) {
        state->status = SC_ARGS_ERROR;
        return false;
    }
    if (!positional->variadic) {
        state->positional_index++;
    }
    return true;
}

/** Verifies required options (whole chain) and positionals (leaf only). */
static bool check_required(ParseState *state) {
    char message[PARSE_MESSAGE_BUFFER];

    for (const ScArgsCmd *cmd = state->current; cmd; cmd = cmd->parent) {
        for (size_t i = 0; i < cmd->option_count; i++) {
            const ArgOption *option = &cmd->options[i];
            if (option->required && !option->present) {
                snprintf(
                    message, sizeof message,
                    "Missing required option '--%s'", option->long_name
                );
                report_error(state->args, message, option->help);
                return false;
            }
        }
    }

    for (size_t i = 0; i < state->current->positional_count; i++) {
        const ArgPositional *positional = &state->current->positionals[i];
        if (positional->required && positional->value_count == 0) {
            snprintf(
                message, sizeof message,
                "Missing required argument '%s'", positional->name
            );
            report_error(state->args, message, positional->help);
            return false;
        }
    }
    return true;
}

/** Consumes and sanitizes the next argv token as an option value. */
static char *next_argv_value(ParseState *state) {
    if (state->index + 1 >= state->argc) { return NULL; }
    state->index++;
    return sc_sanitize_copy(state->argv[state->index], false);
}

/** `-5`, `-5.2`, `-.5` are values, not options. */
static bool is_negative_number(const char *token) {
    return token[0] == '-'
        && (isdigit((unsigned char)token[1]) || token[1] == '.');
}

/** Renders a parse error as a pretty error to stderr. */
static void report_error(
    const ScArgs *args, const char *message, const char *hint
) {
    (void)args;
    ScError *error = sc_error_new(message);
    if (!error) { return; }
    if (hint && hint[0]) {
        sc_error_set_hint(error, hint);
    }
    sc_error_set_code(error, PARSE_ERROR_EXIT_CODE);
    sc_error_print_stderr(error);
    sc_error_free(error);
}

/** NULL-safe status assignment. */
static void set_status(ScArgsStatus *out, ScArgsStatus value) {
    if (out) { *out = value; }
}
