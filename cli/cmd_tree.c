/**
 * @file cmd_tree.c
 * The `tree` subcommand: renders indented lines as a sparcli tree.
 *
 * Input format: each line is one node; its depth is the number of leading
 * indentation steps (`--indent` spaces, or one tab) relative to the root.
 *
 *     root
 *       child a
 *       child b
 *         leaf
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"
#include "cli_stdin.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

enum {
    SC_CLI_TREE_DEFAULT_INDENT = 2,
    SC_CLI_TREE_MAX_DEPTH      = 64,
};

/** Parsed arguments of `sparcli tree`. */
typedef struct TreeArgs {
    ScTreeOpts  opts;
    int         indent; /**< Spaces per indentation level. */
    const char *file;   /**< Input file (`NULL`/`"-"` = stdin). */
} TreeArgs;

static int run_tree(ScCliCtx *ctx, const TreeArgs *args);
    static int line_depth(const char *line, int indent, const char **text);

static const char TREE_USAGE[] =
    "Usage: sparcli tree [options] [FILE]\n"
    "\n"
    "Render indented lines from FILE (or stdin) as a tree. Each level of\n"
    "indentation (--indent spaces, or one tab) nests the node one level\n"
    "deeper.\n"
    "\n"
    "Options:\n"
    "  --border STYLE             Connector style: ascii|single|double|\n"
    "                             rounded|thick (default: single)\n"
    "  --color COLOR              Connector color\n"
    "  --indent N                 Spaces per indentation level (default 2)\n"
    "  --no-guide                 Hide vertical guide lines\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_tree(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_BORDER = SC_CLI_OPT_CMD_BASE,
        OPT_COLOR,
        OPT_INDENT,
        OPT_NO_GUIDE,
    };
    static const struct option longopts[] = {
        { "border",   required_argument, NULL, OPT_BORDER },
        { "color",    required_argument, NULL, OPT_COLOR },
        { "indent",   required_argument, NULL, OPT_INDENT },
        { "no-guide", no_argument,       NULL, OPT_NO_GUIDE },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    TreeArgs args = {
        .opts   = { .type = SC_BORDER_SINGLE },
        .indent = SC_CLI_TREE_DEFAULT_INDENT,
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &args.opts.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.connector_color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_INDENT:
            if (!sc_cli_parse_int(optarg, &args.indent)
                || args.indent == 0) {
                return sc_cli_error(ctx, "invalid indent '%s'", optarg);
            }
            break;
        case OPT_NO_GUIDE:
            args.opts.no_guide = true;
            break;
        case SC_CLI_OPT_HELP:
            fputs(TREE_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    TreeArgs final_args = args;
    if (optind < argc) {
        final_args.file = argv[optind];
    }
    return run_tree(ctx, &final_args);
}

static int run_tree(ScCliCtx *ctx, const TreeArgs *args) {
    char *data = sc_cli_read_source(args->file);
    if (data == NULL) {
        return sc_cli_error(ctx, "cannot read input");
    }

    size_t line_count = 0;
    char **lines      = sc_cli_split_lines(data, &line_count);
    if (lines == NULL) {
        free(data);
        return sc_cli_error(ctx, "out of memory");
    }

    ScTree  *tree  = sc_tree_new(args->opts);
    ScText **texts = calloc(line_count > 0 ? line_count : 1,
                            sizeof(*texts));
    if (tree == NULL || texts == NULL) {
        sc_tree_free(tree);
        free(texts);
        free(lines);
        free(data);
        return sc_cli_error(ctx, "out of memory");
    }

    /* One parent node per depth level; a node at depth d attaches to the
       last node seen at depth d-1. */
    ScTreeNode *parents[SC_CLI_TREE_MAX_DEPTH] = { NULL };
    int         max_seen_depth                 = -1;
    int         status                         = SC_CLI_EXIT_OK;
    size_t      text_count                     = 0;

    for (size_t i = 0; i < line_count; i++) {
        if (lines[i][0] == '\0') {
            continue; /* skip blank lines */
        }

        const char *node_text = NULL;
        int depth = line_depth(lines[i], args->indent, &node_text);
        if (depth >= SC_CLI_TREE_MAX_DEPTH
            || depth > max_seen_depth + 1) {
            status = sc_cli_error(ctx, "line %zu: indentation skips a level",
                                  i + 1);
            break;
        }

        ScText *text = sc_cli_text(ctx, node_text);
        if (text == NULL) {
            status = sc_cli_error(ctx, "out of memory");
            break;
        }
        texts[text_count] = text;
        text_count++;

        ScTreeNode *parent = (depth > 0) ? parents[depth - 1] : NULL;
        ScTreeNode *node   = sc_tree_add_text(tree, parent, text, NULL,
                                              (ScTextStyle){ 0 });
        parents[depth] = node;
        if (depth > max_seen_depth) {
            max_seen_depth = depth;
        }
    }

    if (status == SC_CLI_EXIT_OK) {
        sc_tree_print(tree);
    }

    sc_tree_free(tree);
    for (size_t i = 0; i < text_count; i++) {
        sc_text_free(texts[i]);
    }
    free(texts);
    free(lines);
    free(data);
    return status;
}

/* Returns the indentation depth of `line` and points `*text` at the first
   non-whitespace character. Tabs count as one level each; otherwise
   `indent` spaces form one level (partial indents round down). */
static int line_depth(const char *line, int indent, const char **text) {
    int spaces = 0;
    int tabs   = 0;

    const char *cursor = line;
    while (*cursor == ' ' || *cursor == '\t') {
        if (*cursor == '\t') {
            tabs++;
        } else {
            spaces++;
        }
        cursor++;
    }

    *text = cursor;
    return tabs + (spaces / indent);
}
