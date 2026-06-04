/**
 * @file cmd_select.c
 * Interactive selection subcommands: `select`, `fuzzy` and `date`.
 *
 * Items come from positional arguments or - when none are given - from
 * stdin (one item per line). The chosen item(s) are printed to stdout,
 * one per line; the UI itself is drawn on `/dev/tty`, so piping items in
 * and capturing the choice out works at the same time:
 * `branch=$(git branch --format='%(refname:short)' | sparcli select)`.
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"
#include "cli_stdin.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/** Item source: argv items or owned stdin lines. */
typedef struct SelectSource {
    char  **items;
    size_t  count;
    char   *data;  /**< Owned stdin buffer (`NULL` for argv items). */
    char  **lines; /**< Owned line array (`NULL` for argv items). */
} SelectSource;

static bool select_source_init(SelectSource *source, int argc, char **argv);
static void select_source_release(SelectSource *source);
static bool parse_search_columns(const char *spec, uint64_t *out);


/* ── arrow-key navigation (--arrow-nav) ─────────────────────────────────────
 * Left/Right are bound to RETURN-mode shortcuts so a wrapper script (e.g.
 * examples/run.zsh) can walk a multi-stage picker: Left reports "back" via the
 * SC_CLI_EXIT_BACK exit code, Right submits like Enter. */
enum { NAV_BACK_ID = 1, NAV_FWD_ID = 2 };

/** Closes the fuzzy prompt only when a row is matched (Right = submit). */
static bool fuzzy_forward_cb(int id, void *user) {
    (void)id;
    return !sc_fuzzy_has_selection((const ScFuzzy *)user);
}

/** Maps the input status + fired nav id to a CLI exit code. */
static int nav_exit(ScInputStatus status, int fired) {
    if (status == SC_INPUT_OK && fired == NAV_BACK_ID) {
        return SC_CLI_EXIT_BACK;
    }
    return sc_cli_input_exit(status);
}


/* ── select ─────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli select`. */
typedef struct SelectArgs {
    ScSelectOpts   opts;
    ScCliInputArgs input;
    ScCliStyleArgs styles;
    bool           arrow_nav; /**< Bind Left=back / Right=select. */
} SelectArgs;

static int run_select(ScCliCtx *ctx, SelectArgs *args, int argc,
                      char **argv);

static const char SELECT_USAGE[] =
    "Usage: sparcli select [options] [ITEM...]\n"
    "\n"
    "Choose one item (or several with --multi) from a list. Items come\n"
    "from the ITEM arguments or - without arguments - from stdin, one\n"
    "item per line. The chosen item(s) are printed to stdout, one per\n"
    "line.\n"
    "\n"
    "Options:\n"
    "  --prompt TEXT              Heading shown above the list\n"
    "  --multi                    Multi-select with checkboxes\n"
    "  --max-visible N            Viewport height (default 10)\n"
    "  --marker STR               Marker before non-cursor rows\n"
    "  --cursor-marker STR        Marker before the cursor row\n"
    "  --checkbox-on STR          Checked box glyph (multi-select)\n"
    "  --checkbox-off STR         Unchecked box glyph (multi-select)\n"
    "  --arrow-nav                Left = back (exit 3), Right = select\n"
    SC_CLI_BOX_USAGE
    SC_CLI_INPUT_USAGE
    "\n"
    "--style elements: prompt, selected, summary, hint\n"
    "Set the cursor-row colors with --style selected (e.g.\n"
    "--style selected='white on magenta').\n";

