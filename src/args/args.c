#include "sparcli.h"
#include "args/args_internal.h"
#include "core/sanitize_internal.h"

#include <stdlib.h>
#include <string.h>


/** Initial capacity of the option/positional/subcommand arrays. */
#define ARGS_INITIAL_CAPACITY 4

/** Metavar used when an option declares none. */
#define ARGS_DEFAULT_METAVAR "VALUE"


// Forward declarations indented to reflect call hierarchy
static void init_command(ScArgsCmd *cmd, ScArgs *owner, ScArgsCmd *parent);
static void free_command_contents(ScArgsCmd *cmd);
    static void free_option(ArgOption *option);
    static void free_positional(ArgPositional *positional);
static void reset_command_results(ScArgsCmd *cmd);
static char *sanitized_copy_or_empty(const char *str);
static ArgOption *add_option(
    ScArgsCmd *cmd, const char *long_name, char short_name, const char *help
);
static const ArgOption *lookup_result_option(
    const ScArgs *args, const char *name
);
static const ArgPositional *lookup_result_positional(
    const ScArgs *args, const char *name
);
    static const ScArgsCmd *result_leaf(const ScArgs *args);


/* ── Builder ────────────────────────────────────────────────────────────── */

ScArgs *sc_args_new(ScArgsOpts opts) {
    ScArgs *args = calloc(1, sizeof(ScArgs));
    if (!args) { return NULL; }

    args->prog = sanitized_copy_or_empty(opts.prog);
    args->about = sanitized_copy_or_empty(opts.about);
    args->version = opts.version ? sc_sanitize_copy(opts.version, false) : NULL;
    args->ansi = opts.ansi;
    if (!args->prog || !args->about) {
        free(args->prog);
        free(args->about);
        free(args->version);
        free(args);
        return NULL;
    }

    init_command(&args->root, args, NULL);
    args->root.name = strdup(args->prog);
    args->root.about = strdup(args->about);
    return args;
}

void sc_args_free(ScArgs *args) {
    if (!args) { return; }
    free_command_contents(&args->root);
    free(args->prog);
    free(args->version);
    free(args->about);
    free(args);
}

void sc_args_reset(ScArgs *args) {
    if (!args) { return; }
    reset_command_results(&args->root);
    args->matched = NULL;
}

ScArgsCmd *sc_args_root(ScArgs *args) {
    return args ? &args->root : NULL;
}

ScArgsCmd *sc_args_subcommand(
    ScArgsCmd *parent, const char *name, const char *about
) {
    if (!parent || !name) { return NULL; }

    if (parent->subcommand_count == parent->subcommand_capacity) {
        size_t new_capacity = parent->subcommand_capacity
            ? parent->subcommand_capacity * 2
            : ARGS_INITIAL_CAPACITY;
        ScArgsCmd **grown = realloc(
            parent->subcommands, new_capacity * sizeof(ScArgsCmd *)
        );
        if (!grown) { return NULL; }
        parent->subcommands = grown;
        parent->subcommand_capacity = new_capacity;
    }

    ScArgsCmd *cmd = calloc(1, sizeof(ScArgsCmd));
    if (!cmd) { return NULL; }
    init_command(cmd, parent->owner, parent);
    cmd->name = sanitized_copy_or_empty(name);
    cmd->about = sanitized_copy_or_empty(about);
    if (!cmd->name || !cmd->about) {
        free(cmd->name);
        free(cmd->about);
        free(cmd);
        return NULL;
    }

    parent->subcommands[parent->subcommand_count++] = cmd;
    return cmd;
}

void sc_args_cmd_group(ScArgsCmd *cmd, const char *group) {
    if (!cmd) { return; }
    free(cmd->group);
    cmd->group = group ? sc_sanitize_copy(group, false) : NULL;
}

void sc_args_flag(
    ScArgsCmd *cmd, const char *long_name, char short_name, const char *help
) {
    ArgOption *option = add_option(cmd, long_name, short_name, help);
    if (!option) { return; }
    option->is_flag = true;
}

void sc_args_opt(
    ScArgsCmd *cmd, const char *long_name, char short_name, ScArgType type,
    const char *metavar, const char *help
) {
    ArgOption *option = add_option(cmd, long_name, short_name, help);
    if (!option) { return; }
    option->type = type;
    option->metavar = sanitized_copy_or_empty(
        metavar ? metavar : ARGS_DEFAULT_METAVAR
    );
}

