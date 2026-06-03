/**
 * @file cmd_layout.c
 * Container/layout subcommands: `panel`, `list`, `kv` and `columns`.
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"
#include "cli_stdin.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>

/** Item source shared by list/columns: argv items or stdin lines. */
typedef struct ItemSource {
    char  **items; /**< Item strings (borrowed from argv or `lines`). */
    size_t  count; /**< Number of items. */
    char   *data;  /**< Owned stdin buffer (`NULL` for argv items). */
    char  **lines; /**< Owned line array (`NULL` for argv items). */
} ItemSource;

static bool item_source_init(ItemSource *source, int argc, char **argv);
static void item_source_release(ItemSource *source);


/* ── panel ──────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli panel`. */
typedef struct PanelArgs {
    ScPanelOpts opts;
    const char *title;    /**< Raw title text (markup-parsed at render). */
    const char *subtitle; /**< Raw subtitle text. */
} PanelArgs;

static int run_panel(ScCliCtx *ctx, PanelArgs *args, int argc, char **argv);
    static ScText *apply_title(const ScCliCtx *ctx, ScTitle *title,
                               const char *raw);

static const char PANEL_USAGE[] =
    "Usage: sparcli panel [options] [TEXT...]\n"
    "\n"
    "Draw a bordered panel around TEXT (or stdin when TEXT is omitted).\n"
    "\n"
    "Options:\n"
    "  --title TEXT               Panel title (top edge)\n"
    "  --subtitle TEXT            Second title (bottom edge, right)\n"
    "  --title-align left|center|right   Title alignment\n"
    "  --border STYLE             none|ascii|single|double|rounded|thick\n"
    "  --color COLOR              Border color (name, #RRGGBB or R,G,B)\n"
    "  --bg COLOR                 Content background color\n"
    "  --align left|center|right  Content alignment\n"
    "  --padding EDGES            Inner padding: N, V,H or T,R,B,L\n"
    "  --margin EDGES             Outer margin: N, V,H or T,R,B,L\n"
    "  --width N                  Panel width (0 = fit content)\n"
    "  --full-width               Stretch to the terminal width\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_panel(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_TITLE = SC_CLI_OPT_CMD_BASE,
        OPT_SUBTITLE,
        OPT_TITLE_ALIGN,
        OPT_BORDER,
        OPT_COLOR,
        OPT_BG,
        OPT_ALIGN,
        OPT_PADDING,
        OPT_MARGIN,
        OPT_WIDTH,
        OPT_FULL_WIDTH,
    };
    static const struct option longopts[] = {
        { "title",       required_argument, NULL, OPT_TITLE },
        { "subtitle",    required_argument, NULL, OPT_SUBTITLE },
        { "title-align", required_argument, NULL, OPT_TITLE_ALIGN },
        { "border",      required_argument, NULL, OPT_BORDER },
        { "color",       required_argument, NULL, OPT_COLOR },
        { "bg",          required_argument, NULL, OPT_BG },
        { "align",       required_argument, NULL, OPT_ALIGN },
        { "padding",     required_argument, NULL, OPT_PADDING },
        { "margin",      required_argument, NULL, OPT_MARGIN },
        { "width",       required_argument, NULL, OPT_WIDTH },
        { "full-width",  no_argument,       NULL, OPT_FULL_WIDTH },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    PanelArgs args = {
        .opts = {
            .border   = { .type = SC_BORDER_ROUNDED },
            .padding  = { .left = 1, .right = 1 },
            .title    = { .pad = 1 },
            .subtitle = { .pad    = 1,
                          .halign = SC_ALIGN_RIGHT,
                          .pos    = SC_POSITION_BOTTOM },
        },
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_TITLE:
            args.title = optarg;
            break;
        case OPT_SUBTITLE:
            args.subtitle = optarg;
            break;
        case OPT_TITLE_ALIGN:
            if (!sc_cli_parse_halign(optarg, &args.opts.title.halign)) {
                return sc_cli_error(ctx, "invalid alignment '%s'", optarg);
            }
            break;
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &args.opts.border.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.border.color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_BG:
            if (!sc_cli_parse_color(optarg, &args.opts.bg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_ALIGN:
            if (!sc_cli_parse_halign(optarg, &args.opts.content_align)) {
                return sc_cli_error(ctx, "invalid alignment '%s'", optarg);
            }
            break;
        case OPT_PADDING:
            if (!sc_cli_parse_edges(optarg, &args.opts.padding)) {
                return sc_cli_error(ctx, "invalid padding '%s'", optarg);
            }
            break;
        case OPT_MARGIN:
            if (!sc_cli_parse_edges(optarg, &args.opts.margin)) {
                return sc_cli_error(ctx, "invalid margin '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_FULL_WIDTH:
            args.opts.full_width = true;
            break;
        case SC_CLI_OPT_HELP:
            fputs(PANEL_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_panel(ctx, &args, argc - optind, argv + optind);
}

static int run_panel(ScCliCtx *ctx, PanelArgs *args, int argc, char **argv) {
    char *content = sc_cli_content(argc, argv);
    if (content == NULL) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScText *content_text = sc_cli_text(ctx, content);
    free(content);
    if (content_text == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    ScText *title_text    = apply_title(ctx, &args->opts.title,
                                        args->title);
    ScText *subtitle_text = apply_title(ctx, &args->opts.subtitle,
                                        args->subtitle);

    sc_panel_text(content_text, args->opts);

    sc_text_free(content_text);
    sc_text_free(title_text);
    sc_text_free(subtitle_text);
    return SC_CLI_EXIT_OK;
}

/* Wires `raw` into a ScTitle: markup goes through `rich_text` (with the
   raw string kept as `text` fallback, matching the library's own usage),
   plain text through `text` only. Returns the parsed rich title (caller
   frees after rendering) or NULL when plain/absent. */
static ScText *apply_title(const ScCliCtx *ctx, ScTitle *title,
                           const char *raw) {
    if (raw == NULL) {
        return NULL;
    }

    title->text = raw;
    if (!ctx->markup) {
        return NULL;
    }

    ScText *rich = sc_markup_parse(raw);
    if (rich == NULL) {
        return NULL; /* fall back to the plain text on alloc failure */
    }
    title->rich_text = rich;
    return rich;
}


/* ── list ───────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli list`. */
typedef struct ListArgs {
    ScListOpts opts;
} ListArgs;

static int run_list(ScCliCtx *ctx, const ListArgs *args, int argc,
                    char **argv);

static const char LIST_USAGE[] =
    "Usage: sparcli list [options] [ITEM...]\n"
    "\n"
    "Render a bulleted or numbered list. Each ITEM argument is one list\n"
    "entry; without ITEM arguments each stdin line becomes an entry.\n"
    "\n"
    "Options:\n"
    "  --marker STYLE             bullet|number|alpha|alpha-upper|\n"
    "                             roman|roman-upper (default: bullet)\n"
    "  --bullet STR               Bullet character (marker 'bullet' only)\n"
    "  --marker-color COLOR       Marker color\n"
    "  --marker-prefix STR        Text before the marker value\n"
    "  --marker-suffix STR        Text after the marker value\n"
    "  --indent N                 Left indent in columns\n"
    "  --gap N                    Blank lines between items\n"
    "  --margin EDGES             Outer margin: N or T,R,B,L\n"
    "  --width N                  List width (0 = terminal width)\n"
    SC_CLI_COMMON_USAGE
    "\n"
    "--style elements: marker (overrides --marker-color)\n";

int sc_cli_cmd_list(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_MARKER = SC_CLI_OPT_CMD_BASE,
        OPT_BULLET,
        OPT_MARKER_COLOR,
        OPT_MARKER_PREFIX,
        OPT_MARKER_SUFFIX,
        OPT_INDENT,
        OPT_GAP,
        OPT_MARGIN,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "marker",        required_argument, NULL, OPT_MARKER },
        { "bullet",        required_argument, NULL, OPT_BULLET },
        { "marker-color",  required_argument, NULL, OPT_MARKER_COLOR },
        { "marker-prefix", required_argument, NULL, OPT_MARKER_PREFIX },
        { "marker-suffix", required_argument, NULL, OPT_MARKER_SUFFIX },
        { "indent",        required_argument, NULL, OPT_INDENT },
        { "gap",           required_argument, NULL, OPT_GAP },
        { "margin",        required_argument, NULL, OPT_MARGIN },
        { "width",         required_argument, NULL, OPT_WIDTH },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    ListArgs       args   = { 0 };
    ScCliStyleArgs styles = { 0 };
    int            opt    = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_MARKER:
            if (!sc_cli_parse_list_marker(optarg, &args.opts.marker)) {
                return sc_cli_error(ctx, "invalid marker '%s'", optarg);
            }
            break;
        case OPT_BULLET:
            args.opts.bullet = optarg;
            break;
        case OPT_MARKER_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.marker_style.fg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_MARKER_PREFIX:
            args.opts.marker_prefix = optarg;
            break;
        case OPT_MARKER_SUFFIX:
            args.opts.marker_suffix = optarg;
            break;
        case OPT_INDENT:
            if (!sc_cli_parse_int(optarg, &args.opts.indent)) {
                return sc_cli_error(ctx, "invalid indent '%s'", optarg);
            }
            break;
        case OPT_GAP:
            if (!sc_cli_parse_int(optarg, &args.opts.item_gap)) {
                return sc_cli_error(ctx, "invalid gap '%s'", optarg);
            }
            break;
        case OPT_MARGIN:
            if (!sc_cli_parse_edges(optarg, &args.opts.margin)) {
                return sc_cli_error(ctx, "invalid margin '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(LIST_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    const ScCliStyleSlot slots[] = {
        { "marker", &args.opts.marker_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    return run_list(ctx, &args, argc - optind, argv + optind);
}

static int run_list(ScCliCtx *ctx, const ListArgs *args, int argc,
                    char **argv) {
    ItemSource source = { 0 };
    if (!item_source_init(&source, argc, argv)) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScList *list = sc_list_new(args->opts);
    if (list == NULL) {
        item_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }

    /* Item texts are borrowed by the list until print, so they are
       collected and freed only after rendering. */
    ScText **texts = calloc(source.count > 0 ? source.count : 1,
                            sizeof(*texts));
    if (texts == NULL) {
        sc_list_free(list);
        item_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }

    for (size_t i = 0; i < source.count; i++) {
        texts[i] = sc_cli_text(ctx, source.items[i]);
        if (texts[i] != NULL) {
            sc_list_add_text(list, texts[i]);
        }
    }

    sc_list_print(list);

    sc_list_free(list);
    for (size_t i = 0; i < source.count; i++) {
        sc_text_free(texts[i]);
    }
    free(texts);
    item_source_release(&source);
    return SC_CLI_EXIT_OK;
}


/* ── kv ─────────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli kv`. */
typedef struct KvArgs {
    ScKVOpts    opts;
    char        delim; /**< Input delimiter; 0 = auto (tab, then colon). */
    const char *file;  /**< Input file ("-"/NULL = stdin). */
} KvArgs;

static int run_kv(ScCliCtx *ctx, const KvArgs *args);
    static bool split_kv_line(char *line, char delim, const char **key,
                              const char **value);

static const char KV_USAGE[] =
    "Usage: sparcli kv [options] [FILE]\n"
    "\n"
    "Render 'key: value' (or 'key<TAB>value') lines from FILE or stdin\n"
    "as an aligned key/value list. Values are taken literally (the kv\n"
    "widget does not support inline markup).\n"
    "\n"
    "Options:\n"
    "  --delim CHAR               Input delimiter (default: tab or ':')\n"
    "  --sep STR                  Output separator (default: two spaces)\n"
    "  --key-width N              Fixed key column width (0 = widest key)\n"
    "  --key-color COLOR          Key color\n"
    "  --val-color COLOR          Value color\n"
    "  --wrap                     Word-wrap long values\n"
    "  --gap N                    Blank lines between items\n"
    "  --margin EDGES             Outer margin: N or T,R,B,L\n"
    "  --width N                  Total width (0 = terminal width)\n"
    SC_CLI_COMMON_USAGE
    "\n"
    "--style elements: key (overrides --key-color), val (overrides --val-color)\n";

int sc_cli_cmd_kv(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_DELIM = SC_CLI_OPT_CMD_BASE,
        OPT_SEP,
        OPT_KEY_WIDTH,
        OPT_KEY_COLOR,
        OPT_VAL_COLOR,
        OPT_WRAP,
        OPT_GAP,
        OPT_MARGIN,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "delim",     required_argument, NULL, OPT_DELIM },
        { "sep",       required_argument, NULL, OPT_SEP },
        { "key-width", required_argument, NULL, OPT_KEY_WIDTH },
        { "key-color", required_argument, NULL, OPT_KEY_COLOR },
        { "val-color", required_argument, NULL, OPT_VAL_COLOR },
        { "wrap",      no_argument,       NULL, OPT_WRAP },
        { "gap",       required_argument, NULL, OPT_GAP },
        { "margin",    required_argument, NULL, OPT_MARGIN },
        { "width",     required_argument, NULL, OPT_WIDTH },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    KvArgs         args   = { 0 };
    ScCliStyleArgs styles = { 0 };
    int            opt    = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_DELIM:
            args.delim = optarg[0];
            break;
        case OPT_SEP:
            args.opts.sep = optarg;
            break;
        case OPT_KEY_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.key_width)) {
                return sc_cli_error(ctx, "invalid key width '%s'", optarg);
            }
            break;
        case OPT_KEY_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.key_style.fg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_VAL_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.val_style.fg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_WRAP:
            args.opts.wrap_val = true;
            break;
        case OPT_GAP:
            if (!sc_cli_parse_int(optarg, &args.opts.item_gap)) {
                return sc_cli_error(ctx, "invalid gap '%s'", optarg);
            }
            break;
        case OPT_MARGIN:
            if (!sc_cli_parse_edges(optarg, &args.opts.margin)) {
                return sc_cli_error(ctx, "invalid margin '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(KV_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    const ScCliStyleSlot slots[] = {
        { "key", &args.opts.key_style },
        { "val", &args.opts.val_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    if (optind < argc) {
        args.file = argv[optind];
    }
    return run_kv(ctx, &args);
}

static int run_kv(ScCliCtx *ctx, const KvArgs *args) {
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

    ScKV *kv = sc_kv_new(args->opts);
    if (kv == NULL) {
        free(lines);
        free(data);
        return sc_cli_error(ctx, "out of memory");
    }

    for (size_t i = 0; i < line_count; i++) {
        if (lines[i][0] == '\0') {
            continue; /* skip blank lines */
        }
        const char *key   = NULL;
        const char *value = NULL;
        if (!split_kv_line(lines[i], args->delim, &key, &value)) {
            sc_kv_free(kv);
            free(lines);
            free(data);
            return sc_cli_error(ctx, "line %zu has no key/value delimiter",
                                i + 1);
        }
        sc_kv_add(kv, key, value);
    }

    sc_kv_print(kv);
    sc_kv_free(kv);
    free(lines);
    free(data);
    return SC_CLI_EXIT_OK;
}

/* Splits `line` in place at the first delimiter. With `delim == 0` a tab
   is tried first, then a colon. Whitespace after the delimiter is
   skipped. */
static bool split_kv_line(char *line, char delim, const char **key,
                          const char **value) {
    char *split = NULL;
    if (delim != 0) {
        split = strchr(line, delim);
    } else {
        split = strchr(line, '\t');
        if (split == NULL) {
            split = strchr(line, ':');
        }
    }
    if (split == NULL) {
        return false;
    }

    *split = '\0';
    split++;
    while (*split == ' ' || *split == '\t') {
        split++;
    }

    *key   = line;
    *value = split;
    return true;
}


/* ── columns ────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli columns`. */
typedef struct ColumnsArgs {
    ScColumnsOpts opts;
} ColumnsArgs;

static int run_columns(ScCliCtx *ctx, const ColumnsArgs *args, int argc,
                       char **argv);

static const char COLUMNS_USAGE[] =
    "Usage: sparcli columns [options] [ITEM...]\n"
    "\n"
    "Lay out items side by side. Each ITEM argument (or each stdin line)\n"
    "becomes one column.\n"
    "\n"
    "Options:\n"
    "  --gap N                    Spaces between columns\n"
    "  --sep STYLE                Separator: none|ascii|single|double|\n"
    "                             rounded|thick (default: none)\n"
    "  --sep-color COLOR          Separator color\n"
    "  --width N                  Total width (0 = fit content)\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_columns(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_GAP = SC_CLI_OPT_CMD_BASE,
        OPT_SEP,
        OPT_SEP_COLOR,
        OPT_WIDTH,
    };
    static const struct option longopts[] = {
        { "gap",       required_argument, NULL, OPT_GAP },
        { "sep",       required_argument, NULL, OPT_SEP },
        { "sep-color", required_argument, NULL, OPT_SEP_COLOR },
        { "width",     required_argument, NULL, OPT_WIDTH },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    ColumnsArgs args = { 0 };
    int         opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_GAP:
            if (!sc_cli_parse_int(optarg, &args.opts.gap)) {
                return sc_cli_error(ctx, "invalid gap '%s'", optarg);
            }
            break;
        case OPT_SEP:
            if (!sc_cli_parse_border(optarg, &args.opts.sep.type)) {
                return sc_cli_error(ctx, "invalid separator '%s'", optarg);
            }
            break;
        case OPT_SEP_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.sep.color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.total_width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(COLUMNS_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_columns(ctx, &args, argc - optind, argv + optind);
}

static int run_columns(ScCliCtx *ctx, const ColumnsArgs *args, int argc,
                       char **argv) {
    ItemSource source = { 0 };
    if (!item_source_init(&source, argc, argv)) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScColumns *columns = sc_columns_new(args->opts);
    if (columns == NULL) {
        item_source_release(&source);
        return sc_cli_error(ctx, "out of memory");
    }

    /* Columns capture content eagerly at add time, so each item text can
       be freed right after adding it. */
    for (size_t i = 0; i < source.count; i++) {
        ScText *text = sc_cli_text(ctx, source.items[i]);
        if (text == NULL) {
            continue;
        }
        sc_columns_add_text(columns, text, (ScColItem){ 0 });
        sc_text_free(text);
    }

    sc_columns_print(columns);
    sc_columns_free(columns);
    item_source_release(&source);
    return SC_CLI_EXIT_OK;
}


/* ── shared item source ─────────────────────────────────────────────────── */

/* Fills `source` with argv items, or with stdin lines when argc == 0. */
static bool item_source_init(ItemSource *source, int argc, char **argv) {
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

static void item_source_release(ItemSource *source) {
    free(source->lines);
    free(source->data);
    source->lines = NULL;
    source->data  = NULL;
}
