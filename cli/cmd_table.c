/**
 * @file cmd_table.c
 * The `table` subcommand: renders CSV/TSV data as a sparcli table.
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_csv.h"
#include "cli_parse.h"
#include "cli_stdin.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

/** Parsed arguments of `sparcli table`. */
typedef struct TableArgs {
    ScTableOpts opts;
    ScHAlign    col_halign; /**< Default horizontal alignment of columns. */
    char        delim;      /**< CSV field delimiter. */
    const char *file;       /**< Input file (`NULL`/`"-"` = stdin). */
} TableArgs;

static int run_table(ScCliCtx *ctx, TableArgs *args);
    static void add_columns(ScTableData *table, const ScCliCsv *csv,
                            const TableArgs *args);
    static bool add_rows(ScCliCtx *ctx, ScTableData *table,
                         const ScCliCsv *csv, const TableArgs *args);
        static bool is_blank_row(const ScCliCsv *csv, size_t row);

static const char TABLE_USAGE[] =
    "Usage: sparcli table [options] [FILE]\n"
    "\n"
    "Render CSV data from FILE (or stdin) as a table. Use --tsv or\n"
    "--delim for other delimiters; quoted fields (RFC 4180) are\n"
    "supported. Cell text is parsed as markup unless --no-markup is\n"
    "given.\n"
    "\n"
    "Options:\n"
    "  --tsv                      Tab-separated input\n"
    "  --delim CHAR               Custom field delimiter\n"
    "  --header-row               Treat the first row as the header\n"
    "  --header-col               Treat the first column as a header\n"
    "  --border STYLE             none|ascii|single|double|rounded|thick\n"
    "  --color COLOR              Outer border color\n"
    "  --inner-color COLOR        Inner separator color\n"
    "  --no-outer                 Hide the outer frame\n"
    "  --no-inner-h               Hide row separators\n"
    "  --no-inner-v               Hide column separators\n"
    "  --striped                  Alternate row backgrounds\n"
    "  --stripe-bg COLOR          Stripe background color\n"
    "  --title TEXT               Table title\n"
    "  --align left|center|right  Default column alignment\n"
    "  --width N                  Total table width (0 = fit content)\n"
    "  --max-rows N               Show at most N rows\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_table(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_TSV = SC_CLI_OPT_CMD_BASE,
        OPT_DELIM,
        OPT_HEADER_ROW,
        OPT_HEADER_COL,
        OPT_BORDER,
        OPT_COLOR,
        OPT_INNER_COLOR,
        OPT_NO_OUTER,
        OPT_NO_INNER_H,
        OPT_NO_INNER_V,
        OPT_STRIPED,
        OPT_STRIPE_BG,
        OPT_TITLE,
        OPT_ALIGN,
        OPT_WIDTH,
        OPT_MAX_ROWS,
    };
    static const struct option longopts[] = {
        { "tsv",         no_argument,       NULL, OPT_TSV },
        { "delim",       required_argument, NULL, OPT_DELIM },
        { "header-row",  no_argument,       NULL, OPT_HEADER_ROW },
        { "header-col",  no_argument,       NULL, OPT_HEADER_COL },
        { "border",      required_argument, NULL, OPT_BORDER },
        { "color",       required_argument, NULL, OPT_COLOR },
        { "inner-color", required_argument, NULL, OPT_INNER_COLOR },
        { "no-outer",    no_argument,       NULL, OPT_NO_OUTER },
        { "no-inner-h",  no_argument,       NULL, OPT_NO_INNER_H },
        { "no-inner-v",  no_argument,       NULL, OPT_NO_INNER_V },
        { "striped",     no_argument,       NULL, OPT_STRIPED },
        { "stripe-bg",   required_argument, NULL, OPT_STRIPE_BG },
        { "title",       required_argument, NULL, OPT_TITLE },
        { "align",       required_argument, NULL, OPT_ALIGN },
        { "width",       required_argument, NULL, OPT_WIDTH },
        { "max-rows",    required_argument, NULL, OPT_MAX_ROWS },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    TableArgs args = {
        .opts  = { .border   = { .type = SC_BORDER_SINGLE },
                   .cell_pad = { .left = 1, .right = 1 } },
        .delim = ',',
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_TSV:
            args.delim = '\t';
            break;
        case OPT_DELIM:
            args.delim = optarg[0];
            break;
        case OPT_HEADER_ROW:
            args.opts.header.row = true;
            break;
        case OPT_HEADER_COL:
            args.opts.header.col = true;
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &args.opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.border.outer_color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_INNER_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.border.inner_color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_NO_OUTER:
            args.opts.border.no_outer = true;
            break;
        case OPT_NO_INNER_H:
            args.opts.border.no_inner_h = true;
            break;
        case OPT_NO_INNER_V:
            args.opts.border.no_inner_v = true;
            break;
        case OPT_STRIPED:
            args.opts.striped = true;
            break;
        case OPT_STRIPE_BG:
            if (!sc_cli_parse_color(optarg, &args.opts.stripe_bg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_TITLE:
            args.opts.title.text = optarg;
            break;
        case OPT_ALIGN:
            if (!sc_cli_parse_halign(optarg, &args.col_halign)) {
                return sc_cli_error(ctx, "invalid alignment '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.total_width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_MAX_ROWS:
            if (!sc_cli_parse_int(optarg, &args.opts.max_rows)) {
                return sc_cli_error(ctx, "invalid row count '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(TABLE_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    if (optind < argc) {
        args.file = argv[optind];
    }
    return run_table(ctx, &args);
}

static int run_table(ScCliCtx *ctx, TableArgs *args) {
    char *data = sc_cli_read_source(args->file);
    if (data == NULL) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScCliCsv *csv = sc_cli_csv_parse(data, args->delim);
    free(data);
    if (csv == NULL) {
        return sc_cli_error(ctx, "invalid CSV input (unterminated quote?)");
    }
    if (sc_cli_csv_rows(csv) == 0) {
        sc_cli_csv_free(csv);
        return sc_cli_error(ctx, "no input data");
    }

    /* A bold header row is the default whenever a header is requested. */
    if (args->opts.header.row
        && args->opts.header.style.attr == SC_TEXT_ATTR_NONE) {
        args->opts.header.style.attr = SC_TEXT_ATTR_BOLD;
    }

    ScTableData *table = sc_table_new();
    if (table == NULL) {
        sc_cli_csv_free(csv);
        return sc_cli_error(ctx, "out of memory");
    }

    add_columns(table, csv, args);
    if (!add_rows(ctx, table, csv, args)) {
        sc_table_free(table);
        sc_cli_csv_free(csv);
        return sc_cli_error(ctx, "out of memory");
    }

    sc_table_print(table, args->opts);

    /* Plain-string cells borrow the CSV fields, so the CSV document is
       freed only after printing. */
    sc_table_free(table);
    sc_cli_csv_free(csv);
    return SC_CLI_EXIT_OK;
}

/* Adds one table column per CSV column; with --header-row the first CSV
   row provides the header strings. */
static void add_columns(ScTableData *table, const ScCliCsv *csv,
                        const TableArgs *args) {
    size_t    col_count = sc_cli_csv_max_cols(csv);
    ScColOpts col_opts  = { .halign = args->col_halign };

    for (size_t col = 0; col < col_count; col++) {
        const char *header = NULL;
        if (args->opts.header.row) {
            header = sc_cli_csv_field(csv, 0, col);
        }
        sc_table_add_column(table, header, col_opts);
    }
}

/* Adds every CSV data row to the table (skipping the header row when it
   was consumed by add_columns and skipping blank rows). */
static bool add_rows(ScCliCtx *ctx, ScTableData *table, const ScCliCsv *csv,
                     const TableArgs *args) {
    size_t col_count = sc_cli_csv_max_cols(csv);
    size_t row_count = sc_cli_csv_rows(csv);
    size_t first_row = args->opts.header.row ? 1 : 0;

    ScCell *cells = malloc(col_count * sizeof(*cells));
    if (cells == NULL) {
        return false;
    }

    for (size_t row = first_row; row < row_count; row++) {
        if (is_blank_row(csv, row)) {
            continue;
        }
        for (size_t col = 0; col < col_count; col++) {
            const char *field = sc_cli_csv_field(csv, row, col);
            cells[col] = ctx->markup ? sc_cell_from_markup(field)
                                     : sc_cell_from_str(field);
        }
        sc_table_add_row(table, cells, col_count);
    }

    free(cells);
    return true;
}

/* A row is blank when it has a single empty field (e.g. an empty line). */
static bool is_blank_row(const ScCliCsv *csv, size_t row) {
    return sc_cli_csv_cols(csv, row) == 1
           && sc_cli_csv_field(csv, row, 0)[0] == '\0';
}