void sc_args_opt_default(
    ScArgsCmd *cmd, const char *long_name, const char *default_value
) {
    ArgOption *option = sc_args_find_option(cmd, long_name);
    if (!option || !default_value) { return; }
    free(option->default_value);
    option->default_value = sc_sanitize_copy(default_value, false);
}

void sc_args_opt_choices(
    ScArgsCmd *cmd, const char *long_name, const char *const *choices
) {
    ArgOption *option = sc_args_find_option(cmd, long_name);
    if (!option || !choices) { return; }

    size_t count = 0;
    while (choices[count]) { count++; }

    // Copy first, so the old list survives an allocation failure intact.
    // An empty list (count == 0) skips the allocation and clears below.
    char **copies = NULL;
    if (count > 0) {
        copies = calloc(count, sizeof(char *));
        if (!copies) { return; }
        for (size_t i = 0; i < count; i++) {
            copies[i] = sc_sanitize_copy(choices[i], false);
            if (!copies[i]) {
                for (size_t j = 0; j < i; j++) { free(copies[j]); }
                free(copies);
                return;
            }
        }
    }

    for (size_t i = 0; i < option->choice_count; i++) {
        free(option->choices[i]);
    }
    free(option->choices);
    option->choices = copies;   // NULL when the new list is empty
    option->choice_count = count;
}

void sc_args_opt_required(ScArgsCmd *cmd, const char *long_name) {
    ArgOption *option = sc_args_find_option(cmd, long_name);
    if (!option) { return; }
    option->required = true;
}

void sc_args_positional(
    ScArgsCmd *cmd, const char *name, ScArgType type, const char *help,
    bool required, bool variadic
) {
    if (!cmd || !name) { return; }

    if (cmd->positional_count == cmd->positional_capacity) {
        size_t new_capacity = cmd->positional_capacity
            ? cmd->positional_capacity * 2
            : ARGS_INITIAL_CAPACITY;
        ArgPositional *grown = realloc(
            cmd->positionals, new_capacity * sizeof(ArgPositional)
        );
        if (!grown) { return; }
        cmd->positionals = grown;
        cmd->positional_capacity = new_capacity;
    }

    ArgPositional *positional = &cmd->positionals[cmd->positional_count];
    memset(positional, 0, sizeof(ArgPositional));
    positional->name = sanitized_copy_or_empty(name);
    positional->help = sanitized_copy_or_empty(help);
    positional->type = type;
    positional->required = required;
    positional->variadic = variadic;
    if (!positional->name || !positional->help) {
        free(positional->name);
        free(positional->help);
        return;
    }
    cmd->positional_count++;
}


/* ── Result getters ─────────────────────────────────────────────────────── */

const char *sc_args_get_str(const ScArgs *args, const char *name) {
    const ArgOption *option = lookup_result_option(args, name);
    if (option) {
        if (option->present && option->value_text) {
            return option->value_text;
        }
        return option->default_value;
    }

    const ArgPositional *positional = lookup_result_positional(args, name);
    if (positional && positional->value_count > 0) {
        return positional->values[0];
    }
    return NULL;
}

long sc_args_get_int(const ScArgs *args, const char *name) {
    const char *text = sc_args_get_str(args, name);
    long value = 0;
    if (text && sc_args_parse_int(text, &value)) {
        return value;
    }
    return 0;
}

double sc_args_get_double(const ScArgs *args, const char *name) {
    const char *text = sc_args_get_str(args, name);
    double value = 0.0;
    if (text && sc_args_parse_double(text, &value)) {
        return value;
    }
    return 0.0;
}

bool sc_args_get_flag(const ScArgs *args, const char *name) {
    const ArgOption *option = lookup_result_option(args, name);
    return option != NULL && option->present;
}

int sc_args_get_enum(const ScArgs *args, const char *name) {
    const ArgOption *option = lookup_result_option(args, name);
    if (!option) { return -1; }

    const char *text = option->present && option->value_text
        ? option->value_text
        : option->default_value;
    if (!text) { return -1; }
    return sc_args_find_choice(option, text);
}

