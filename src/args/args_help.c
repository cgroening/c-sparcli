#include "sparcli.h"
#include "args/args_internal.h"
#include "core/text_internal.h"
#include "core/sc_memstream.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Buffer size for composed left-column / usage strings. */
#define HELP_LINE_BUFFER 256

/** Heading used for subcommands without an explicit group. */
#define HELP_DEFAULT_GROUP "Commands"

/** Indentation of the help tables. */
#define HELP_TABLE_INDENT 2


/**
 * Keeps composed cell strings alive until the table that borrows them has
 * been rendered (table cells do not copy their strings).
 */
typedef struct StringArena {
    char **items;
    size_t count;
    size_t capacity;
} StringArena;


/** Span styles used by the help renderer. */
static const ScTextStyle BOLD_STYLE = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle DIM_STYLE = {
    SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle PLAIN_STYLE = {
    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
};
static const ScTextStyle HEADING_STYLE = {
    SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE
};


// Forward declarations indented to reflect call hierarchy
static void render_header(const ScArgs *args, const ScArgsCmd *cmd);
static void render_usage(const ScArgs *args, const ScArgsCmd *cmd);
    static void write_command_path(
        const ScArgsCmd *cmd, char *buffer, size_t size
    );
static void render_subcommands(const ScArgsCmd *cmd);
    static const char *group_of(const ScArgsCmd *cmd);
    static bool group_already_rendered(
        const ScArgsCmd *parent, size_t index
    );
static void render_positionals(const ScArgsCmd *cmd);
static void render_options(const ScArgs *args, const ScArgsCmd *cmd);
    static void add_option_row(
        ScTableData *table, StringArena *arena, const ArgOption *option
    );
        static void compose_option_left_column(
            const ArgOption *option, char *buffer, size_t size
        );
        static char *compose_option_help(const ArgOption *option);
static void render_section_heading(const char *heading);
static void print_two_column_table(ScTableData *table);
static void print_blank_line(void);
static const char *arena_store(StringArena *arena, const char *str);
static const char *arena_adopt(StringArena *arena, char *str);
static void arena_free(StringArena *arena);


void sc_args_print_help(const ScArgs *args, const ScArgsCmd *cmd) {
    if (!args) { return; }
    sc_args_render_help(args, cmd ? cmd : &args->root);
}

void sc_args_render_help(const ScArgs *args, const ScArgsCmd *cmd) {
    render_header(args, cmd);
    render_usage(args, cmd);
    render_subcommands(cmd);
    render_positionals(cmd);
    render_options(args, cmd);

    if (cmd->subcommand_count > 0) {
        char path[HELP_LINE_BUFFER];
        write_command_path(cmd, path, sizeof path);
        ScText *footer = sc_text_new();
        if (footer) {
            sc_text_append_raw(footer, "Run '", DIM_STYLE);
            sc_text_append_raw(footer, path, DIM_STYLE);
            sc_text_append_raw(
                footer, " <command> --help' for command details.", DIM_STYLE
            );
            sc_print_text(footer);
            sc_text_free(footer);
            fputc('\n', sc_output_stream());
        }
    }
}

void sc_args_render_version(const ScArgs *args) {
    ScText *line = sc_text_new();
    if (!line) { return; }
    sc_text_append_raw(line, args->prog, BOLD_STYLE);
    if (args->version) {
        sc_text_append_raw(line, " ", PLAIN_STYLE);
        sc_text_append_raw(line, args->version, PLAIN_STYLE);
    }
    sc_print_text(line);
    sc_text_free(line);
    fputc('\n', sc_output_stream());
}

/** Bold program name + dim version, then the about line. */
static void render_header(const ScArgs *args, const ScArgsCmd *cmd) {
    ScText *header = sc_text_new();
    if (!header) { return; }

    char path[HELP_LINE_BUFFER];
    write_command_path(cmd, path, sizeof path);
    sc_text_append_raw(header, path, BOLD_STYLE);
    if (args->version && cmd == &args->root) {
        sc_text_append_raw(header, " ", PLAIN_STYLE);
        sc_text_append_raw(header, args->version, DIM_STYLE);
    }

    const char *about = cmd->about;
    if (about && about[0]) {
        sc_text_append_raw(header, "\n", PLAIN_STYLE);
        sc_text_append_raw(header, about, PLAIN_STYLE);
    }
    sc_print_text(header);
    sc_text_free(header);
    fputc('\n', sc_output_stream());
    print_blank_line();
}

/** `Usage: prog [options] <command>` line, adapted to the tree shape. */
static void render_usage(const ScArgs *args, const ScArgsCmd *cmd) {
    (void)args;
    char path[HELP_LINE_BUFFER];
    write_command_path(cmd, path, sizeof path);

    ScText *usage = sc_text_new();
    if (!usage) { return; }
    sc_text_append_raw(usage, "Usage: ", HEADING_STYLE);
    sc_text_append_raw(usage, path, PLAIN_STYLE);
    sc_text_append_raw(usage, " [options]", DIM_STYLE);

    if (cmd->subcommand_count > 0) {
        sc_text_append_raw(usage, " <command>", PLAIN_STYLE);
    }
    for (size_t i = 0; i < cmd->positional_count; i++) {
        const ArgPositional *positional = &cmd->positionals[i];
        char slot[HELP_LINE_BUFFER];
        snprintf(
            slot, sizeof slot, " %s%s%s%s",
            positional->required ? "<" : "[",
            positional->name,
            positional->variadic ? "..." : "",
            positional->required ? ">" : "]"
        );
        sc_text_append_raw(usage, slot, PLAIN_STYLE);
    }
    sc_print_text(usage);
    sc_text_free(usage);
    fputc('\n', sc_output_stream());
    print_blank_line();
}

/** Builds "prog sub subsub" from the root to `cmd`. */
static void write_command_path(
    const ScArgsCmd *cmd, char *buffer, size_t size
) {
    if (cmd->parent) {
        write_command_path(cmd->parent, buffer, size);
        size_t used = strlen(buffer);
        snprintf(buffer + used, size - used, " %s", cmd->name);
    } else {
        snprintf(buffer, size, "%s", cmd->name);
    }
}

/** One table per command group, in order of first appearance. */
static void render_subcommands(const ScArgsCmd *cmd) {
    if (cmd->subcommand_count == 0) { return; }

    for (size_t i = 0; i < cmd->subcommand_count; i++) {
        if (group_already_rendered(cmd, i)) { continue; }
        const char *group = group_of(cmd->subcommands[i]);

        render_section_heading(group);
        ScTableData *table = sc_table_new();
        if (!table) { return; }
        sc_table_add_column(table, "", (ScColOpts){ 0 });
        sc_table_add_column(table, "", (ScColOpts){ 0 });

        for (size_t j = i; j < cmd->subcommand_count; j++) {
            const ScArgsCmd *subcommand = cmd->subcommands[j];
            if (strcmp(group_of(subcommand), group) != 0) { continue; }
            sc_table_add_row(table, (ScCell[]){
                sc_cell(subcommand->name),
                sc_cell(subcommand->about),
            }, 2);
        }
        print_two_column_table(table);
        sc_table_free(table);
        print_blank_line();
    }
}

/** The group heading a subcommand belongs to. */
static const char *group_of(const ScArgsCmd *cmd) {
    return cmd->group ? cmd->group : HELP_DEFAULT_GROUP;
}

/** `true` when the group of subcommand `index` was rendered earlier. */
static bool group_already_rendered(const ScArgsCmd *parent, size_t index) {
    const char *group = group_of(parent->subcommands[index]);
    for (size_t i = 0; i < index; i++) {
        if (strcmp(group_of(parent->subcommands[i]), group) == 0) {
            return true;
        }
    }
    return false;
}

/** The "Arguments" section (positionals). */
static void render_positionals(const ScArgsCmd *cmd) {
    if (cmd->positional_count == 0) { return; }

    render_section_heading("Arguments");
    ScTableData *table = sc_table_new();
    if (!table) { return; }
    sc_table_add_column(table, "", (ScColOpts){ 0 });
    sc_table_add_column(table, "", (ScColOpts){ 0 });

    // Cell strings are borrowed by the table: keep them alive until print
    StringArena arena = { 0 };
    for (size_t i = 0; i < cmd->positional_count; i++) {
        const ArgPositional *positional = &cmd->positionals[i];
        char left[HELP_LINE_BUFFER];
        snprintf(
            left, sizeof left, "%s%s",
            positional->name, positional->variadic ? "..." : ""
        );
        char right[HELP_LINE_BUFFER];
        snprintf(
            right, sizeof right, "%s%s",
            positional->help, positional->required ? " (required)" : ""
        );
        sc_table_add_row(table, (ScCell[]){
            sc_cell(arena_store(&arena, left)),
            sc_cell(arena_store(&arena, right)),
        }, 2);
    }
    print_two_column_table(table);
    sc_table_free(table);
    arena_free(&arena);
    print_blank_line();
}

/** The "Options" section: own options + inherited + reserved help/version. */
static void render_options(const ScArgs *args, const ScArgsCmd *cmd) {
    render_section_heading("Options");
    ScTableData *table = sc_table_new();
    if (!table) { return; }
    sc_table_add_column(table, "", (ScColOpts){ 0 });
    sc_table_add_column(table, "", (ScColOpts){ 0 });

    // Cell strings are borrowed by the table: keep them alive until print
    StringArena arena = { 0 };

    // Own options first, then inherited ones (root last)
    for (const ScArgsCmd *node = cmd; node; node = node->parent) {
        for (size_t i = 0; i < node->option_count; i++) {
            add_option_row(table, &arena, &node->options[i]);
        }
    }

    // Reserved options
    sc_table_add_row(table, (ScCell[]){
        sc_cell("-h, --help"), sc_cell("Show this help"),
    }, 2);
    if (args->version && cmd == &args->root) {
        sc_table_add_row(table, (ScCell[]){
            sc_cell("-V, --version"), sc_cell("Show the version"),
        }, 2);
    }

    print_two_column_table(table);
    sc_table_free(table);
    arena_free(&arena);
    print_blank_line();
}

/** Adds one option row (left column spec + composed help text). */
static void add_option_row(
    ScTableData *table, StringArena *arena, const ArgOption *option
) {
    char left[HELP_LINE_BUFFER];
    compose_option_left_column(option, left, sizeof left);

    char *help = compose_option_help(option);
    const char *help_cell = help
        ? arena_adopt(arena, help)
        : option->help;
    sc_table_add_row(table, (ScCell[]){
        sc_cell(arena_store(arena, left)),
        sc_cell(help_cell),
    }, 2);
}

/** `"-j, --jobs <N>"` / `"    --flag"` left column. */
static void compose_option_left_column(
    const ArgOption *option, char *buffer, size_t size
) {
    char short_part[8];
    if (option->short_name) {
        snprintf(short_part, sizeof short_part, "-%c, ", option->short_name);
    } else {
        snprintf(short_part, sizeof short_part, "    ");
    }

    if (option->is_flag) {
        snprintf(buffer, size, "%s--%s", short_part, option->long_name);
    } else {
        snprintf(
            buffer, size, "%s--%s <%s>",
            short_part, option->long_name,
            option->metavar ? option->metavar : "VALUE"
        );
    }
}

/** Help text + `[default: …]` + `[choices: …]` + `(required)` suffixes. */
static char *compose_option_help(const ArgOption *option) {
    char *buffer = NULL;
    size_t size = 0;
    ScMemStream ms;
    FILE *stream = sc_memstream_open(&ms, &buffer, &size);
    if (!stream) { return NULL; }

    fputs(option->help, stream);
    if (option->required) {
        fputs(" (required)", stream);
    }
    if (option->default_value) {
        fprintf(stream, " [default: %s]", option->default_value);
    }
    if (option->choice_count > 0) {
        fputs(" [choices: ", stream);
        for (size_t i = 0; i < option->choice_count; i++) {
            fprintf(stream, "%s%s", i > 0 ? ", " : "", option->choices[i]);
        }
        fputs("]", stream);
    }
    sc_memstream_close(&ms);
    return buffer;
}

/** Bold cyan section heading. */
static void render_section_heading(const char *heading) {
    ScText *text = sc_text_new();
    if (!text) { return; }
    sc_text_append_raw(text, heading, HEADING_STYLE);
    sc_text_append_raw(text, ":", HEADING_STYLE);
    sc_print_text(text);
    sc_text_free(text);
    fputc('\n', sc_output_stream());
}

/** Renders a two-column table without any borders, indented. */
static void print_two_column_table(ScTableData *table) {
    ScRendered *rendered = sc_capture_table(table, (ScTableOpts){
        .border = {
            .type = SC_BORDER_NONE,
            .no_outer = true,
            .no_inner_h = true,
            .no_inner_v = true,
        },
        .cell_pad = { .top = 0, .right = 2, .bottom = 0, .left = 0 },
    });
    if (!rendered) { return; }
    sc_pad_print(rendered, (ScPadOpts){ .left = HELP_TABLE_INDENT });
    sc_rendered_free(rendered);
}

/** One empty line on the output stream. */
static void print_blank_line(void) {
    fputc('\n', sc_output_stream());
}

/** Stores a copy of `str` in the arena; returns the stored copy. */
static const char *arena_store(StringArena *arena, const char *str) {
    char *copy = strdup(str);
    if (!copy) { return ""; }
    return arena_adopt(arena, copy);
}

/** Adopts an already-heap-allocated string into the arena. */
static const char *arena_adopt(StringArena *arena, char *str) {
    if (arena->count == arena->capacity) {
        size_t new_capacity = arena->capacity ? arena->capacity * 2 : 8;
        char **grown = realloc(arena->items, new_capacity * sizeof(char *));
        if (!grown) {
            free(str);
            return "";
        }
        arena->items = grown;
        arena->capacity = new_capacity;
    }
    arena->items[arena->count++] = str;
    return str;
}

/** Frees every string held by the arena. */
static void arena_free(StringArena *arena) {
    for (size_t i = 0; i < arena->count; i++) {
        free(arena->items[i]);
    }
    free(arena->items);
    arena->items = NULL;
    arena->count = 0;
    arena->capacity = 0;
}