int sc_cli_cmd_select(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_PROMPT = SC_CLI_OPT_CMD_BASE,
        OPT_MULTI,
        OPT_MAX_VISIBLE,
        OPT_MARKER,
        OPT_CURSOR_MARKER,
        OPT_CHECKBOX_ON,
        OPT_CHECKBOX_OFF,
        OPT_ARROW_NAV,
    };
    static const struct option longopts[] = {
        { "prompt",        required_argument, NULL, OPT_PROMPT },
        { "multi",         no_argument,       NULL, OPT_MULTI },
        { "max-visible",   required_argument, NULL, OPT_MAX_VISIBLE },
        { "marker",        required_argument, NULL, OPT_MARKER },
        { "cursor-marker", required_argument, NULL, OPT_CURSOR_MARKER },
        { "checkbox-on",   required_argument, NULL, OPT_CHECKBOX_ON },
        { "checkbox-off",  required_argument, NULL, OPT_CHECKBOX_OFF },
        { "arrow-nav",     no_argument,       NULL, OPT_ARROW_NAV },
        SC_CLI_BOX_LONGOPTS,
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    SelectArgs args = { 0 };
    int        opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_PROMPT:
            args.opts.prompt = optarg;
            break;
        case OPT_MULTI:
            args.opts.multi = true;
            break;
        case OPT_MAX_VISIBLE:
            if (!sc_cli_parse_int(optarg, &args.opts.max_visible)) {
                return sc_cli_error(ctx, "invalid count '%s'", optarg);
            }
            break;
        case OPT_MARKER:
            args.opts.marker = optarg;
            break;
        case OPT_CURSOR_MARKER:
            args.opts.cursor_marker = optarg;
            break;
        case OPT_CHECKBOX_ON:
            args.opts.checkbox_on = optarg;
            break;
        case OPT_CHECKBOX_OFF:
            args.opts.checkbox_off = optarg;
            break;
        case OPT_ARROW_NAV:
            args.arrow_nav = true;
            break;
        case SC_CLI_OPT_BOXED:
        case SC_CLI_OPT_BORDER:
        case SC_CLI_OPT_BORDER_COLOR:
        case SC_CLI_OPT_BORDER_BG:
        case SC_CLI_OPT_BG:
        case SC_CLI_OPT_PADDING:
        case SC_CLI_OPT_MARGIN:
        case SC_CLI_OPT_WIDTH:
            if (!sc_cli_box_opt(ctx, opt, optarg, &args.opts.box)) {
                return SC_CLI_EXIT_ERROR;
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&args.styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(SELECT_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &args.input)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_select(ctx, &args, argc - optind, argv + optind);
}

static int run_select(ScCliCtx *ctx, SelectArgs *args, int argc,
                      char **argv) {
    SelectSource source = { 0 };
    if (!select_source_init(&source, argc, argv)) {
        return sc_cli_error(ctx, "cannot read items");
    }
    if (source.count == 0) {
        select_source_release(&source);
        return sc_cli_error(ctx, "no items to select from");
    }

    args->opts.hide_summary  = true;
    args->opts.prompt_markup = ctx->markup;
    args->opts.hint          = args->input.hint;
    args->opts.hint_pos      = args->input.hint_pos;
    if (args->input.hide_hint) {
        args->opts.hint_layout = SC_HINT_HIDDEN;
    } else if (args->input.hint_layout != SC_HINT_LAYOUT_DEFAULT) {
        args->opts.hint_layout = args->input.hint_layout;
    }

    const ScCliStyleSlot slots[] = {
        { "prompt",   &args->opts.prompt_style },
        { "selected", &args->opts.selected_style },
        { "summary",  &args->opts.summary_style },
        { "hint",     &args->opts.hint_style },
    };
    if (!sc_cli_apply_styles(ctx, &args->styles, slots,
                             SC_CLI_TABLE_SIZE(slots))) {
        select_source_release(&source);
        return SC_CLI_EXIT_ERROR;
    }

    // Left/Right both end the prompt (RETURN); the cursor item is always
    // valid, so Right submits exactly like Enter. The shortcut pointers go on
    // a local opts copy so `&fired` never escapes into the caller's struct.
    int fired = -1;
    ScShortcut nav[2] = {
        { .chord = sc_key_special(SC_KEY_LEFT),  .id = NAV_BACK_ID,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "back" },
        { .chord = sc_key_special(SC_KEY_RIGHT), .id = NAV_FWD_ID,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "select" },
    };
    ScSelectOpts opts = args->opts;
    if (args->arrow_nav) {
        opts.shortcuts       = nav;
        opts.n_shortcuts     = 2;
        opts.out_shortcut_id = &fired;
    }

    ScSelect *select = sc_select_new(opts);
    if (select == NULL) {
        select_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }
    for (size_t i = 0; i < source.count; i++) {
        sc_select_add(select, source.items[i]);
    }

    size_t *indices = malloc(source.count * sizeof(*indices));
    if (indices == NULL) {
        sc_select_free(select);
        select_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }

    size_t        count  = args->opts.multi ? source.count : 1;
    ScInputStatus status = sc_select_run(select, indices, &count);

    if (status == SC_INPUT_OK && fired != NAV_BACK_ID) {
        for (size_t i = 0; i < count; i++) {
            printf("%s\n", source.items[indices[i]]);
        }
    }

    free(indices);
    sc_select_free(select);
    select_source_release(&source);
    return nav_exit(status, fired);
}


/* ── fuzzy ──────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli fuzzy`. */
typedef struct FuzzyArgs {
    ScFuzzyOpts    opts;
    ScCliInputArgs input;
    ScCliStyleArgs styles;
    char           delim;      /**< Field delimiter for table mode. */
    bool           table_mode; /**< Split items into table columns. */
    bool           header_row; /**< First line provides the headers. */
    bool           arrow_nav;  /**< Bind Left=back / Right=select. */
} FuzzyArgs;

static int run_fuzzy(ScCliCtx *ctx, FuzzyArgs *args, int argc, char **argv);
    static int run_fuzzy_table(ScCliCtx *ctx, FuzzyArgs *args,
                               const SelectSource *source);
        static void free_row_fields(char ***rows, size_t count);

static const char FUZZY_USAGE[] =
    "Usage: sparcli fuzzy [options] [ITEM...]\n"
    "\n"
    "Incrementally fuzzy-search a list and pick one item. Items come\n"
    "from the ITEM arguments or - without arguments - from stdin, one\n"
    "item per line. The chosen item is printed to stdout.\n"
    "\n"
    "With --tsv (or --delim) each line is split into columns and shown\n"
    "as a table; the first field of the chosen row is printed.\n"
    "\n"
    "Options:\n"
    "  --prompt TEXT              Search field label\n"
    "  --max-visible N            Viewport height (default 10)\n"
    "  --tsv                      Tab-separated table view\n"
    "  --delim CHAR               Custom delimiter for the table view\n"
    "  --header-row               First input line provides the headers\n"
    "  --marker STR               Marker before non-cursor rows\n"
    "  --cursor-marker STR        Marker before the cursor row\n"
    "  --search-columns LIST      Table columns to search (e.g. 1,3)\n"
    "  --arrow-nav                Left = back (exit 3), Right = select\n"
    SC_CLI_BOX_USAGE
    SC_CLI_INPUT_USAGE
    "\n"
    "--style elements: prompt, selected, cursor, counter, summary, hint\n"
    "Set the cursor-row colors with --style selected (e.g.\n"
    "--style selected='white on magenta'); in table view selected's\n"
    "background overrides the accent highlight.\n";

int sc_cli_cmd_fuzzy(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_PROMPT = SC_CLI_OPT_CMD_BASE,
        OPT_MAX_VISIBLE,
        OPT_TSV,
        OPT_DELIM,
        OPT_HEADER_ROW,
        OPT_MARKER,
        OPT_CURSOR_MARKER,
        OPT_SEARCH_COLUMNS,
        OPT_ARROW_NAV,
    };
    static const struct option longopts[] = {
        { "prompt",         required_argument, NULL, OPT_PROMPT },
        { "max-visible",    required_argument, NULL, OPT_MAX_VISIBLE },
        { "tsv",            no_argument,       NULL, OPT_TSV },
        { "delim",          required_argument, NULL, OPT_DELIM },
        { "header-row",     no_argument,       NULL, OPT_HEADER_ROW },
        { "marker",         required_argument, NULL, OPT_MARKER },
        { "cursor-marker",  required_argument, NULL, OPT_CURSOR_MARKER },
        { "search-columns", required_argument, NULL, OPT_SEARCH_COLUMNS },
        { "arrow-nav",      no_argument,       NULL, OPT_ARROW_NAV },
        SC_CLI_BOX_LONGOPTS,
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    FuzzyArgs args = { .delim = '\t' };
    int       opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_PROMPT:
            args.opts.prompt = optarg;
            break;
        case OPT_MAX_VISIBLE:
            if (!sc_cli_parse_int(optarg, &args.opts.max_visible)) {
                return sc_cli_error(ctx, "invalid count '%s'", optarg);
            }
            break;
        case OPT_TSV:
            args.table_mode = true;
            break;
        case OPT_DELIM:
            args.table_mode = true;
            args.delim      = optarg[0];
            break;
        case OPT_HEADER_ROW:
            args.header_row = true;
            break;
        case OPT_MARKER:
            args.opts.marker = optarg;
            break;
        case OPT_CURSOR_MARKER:
            args.opts.cursor_marker = optarg;
            break;
        case OPT_SEARCH_COLUMNS:
            if (!parse_search_columns(optarg, &args.opts.search_columns)) {
                return sc_cli_error(ctx, "invalid columns '%s' (e.g. 1,3)",
                                    optarg);
            }
            break;
        case OPT_ARROW_NAV:
            args.arrow_nav = true;
            break;
        case SC_CLI_OPT_BOXED:
        case SC_CLI_OPT_BORDER:
        case SC_CLI_OPT_BORDER_COLOR:
        case SC_CLI_OPT_BORDER_BG:
        case SC_CLI_OPT_BG:
        case SC_CLI_OPT_PADDING:
        case SC_CLI_OPT_MARGIN:
        case SC_CLI_OPT_WIDTH:
            if (!sc_cli_box_opt(ctx, opt, optarg, &args.opts.box)) {
                return SC_CLI_EXIT_ERROR;
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&args.styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(FUZZY_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &args.input)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_fuzzy(ctx, &args, argc - optind, argv + optind);
}

static int run_fuzzy(ScCliCtx *ctx, FuzzyArgs *args, int argc, char **argv) {
    SelectSource source = { 0 };
    if (!select_source_init(&source, argc, argv)) {
        return sc_cli_error(ctx, "cannot read items");
    }
    if (source.count == 0) {
        select_source_release(&source);
        return sc_cli_error(ctx, "no items to search");
    }

    args->opts.hide_summary  = true;
    args->opts.prompt_markup = ctx->markup;
    args->opts.hint          = args->input.hint;
    args->opts.hint_pos      = args->input.hint_pos;
    if (args->input.hide_hint) {
        args->opts.hint_layout = SC_HINT_HIDDEN;
    } else if (args->input.hint_layout != SC_HINT_LAYOUT_DEFAULT) {
        args->opts.hint_layout = args->input.hint_layout;
    }

    const ScCliStyleSlot slots[] = {
        { "prompt",   &args->opts.prompt_style },
        { "selected", &args->opts.selected_style },
        { "cursor",   &args->opts.cursor_style },
        { "counter",  &args->opts.counter_style },
        { "summary",  &args->opts.summary_style },
        { "hint",     &args->opts.hint_style },
    };
    if (!sc_cli_apply_styles(ctx, &args->styles, slots,
                             SC_CLI_TABLE_SIZE(slots))) {
        select_source_release(&source);
        return SC_CLI_EXIT_ERROR;
    }

    if (args->table_mode) {
        int status = run_fuzzy_table(ctx, args, &source);
        select_source_release(&source);
        return status;
    }

    // Left ends the prompt (RETURN, reported as "back"); Right submits via a
    // callback that only closes when a row is matched (so an empty filter
    // never submits a bogus item). The callback's handle is set after new().
    int fired = -1;
    ScShortcut nav[2] = {
        { .chord = sc_key_special(SC_KEY_LEFT),  .id = NAV_BACK_ID,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "back" },
        { .chord = sc_key_special(SC_KEY_RIGHT),
          .mode = SC_SHORTCUT_CALLBACK, .on_fire = fuzzy_forward_cb,
          .hint_label = "select" },
    };
    ScFuzzyOpts opts = args->opts;
    if (args->arrow_nav) {
        opts.shortcuts       = nav;
        opts.n_shortcuts     = 2;
        opts.out_shortcut_id = &fired;
    }

    ScFuzzy *fuzzy = sc_fuzzy_new(opts);
    if (fuzzy == NULL) {
        select_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }
    nav[1].user = fuzzy;   // borrowed shortcuts array: handle reaches the cb
    for (size_t i = 0; i < source.count; i++) {
        sc_fuzzy_add(fuzzy, source.items[i]);
    }

    size_t        picked = 0;
    ScInputStatus status = sc_fuzzy_run(fuzzy, &picked);
    if (status == SC_INPUT_OK && fired != NAV_BACK_ID) {
        printf("%s\n", source.items[picked]);
    }

    sc_fuzzy_free(fuzzy);
    select_source_release(&source);
    return nav_exit(status, fired);
}

/* Table mode: every input line is split at the delimiter into row fields;
   the first line optionally provides the column headers. */
static int run_fuzzy_table(ScCliCtx *ctx, FuzzyArgs *args,
                           const SelectSource *source) {
    /* Split every line into fields. Each row is a NULL-terminated heap
       array of heap strings (the rows outlive the fuzzy run). */
    char ***rows = calloc(source->count, sizeof(*rows));
    if (rows == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    size_t n_cols = 1;
    for (size_t i = 0; i < source->count; i++) {
        size_t fields = 1;
        for (const char *c = source->items[i]; *c != '\0'; c++) {
            fields += (*c == args->delim) ? 1 : 0;
        }
        if (fields > n_cols) {
            n_cols = fields;
        }
    }

    for (size_t i = 0; i < source->count; i++) {
        rows[i] = calloc(n_cols + 1, sizeof(**rows));
        if (rows[i] == NULL) {
            free_row_fields(rows, source->count);
            return sc_cli_error(ctx, "out of memory");
        }

        char *copy   = strdup(source->items[i]);
        char *cursor = copy;
        for (size_t col = 0; col < n_cols && copy != NULL; col++) {
            char *end = strchr(cursor, args->delim);
            if (end != NULL) {
                *end = '\0';
            }
            rows[i][col] = strdup(cursor);
            cursor = (end != NULL) ? end + 1 : cursor + strlen(cursor);
        }
        free(copy);
    }

    size_t first_data = args->header_row ? 1 : 0;
    if (args->header_row && source->count > 0) {
        args->opts.headers = (const char *const *)rows[0];
    }
    args->opts.table  = true;
    args->opts.n_cols = n_cols;

    // Same Left=back / Right=guarded-submit wiring as the plain fuzzy path.
    int fired = -1;
    ScShortcut nav[2] = {
        { .chord = sc_key_special(SC_KEY_LEFT),  .id = NAV_BACK_ID,
          .mode = SC_SHORTCUT_RETURN, .hint_label = "back" },
        { .chord = sc_key_special(SC_KEY_RIGHT),
          .mode = SC_SHORTCUT_CALLBACK, .on_fire = fuzzy_forward_cb,
          .hint_label = "select" },
    };
    ScFuzzyOpts opts = args->opts;
    if (args->arrow_nav) {
        opts.shortcuts       = nav;
        opts.n_shortcuts     = 2;
        opts.out_shortcut_id = &fired;
    }

    ScFuzzy *fuzzy = sc_fuzzy_new(opts);
    if (fuzzy == NULL) {
        free_row_fields(rows, source->count);
        return sc_cli_error(ctx, "out of memory");
    }
    nav[1].user = fuzzy;
    for (size_t i = first_data; i < source->count; i++) {
        sc_fuzzy_add_row(fuzzy, (const char *const *)rows[i], n_cols);
    }

    size_t        picked = 0;
    ScInputStatus status = sc_fuzzy_run(fuzzy, &picked);
    if (status == SC_INPUT_OK && fired != NAV_BACK_ID) {
        printf("%s\n", rows[first_data + picked][0]);
    }

    sc_fuzzy_free(fuzzy);
    free_row_fields(rows, source->count);
    return nav_exit(status, fired);
}

static void free_row_fields(char ***rows, size_t count) {
    for (size_t i = 0; i < count; i++) {
        if (rows[i] == NULL) {
            continue;
        }
        for (size_t col = 0; rows[i][col] != NULL; col++) {
            free(rows[i][col]);
        }
        free(rows[i]);
    }
    free(rows);
}


/* ── date ───────────────────────────────────────────────────────────────── */

static bool parse_iso_date(const char *spec, struct tm *out);

static const char DATE_USAGE[] =
    "Usage: sparcli date [options]\n"
    "\n"
    "Pick a date from a calendar and print it to stdout:\n"
    "  when=$(sparcli date --format %Y-%m-%d)\n"
    "\n"
    "Options:\n"
    "  --prompt TEXT              Heading shown above the calendar\n"
    "  --format FMT               strftime output format (default\n"
    "                             %Y-%m-%d)\n"
    "  --initial YYYY-MM-DD       Initially selected date (default today)\n"
    "  --week-start monday|sunday First day of the week\n"
    "  --header-prev STR          Glyph for the previous-month control\n"
    "  --header-next STR          Glyph for the next-month control\n"
    SC_CLI_BOX_USAGE
    SC_CLI_INPUT_USAGE
    "\n"
    "--style elements: prompt, header, weekday, selected, summary, hint\n";

int sc_cli_cmd_date(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_PROMPT = SC_CLI_OPT_CMD_BASE,
        OPT_FORMAT,
        OPT_INITIAL,
        OPT_WEEK_START,
        OPT_HEADER_PREV,
        OPT_HEADER_NEXT,
    };
    static const struct option longopts[] = {
        { "prompt",      required_argument, NULL, OPT_PROMPT },
        { "format",      required_argument, NULL, OPT_FORMAT },
        { "initial",     required_argument, NULL, OPT_INITIAL },
        { "week-start",  required_argument, NULL, OPT_WEEK_START },
        { "header-prev", required_argument, NULL, OPT_HEADER_PREV },
        { "header-next", required_argument, NULL, OPT_HEADER_NEXT },
        SC_CLI_BOX_LONGOPTS,
        SC_CLI_INPUT_LONGOPTS,
        { 0 },
    };

    ScDatePickerOpts opts       = { 0 };
    ScCliInputArgs   input_args = { 0 };
    ScCliStyleArgs   styles     = { 0 };
    const char      *format     = "%Y-%m-%d";
    const char      *initial    = NULL;

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_PROMPT:
            opts.prompt = optarg;
            break;
        case OPT_FORMAT:
            format = optarg;
            break;
        case OPT_INITIAL:
            initial = optarg;
            break;
        case OPT_WEEK_START:
            if (!sc_cli_parse_week_start(optarg, &opts.week_start)) {
                return sc_cli_error(ctx, "invalid week start '%s'", optarg);
            }
            break;
        case OPT_HEADER_PREV:
            opts.header_prev = optarg;
            break;
        case OPT_HEADER_NEXT:
            opts.header_next = optarg;
            break;
        case SC_CLI_OPT_BOXED:
        case SC_CLI_OPT_BORDER:
        case SC_CLI_OPT_BORDER_COLOR:
        case SC_CLI_OPT_BORDER_BG:
        case SC_CLI_OPT_BG:
        case SC_CLI_OPT_PADDING:
        case SC_CLI_OPT_MARGIN:
        case SC_CLI_OPT_WIDTH:
            if (!sc_cli_box_opt(ctx, opt, optarg, &opts.box)) {
                return SC_CLI_EXIT_ERROR;
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(DATE_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_input_opt(ctx, opt, &input_args)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    /* A zeroed struct tm seeds the picker with today; --initial overrides
       it with an explicit date. */
    struct tm picked = { 0 };
    if (initial != NULL && !parse_iso_date(initial, &picked)) {
        return sc_cli_error(ctx, "invalid date '%s' (expected YYYY-MM-DD)",
                            initial);
    }

    opts.hide_summary  = true;
    opts.prompt_markup = ctx->markup;
    opts.hint          = input_args.hint;
    opts.hint_pos      = input_args.hint_pos;
    if (input_args.hide_hint) {
        opts.hint_layout = SC_HINT_HIDDEN;
    } else if (input_args.hint_layout != SC_HINT_LAYOUT_DEFAULT) {
        opts.hint_layout = input_args.hint_layout;
    }

    const ScCliStyleSlot slots[] = {
        { "prompt",   &opts.prompt_style },
        { "header",   &opts.header_style },
        { "weekday",  &opts.weekday_style },
        { "selected", &opts.selected_style },
        { "summary",  &opts.summary_style },
        { "hint",     &opts.hint_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    ScInputStatus status = sc_datepicker(&picked, opts);
    if (status == SC_INPUT_OK) {
        enum { DATE_BUFFER_SIZE = 256 };
        char buffer[DATE_BUFFER_SIZE] = { 0 };
        if (strftime(buffer, sizeof(buffer), format, &picked) == 0) {
            return sc_cli_error(ctx, "invalid date format '%s'", format);
        }
        printf("%s\n", buffer);
    }
    return sc_cli_input_exit(status);
}

/* Parses a `YYYY-MM-DD` date into `out` (portable strptime substitute). */
static bool parse_iso_date(const char *spec, struct tm *out) {
    enum { MONTHS_PER_YEAR = 12, MAX_MONTH_DAYS = 31, TM_YEAR_BASE = 1900 };

    char *end  = NULL;
    long  year = strtol(spec, &end, 10);
    if (end == spec || *end != '-') {
        return false;
    }

    const char *month_start = end + 1;
    long        month       = strtol(month_start, &end, 10);
    if (end == month_start || *end != '-') {
        return false;
    }

    const char *day_start = end + 1;
    long        day       = strtol(day_start, &end, 10);
    if (end == day_start || *end != '\0') {
        return false;
    }

    if (month < 1 || month > MONTHS_PER_YEAR || day < 1
        || day > MAX_MONTH_DAYS) {
        return false;
    }

    out->tm_year = (int)year - TM_YEAR_BASE;
    out->tm_mon  = (int)month - 1;
    out->tm_mday = (int)day;
    return true;
}


/* ── shared item source ─────────────────────────────────────────────────── */

static bool select_source_init(SelectSource *source, int argc, char **argv) {
    if (argc > 0) {
        source->items = argv;
        source->count = (size_t)argc;
        return true;
    }

    source->data = sc_cli_read_source(NULL);
    if (source->data == NULL) {
        return false;
    }
    source->lines = sc_cli_split_lines(source->data, &source->count);
    if (source->lines == NULL) {
        free(source->data);
        source->data = NULL;
        return false;
    }
    source->items = source->lines;
    return true;
}

static void select_source_release(SelectSource *source) {
    free(source->lines);
    free(source->data);
    source->lines = NULL;
    source->data  = NULL;
}

/* Parses a comma-separated list of 1-based column indices into a bitmask
   (column 1 -> bit 0). Indices above 64 are ignored. Returns false on an
   empty list or a non-positive / unparseable index. */
static bool parse_search_columns(const char *spec, uint64_t *out) {
    enum { MAX_COLUMN = 64 };
    uint64_t    mask   = 0;
    const char *cursor = spec;
    bool        any    = false;

    while (*cursor != '\0') {
        char *end   = NULL;
        long  value = strtol(cursor, &end, 10);
        if (end == cursor || value < 1) {
            return false;
        }
        if (value <= MAX_COLUMN) {
            mask |= (uint64_t)1 << (value - 1);
        }
        any    = true;
        cursor = end;
        if (*cursor == ',') {
            cursor++;
        } else if (*cursor != '\0') {
            return false;
        }
    }

    if (!any) {
        return false;
    }
    *out = mask;
    return true;
}