ScColor sc_args_get_color(const ScArgs *args, const char *name) {
    const char *text = sc_args_get_str(args, name);
    ScColor color = { 0 };
    if (text && sc_args_parse_color(text, &color)) {
        return color;
    }
    return (ScColor){ 0 };
}

const char *const *sc_args_get_many(
    const ScArgs *args, const char *name, size_t *count
) {
    if (count) { *count = 0; }

    const ArgPositional *positional = lookup_result_positional(args, name);
    if (!positional || positional->value_count == 0) { return NULL; }

    if (count) { *count = positional->value_count; }
    return (const char *const *)positional->values;
}

bool sc_args_present(const ScArgs *args, const char *name) {
    const ArgOption *option = lookup_result_option(args, name);
    if (option) { return option->present; }

    const ArgPositional *positional = lookup_result_positional(args, name);
    return positional != NULL && positional->value_count > 0;
}

const char *sc_args_selected_command(const ScArgs *args) {
    if (!args || !args->matched) { return NULL; }
    return args->matched->name;
}

const char *sc_args_cmd_name(const ScArgsCmd *cmd) {
    return cmd ? cmd->name : NULL;
}


/* ── Tree lookups (shared with the other args_*.c files) ────────────────── */

ArgOption *sc_args_find_option(const ScArgsCmd *cmd, const char *long_name) {
    if (!cmd || !long_name) { return NULL; }
    for (size_t i = 0; i < cmd->option_count; i++) {
        if (strcmp(cmd->options[i].long_name, long_name) == 0) {
            return (ArgOption *)&cmd->options[i];
        }
    }
    return NULL;
}

ArgOption *sc_args_find_option_chain(
    const ScArgsCmd *leaf, const char *long_name
) {
    for (const ScArgsCmd *cmd = leaf; cmd; cmd = cmd->parent) {
        ArgOption *option = sc_args_find_option(cmd, long_name);
        if (option) { return option; }
    }
    return NULL;
}

ArgOption *sc_args_find_option_short_chain(
    const ScArgsCmd *leaf, char short_name
) {
    if (short_name == 0) { return NULL; }
    for (const ScArgsCmd *cmd = leaf; cmd; cmd = cmd->parent) {
        for (size_t i = 0; i < cmd->option_count; i++) {
            if (cmd->options[i].short_name == short_name) {
                return (ArgOption *)&cmd->options[i];
            }
        }
    }
    return NULL;
}

ScArgsCmd *sc_args_find_subcommand(const ScArgsCmd *cmd, const char *name) {
    if (!cmd || !name) { return NULL; }
    for (size_t i = 0; i < cmd->subcommand_count; i++) {
        if (strcmp(cmd->subcommands[i]->name, name) == 0) {
            return cmd->subcommands[i];
        }
    }
    return NULL;
}

ArgPositional *sc_args_find_positional_chain(
    const ScArgsCmd *leaf, const char *name
) {
    if (!name) { return NULL; }
    for (const ScArgsCmd *cmd = leaf; cmd; cmd = cmd->parent) {
        for (size_t i = 0; i < cmd->positional_count; i++) {
            if (strcmp(cmd->positionals[i].name, name) == 0) {
                return (ArgPositional *)&cmd->positionals[i];
            }
        }
    }
    return NULL;
}

bool sc_args_positional_add_value(
    ArgPositional *positional, const char *value
) {
    if (!positional || !value) { return false; }

    if (positional->value_count == positional->value_capacity) {
        size_t new_capacity = positional->value_capacity
            ? positional->value_capacity * 2
            : ARGS_INITIAL_CAPACITY;
        char **grown = realloc(
            positional->values, new_capacity * sizeof(char *)
        );
        if (!grown) { return false; }
        positional->values = grown;
        positional->value_capacity = new_capacity;
    }

    char *copy = strdup(value);
    if (!copy) { return false; }
    positional->values[positional->value_count++] = copy;
    return true;
}

bool sc_args_option_set_value(ArgOption *option, const char *value) {
    if (!option || !value) { return false; }
    char *copy = strdup(value);
    if (!copy) { return false; }
    free(option->value_text);
    option->value_text = copy;
    return true;
}


/* ── Internals ──────────────────────────────────────────────────────────── */

/** Zero-initializes a command node and wires its back-pointers. */
static void init_command(ScArgsCmd *cmd, ScArgs *owner, ScArgsCmd *parent) {
    memset(cmd, 0, sizeof(ScArgsCmd));
    cmd->owner = owner;
    cmd->parent = parent;
}

