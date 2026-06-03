/**
 * @file cmd_output.c
 * Simple text output subcommands: `print`, `rule`, `alert` and `badge`.
 */
#include "cli_commands.h"
#include "cli_common.h"
#include "cli_parse.h"

#include "sparcli.h"

#include <getopt.h>
#include <stdlib.h>
#include <string.h>


/* ── print ──────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli print`. */
typedef struct PrintArgs {
    ScHAlign halign;
    bool     has_halign;
    int      width;
} PrintArgs;

static int run_print(ScCliCtx *ctx, const PrintArgs *args, int argc,
                     char **argv);

static const char PRINT_USAGE[] =
    "Usage: sparcli print [options] [TEXT...]\n"
    "\n"
    "Print markup-styled text. Multiple TEXT arguments are joined with\n"
    "spaces; without TEXT the text is read from stdin.\n"
    "\n"
    "Options:\n"
    "  --align left|center|right  Align the text block\n"
    "  --width N                  Alignment width (0 = terminal width)\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_print(ScCliCtx *ctx, int argc, char **argv) {
    enum { OPT_ALIGN = SC_CLI_OPT_CMD_BASE, OPT_WIDTH };
    static const struct option longopts[] = {
        { "align", required_argument, NULL, OPT_ALIGN },
        { "width", required_argument, NULL, OPT_WIDTH },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    PrintArgs args = { 0 };
    int       opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_ALIGN:
            if (!sc_cli_parse_halign(optarg, &args.halign)) {
                return sc_cli_error(ctx, "invalid alignment '%s'", optarg);
            }
            args.has_halign = true;
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(PRINT_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_print(ctx, &args, argc - optind, argv + optind);
}

static int run_print(ScCliCtx *ctx, const PrintArgs *args, int argc,
                     char **argv) {
    char *content = sc_cli_content(argc, argv);
    if (content == NULL) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScText *text = sc_cli_text(ctx, content);
    free(content);
    if (text == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    if (args->has_halign) {
        sc_align_text(text, args->halign, args->width);
    } else {
        sc_print_text(text);
        fputc('\n', sc_output_stream());
    }

    sc_text_free(text);
    return SC_CLI_EXIT_OK;
}


/* ── rule ───────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli rule`. */
typedef struct RuleArgs {
    ScRuleOpts opts;
} RuleArgs;

static int run_rule(ScCliCtx *ctx, RuleArgs *args, int argc, char **argv);

static const char RULE_USAGE[] =
    "Usage: sparcli rule [options] [TITLE]\n"
    "\n"
    "Draw a horizontal rule across the terminal, optionally labelled\n"
    "with a centered TITLE.\n"
    "\n"
    "Options:\n"
    "  --border STYLE             Line style: none|ascii|single|double|\n"
    "                             rounded|thick (default: single)\n"
    "  --color COLOR              Line color (name, #RRGGBB or R,G,B)\n"
    "  --align left|center|right  Title position (default: center)\n"
    "  --title-pad N              Spaces around the title\n"
    "  --width N                  Fixed rule width (0 = terminal width)\n"
    "  --margin EDGES             Outer margin: N or T,R,B,L\n"
    "  (style the title with markup, e.g. rule \"[bold cyan]Title[/]\")\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_rule(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_BORDER = SC_CLI_OPT_CMD_BASE,
        OPT_COLOR,
        OPT_ALIGN,
        OPT_TITLE_PAD,
        OPT_WIDTH,
        OPT_MARGIN,
    };
    static const struct option longopts[] = {
        { "border",    required_argument, NULL, OPT_BORDER },
        { "color",     required_argument, NULL, OPT_COLOR },
        { "align",     required_argument, NULL, OPT_ALIGN },
        { "title-pad", required_argument, NULL, OPT_TITLE_PAD },
        { "width",     required_argument, NULL, OPT_WIDTH },
        { "margin",    required_argument, NULL, OPT_MARGIN },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    RuleArgs args = {
        .opts = { .type  = SC_BORDER_SINGLE,
                  .title = { .halign = SC_ALIGN_CENTER } },
    };
    int      opt  = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_BORDER:
            if (!sc_cli_parse_border(optarg, &args.opts.type)) {
                return sc_cli_error(ctx, "invalid border '%s'", optarg);
            }
            break;
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.color)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_ALIGN:
            if (!sc_cli_parse_halign(optarg, &args.opts.title.halign)) {
                return sc_cli_error(ctx, "invalid alignment '%s'", optarg);
            }
            break;
        case OPT_TITLE_PAD:
            if (!sc_cli_parse_int(optarg, &args.opts.title.pad)) {
                return sc_cli_error(ctx, "invalid pad '%s'", optarg);
            }
            break;
        case OPT_WIDTH:
            if (!sc_cli_parse_int(optarg, &args.opts.width)) {
                return sc_cli_error(ctx, "invalid width '%s'", optarg);
            }
            break;
        case OPT_MARGIN:
            if (!sc_cli_parse_edges(optarg, &args.opts.margin)) {
                return sc_cli_error(ctx, "invalid margin '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_HELP:
            fputs(RULE_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    return run_rule(ctx, &args, argc - optind, argv + optind);
}

static int run_rule(ScCliCtx *ctx, RuleArgs *args, int argc, char **argv) {
    if (argc == 0) {
        sc_rule_str(NULL, args->opts);
        return SC_CLI_EXIT_OK;
    }

    char *title = sc_cli_content(argc, argv);
    if (title == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    ScText *title_text = sc_cli_text(ctx, title);
    free(title);
    if (title_text == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    sc_rule_text(title_text, args->opts);
    sc_text_free(title_text);
    return SC_CLI_EXIT_OK;
}


/* ── alert ──────────────────────────────────────────────────────────────── */

static int run_alert(ScCliCtx *ctx, ScAlertType type, int argc,
                     char **argv);

static const char ALERT_USAGE[] =
    "Usage: sparcli alert [options] LEVEL [TEXT...]\n"
    "\n"
    "Show a full-width alert panel with a level icon and color.\n"
    "LEVEL is one of: info, debug, warning, error, success.\n"
    "Without TEXT the alert body is read from stdin.\n"
    "\n"
    "Options:\n"
    SC_CLI_COMMON_USAGE;

int sc_cli_cmd_alert(ScCliCtx *ctx, int argc, char **argv) {
    static const struct option longopts[] = {
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    int opt = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        if (opt == SC_CLI_OPT_HELP) {
            fputs(ALERT_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        }
        if (!sc_cli_common_opt(ctx, opt)) {
            return sc_cli_error(ctx, "invalid option (see --help)");
        }
    }

    int    positionals = argc - optind;
    char **positional  = argv + optind;
    if (positionals < 1) {
        return sc_cli_error(ctx, "missing alert level (see --help)");
    }

    ScAlertType type = SC_ALERT_INFO;
    if (!sc_cli_parse_alert(positional[0], &type)) {
        return sc_cli_error(ctx, "invalid alert level '%s'", positional[0]);
    }

    return run_alert(ctx, type, positionals - 1, positional + 1);
}

static int run_alert(ScCliCtx *ctx, ScAlertType type, int argc,
                     char **argv) {
    char *content = sc_cli_content(argc, argv);
    if (content == NULL) {
        return sc_cli_error(ctx, "cannot read input");
    }

    ScText *text = sc_cli_text(ctx, content);
    free(content);
    if (text == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    sc_alert_text(type, text);
    sc_text_free(text);
    return SC_CLI_EXIT_OK;
}


/* ── badge ──────────────────────────────────────────────────────────────── */

/** Parsed arguments of `sparcli badge`. */
typedef struct BadgeArgs {
    ScBadgeOpts opts;
} BadgeArgs;

static int run_badge(ScCliCtx *ctx, const BadgeArgs *args, int argc,
                     char **argv);

static const char BADGE_USAGE[] =
    "Usage: sparcli badge [options] TEXT...\n"
    "\n"
    "Print an inline styled badge like [ok] or [v1.2]. The badge text is\n"
    "taken literally (no markup); style it with the options below.\n"
    "\n"
    "Options:\n"
    "  --color COLOR              Badge text color\n"
    "  --bg COLOR                 Badge background color\n"
    "  --bold                     Bold badge text\n"
    "  --left STR                 Left cap (default \"[\")\n"
    "  --right STR                Right cap (default \"]\")\n"
    "  --pad N                    Spaces inside the caps\n"
    SC_CLI_COMMON_USAGE
    "\n"
    "--style elements: text (overrides --color/--bg/--bold)\n";

int sc_cli_cmd_badge(ScCliCtx *ctx, int argc, char **argv) {
    enum {
        OPT_COLOR = SC_CLI_OPT_CMD_BASE,
        OPT_BG,
        OPT_BOLD,
        OPT_LEFT,
        OPT_RIGHT,
        OPT_PAD,
    };
    static const struct option longopts[] = {
        { "color", required_argument, NULL, OPT_COLOR },
        { "bg",    required_argument, NULL, OPT_BG },
        { "bold",  no_argument,       NULL, OPT_BOLD },
        { "left",  required_argument, NULL, OPT_LEFT },
        { "right", required_argument, NULL, OPT_RIGHT },
        { "pad",   required_argument, NULL, OPT_PAD },
        SC_CLI_COMMON_LONGOPTS,
        { 0 },
    };

    BadgeArgs      args   = { 0 };
    ScCliStyleArgs styles = { 0 };
    int            opt    = 0;
    while ((opt = getopt_long(argc, argv, "", longopts, NULL)) != -1) {
        switch (opt) {
        case OPT_COLOR:
            if (!sc_cli_parse_color(optarg, &args.opts.text_style.fg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_BG:
            if (!sc_cli_parse_color(optarg, &args.opts.text_style.bg)) {
                return sc_cli_error(ctx, "invalid color '%s'", optarg);
            }
            break;
        case OPT_BOLD:
            args.opts.text_style.attr |= SC_TEXT_ATTR_BOLD;
            break;
        case OPT_LEFT:
            args.opts.left_cap = optarg;
            break;
        case OPT_RIGHT:
            args.opts.right_cap = optarg;
            break;
        case OPT_PAD:
            if (!sc_cli_parse_int(optarg, &args.opts.pad)) {
                return sc_cli_error(ctx, "invalid pad '%s'", optarg);
            }
            break;
        case SC_CLI_OPT_STYLE:
            sc_cli_style_collect(&styles, optarg);
            break;
        case SC_CLI_OPT_HELP:
            fputs(BADGE_USAGE, stdout);
            return SC_CLI_EXIT_OK;
        default:
            if (!sc_cli_common_opt(ctx, opt)) {
                return sc_cli_error(ctx, "invalid option (see --help)");
            }
            break;
        }
    }

    const ScCliStyleSlot slots[] = {
        { "text", &args.opts.text_style },
    };
    if (!sc_cli_apply_styles(ctx, &styles, slots, SC_CLI_TABLE_SIZE(slots))) {
        return SC_CLI_EXIT_ERROR;
    }

    return run_badge(ctx, &args, argc - optind, argv + optind);
}

static int run_badge(ScCliCtx *ctx, const BadgeArgs *args, int argc,
                     char **argv) {
    if (argc == 0) {
        return sc_cli_error(ctx, "missing badge text (see --help)");
    }

    char *text = sc_cli_content(argc, argv);
    if (text == NULL) {
        return sc_cli_error(ctx, "out of memory");
    }

    sc_print_badge(text, args->opts);
    fputc('\n', sc_output_stream());
    free(text);
    return SC_CLI_EXIT_OK;
}