/** Frees everything a command owns, then its subcommand nodes. */
static void free_command_contents(ScArgsCmd *cmd) {
    for (size_t i = 0; i < cmd->option_count; i++) {
        free_option(&cmd->options[i]);
    }
    free(cmd->options);

    for (size_t i = 0; i < cmd->positional_count; i++) {
        free_positional(&cmd->positionals[i]);
    }
    free(cmd->positionals);

    for (size_t i = 0; i < cmd->subcommand_count; i++) {
        free_command_contents(cmd->subcommands[i]);
        free(cmd->subcommands[i]);
    }
    free(cmd->subcommands);

    free(cmd->name);
    free(cmd->about);
    free(cmd->group);
}

/** Frees the strings owned by one option. */
static void free_option(ArgOption *option) {
    free(option->long_name);
    free(option->metavar);
    free(option->help);
    free(option->default_value);
    for (size_t i = 0; i < option->choice_count; i++) {
        free(option->choices[i]);
    }
    free(option->choices);
    free(option->value_text);
}

/** Frees the strings owned by one positional. */
static void free_positional(ArgPositional *positional) {
    free(positional->name);
    free(positional->help);
    for (size_t i = 0; i < positional->value_count; i++) {
        free(positional->values[i]);
    }
    free(positional->values);
}

/** Clears the parse results of one command and all its subcommands. */
static void reset_command_results(ScArgsCmd *cmd) {
    for (size_t i = 0; i < cmd->option_count; i++) {
        ArgOption *option = &cmd->options[i];
        option->present = false;
        free(option->value_text);
        option->value_text = NULL;
    }

    for (size_t i = 0; i < cmd->positional_count; i++) {
        ArgPositional *positional = &cmd->positionals[i];
        for (size_t j = 0; j < positional->value_count; j++) {
            free(positional->values[j]);
        }
        positional->value_count = 0;
    }

    for (size_t i = 0; i < cmd->subcommand_count; i++) {
        reset_command_results(cmd->subcommands[i]);
    }
}

/** Sanitized copy of `str`; an empty heap string for `NULL`. */
static char *sanitized_copy_or_empty(const char *str) {
    if (!str) { return strdup(""); }
    return sc_sanitize_copy(str, false);
}

/** Shared registration path of `sc_args_flag` / `sc_args_opt`. */
static ArgOption *add_option(
    ScArgsCmd *cmd, const char *long_name, char short_name, const char *help
) {
    if (!cmd || !long_name) { return NULL; }
    // Duplicate names would make lookups ambiguous: ignore re-registration
    if (sc_args_find_option(cmd, long_name)) { return NULL; }

    if (cmd->option_count == cmd->option_capacity) {
        size_t new_capacity = cmd->option_capacity
            ? cmd->option_capacity * 2
            : ARGS_INITIAL_CAPACITY;
        ArgOption *grown = realloc(
            cmd->options, new_capacity * sizeof(ArgOption)
        );
        if (!grown) { return NULL; }
        cmd->options = grown;
        cmd->option_capacity = new_capacity;
    }

    ArgOption *option = &cmd->options[cmd->option_count];
    memset(option, 0, sizeof(ArgOption));
    option->long_name = sanitized_copy_or_empty(long_name);
    option->help = sanitized_copy_or_empty(help);
    option->short_name = short_name;
    if (!option->long_name || !option->help) {
        free(option->long_name);
        free(option->help);
        return NULL;
    }
    cmd->option_count++;
    return option;
}

/** Option lookup for the getters: matched chain, falling back to root. */
static const ArgOption *lookup_result_option(
    const ScArgs *args, const char *name
) {
    if (!args || !name) { return NULL; }
    return sc_args_find_option_chain(result_leaf(args), name);
}

/** Positional lookup for the getters: matched chain, falling back to root. */
static const ArgPositional *lookup_result_positional(
    const ScArgs *args, const char *name
) {
    if (!args || !name) { return NULL; }
    return sc_args_find_positional_chain(result_leaf(args), name);
}

/** The command chain the getters search: matched leaf or the root. */
static const ScArgsCmd *result_leaf(const ScArgs *args) {
    return args->matched ? args->matched : &args->root;
}
