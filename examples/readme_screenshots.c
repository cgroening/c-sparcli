/*
 * readme_screenshots.c
 *
 * Generates the four screenshots referenced in README.md:
 *
 *   1  hero      – panel + striped table + two-column layout + progress bar
 *   2  quickstart – output of the Quick Start snippet
 *   3  gallery   – one representative state per widget
 *   4  markup    – before/after view of the inline markup syntax
 *
 * Build (after `make`):
 *   cc -std=c11 -Iinclude examples/readme_screenshots.c libsparcli.a -o readme_shots
 *
 * Run:
 *   ./readme_shots            # all four screenshots, separated by rules
 *   ./readme_shots 1          # only the hero
 *   ./readme_shots 2          # only the quick-start snippet
 *   ./readme_shots 3          # only the widget gallery
 *   ./readme_shots 4          # only the markup before/after
 */

#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/* ─────────────────────────────────────────────────────────── styles ── */

static const ScTextStyle s_plain  = { SC_TEXT_ATTR_NONE,   SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
static const ScTextStyle s_bold   = { SC_TEXT_ATTR_BOLD,   SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
static const ScTextStyle s_dim    = { SC_TEXT_ATTR_DIM,    SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
static const ScTextStyle s_italic = { SC_TEXT_ATTR_ITALIC, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };

static const ScTextStyle s_b_cyan    = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,    SC_ANSI_COLOR_NONE };
static const ScTextStyle s_b_green   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN,   SC_ANSI_COLOR_NONE };
static const ScTextStyle s_b_yellow  = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW,  SC_ANSI_COLOR_NONE };
static const ScTextStyle s_b_magenta = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE };


/* ─────────────────────────────────────────────────────────── helpers ── */

static void section_rule(const char *title) {
    sc_rule_str(title, (ScRuleOpts){
        .type        = SC_BORDER_DOUBLE,
        .color       = SC_ANSI_COLOR_NONE,
        .title.style = s_bold,
        .title.halign = SC_ALIGN_CENTER,
        .title.pad   = 1,
        .margin      = { 1, 0, 1, 0 },
    });
}

/* Captures a single fixed-width titled rule for use as a stacked column item. */
static ScRendered *capture_titled_rule(
    const char *title, ScBorderType type, ScColor color, ScTextStyle style,
    int width
) {
    return sc_capture_rule_str(title, (ScRuleOpts){
        .type        = type,
        .color       = color,
        .title.style = style,
        .title.halign = SC_ALIGN_CENTER,
        .title.pad   = 1,
        .width       = width,
    });
}


/* ───────────────────────────────────────────────── 1. hero collage ── */

static void shot_hero(void) {
    /* Top: full-width rounded panel with a styled title. */
    {
        ScText *body = sc_text_new();
        sc_text_append(body, "A C11 library for ", s_plain);
        sc_text_append(body, "styled terminal output",   s_b_cyan);
        sc_text_append(body, " – panels, tables, ",      s_plain);
        sc_text_append(body, "columns",                  s_b_green);
        sc_text_append(body, ", lists, progress bars",   s_plain);
        sc_text_append(body, " and Rich-compatible ",    s_plain);
        sc_text_append(body, "[markup]",                 s_b_yellow);
        sc_text_append(body, ".",                        s_plain);

        sc_panel_text(body, (ScPanelOpts){
            .border = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_CYAN },
            .title  = {
                .text  = " sparcli ",
                .style = s_b_cyan,
                .halign = SC_ALIGN_CENTER,
                .pad   = 1,
                .pos   = SC_POSITION_TOP,
            },
            .padding       = { 0, 2, 0, 2 },
            .content_align = SC_ALIGN_CENTER,
            .full_width    = 1,
        });
        sc_text_free(body);
    }

    printf("\n");

    /* Middle: striped table on the left, mini-list on the right. */
    {
        ScColor stripe = sc_color_from_rgb(40, 40, 60);

        ScTableData *t = sc_table_new();
        sc_table_add_column(t, "Service", (ScColOpts){
            0, 0, 14, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(t, "Status", (ScColOpts){
            0, 0, 10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_column(t, "Uptime", (ScColOpts){
            0, 0, 9, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
        });
        sc_table_add_row(t, (ScCell[]){
            sc_cell_m("api-gateway"),
            sc_cell_m("[bold green]● OK[/]"),
            sc_cell("99.98%"),
        }, 3);
        sc_table_add_row(t, (ScCell[]){
            sc_cell_m("auth"),
            sc_cell_m("[bold green]● OK[/]"),
            sc_cell("99.91%"),
        }, 3);
        sc_table_add_row(t, (ScCell[]){
            sc_cell_m("billing"),
            sc_cell_m("[bold yellow]● WARN[/]"),
            sc_cell("97.40%"),
        }, 3);
        sc_table_add_row(t, (ScCell[]){
            sc_cell_m("scheduler"),
            sc_cell_m("[bold red]● FAIL[/]"),
            sc_cell("82.10%"),
        }, 3);

        ScTableOpts topts = {
            .border       = { SC_BORDER_ROUNDED, SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE,
                              SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
            .header.row   = true,
            .header.style = s_b_cyan,
            .striped      = true,
            .stripe_bg    = stripe,
            .title        = { .text = " Overview ", .style = s_b_magenta,
                              .halign = SC_ALIGN_CENTER, .pad = 1,
                              .pos = SC_POSITION_TOP },
            .cell_pad     = { 0, 1, 0, 1 },
        };

        ScList *l = sc_list_new((ScListOpts){
            .marker        = SC_LIST_NUMBER,
            .marker_suffix = ".",
            .marker_style  = s_b_yellow,
            .indent        = 0,
            .width         = 36,
        });
        sc_list_add_str(l, "Compose widgets side by side",  s_plain);
        sc_list_add_str(l, "Capture, pad, and align them",  s_plain);
        sc_list_add_str(l, "Style with Rich-style markup",  s_plain);
        sc_list_add_str(l, "Render to any UTF-8 terminal",  s_plain);

        /* A narrow source tree, shown beside the rules under the list. */
        ScTree *tree = sc_tree_new((ScTreeOpts){
            .type            = SC_BORDER_SINGLE,
            .connector_color = SC_ANSI_COLOR_CYAN,
            .indent          = 1,
        });
        ScTreeNode *root = sc_tree_add_str(tree, NULL, "project/", s_b_cyan, NULL, s_plain);
        ScTreeNode *api  = sc_tree_add_str(tree, root, "api/",     s_b_cyan, NULL, s_plain);
        sc_tree_add_str(tree, api,  "routes.c", s_plain, NULL, s_plain);
        sc_tree_add_str(tree, api,  "auth.c",   s_plain, NULL, s_plain);
        sc_tree_add_str(tree, root, "worker.c", s_plain, NULL, s_plain);
        sc_tree_add_str(tree, root, "store.c",  s_plain, NULL, s_plain);

        /* Three narrow rules stacked vertically. */
        ScRendered *r_rules[] = {
            capture_titled_rule("Pipeline", SC_BORDER_SINGLE, SC_ANSI_COLOR_MAGENTA,   s_b_magenta,   18),
            capture_titled_rule("Workers",  SC_BORDER_DOUBLE, SC_ANSI_COLOR_NONE,   s_b_green,  18),
            capture_titled_rule("Storage",  SC_BORDER_THICK,  SC_ANSI_COLOR_NONE,   s_b_yellow, 18),
        };
        ScRendered *rule_stack = sc_vstack(
            (const ScRendered *const *)r_rules, 3, 1
        );

        /* tree | rules, side by side – this fills the area under the list. */
        ScColumns *under = sc_columns_new((ScColumnsOpts){
            .gap    = 2,
            .sep    = { .type = SC_BORDER_NONE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_tree    (under, tree,       (ScColItem){ 0 });
        sc_columns_add_rendered(under, rule_stack, (ScColItem){ 0 });
        ScRendered *r_under = sc_capture_columns(under);

        /* Right column = list, then the tree|rules block stacked beneath it. */
        ScRendered *r_list = sc_capture_list(l);
        ScRendered *right_parts[] = { r_list, r_under };
        ScRendered *right_col = sc_vstack(
            (const ScRendered *const *)right_parts, 2, 1
        );

        ScRendered *r_table = sc_capture_table(t, topts);

        ScColumns *cols = sc_columns_new((ScColumnsOpts){
            .gap    = 2,
            .sep    = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
            .valign = SC_VALIGN_TOP,
        });
        sc_columns_add_rendered(cols, r_table,   (ScColItem){ 0 });
        sc_columns_add_rendered(cols, right_col, (ScColItem){ 0 });
        sc_columns_print(cols);

        sc_columns_free(cols);
        sc_columns_free(under);
        sc_rendered_free(right_col);
        sc_rendered_free(r_under);
        sc_rendered_free(r_table);
        sc_rendered_free(r_list);
        sc_rendered_free(rule_stack);
        sc_rendered_free(r_rules[0]);
        sc_rendered_free(r_rules[1]);
        sc_rendered_free(r_rules[2]);
        sc_tree_free(tree);
        sc_list_free(l);
        sc_table_free(t);
    }

    printf("\n");

    /* Bottom: a finished progress bar to close the collage. */
    {
        ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
            .type         = SC_PROGRESS_BLOCK,
            .left_cap     = "[",
            .right_cap    = "]",
            .show_percent = true,
            .show_value   = true,
            .width        = 60,
            .label_width  = 12,
            .label_style  = s_bold,
            .thresholds   = {
                .enabled    = true,
                .mid        = 0.5,
                .high       = 0.85,
                .color_low  = SC_ANSI_COLOR_RED,
                .color_mid  = SC_ANSI_COLOR_YELLOW,
                .color_high = SC_ANSI_COLOR_GREEN,
            },
        });
        sc_progressbar_set_label(b, "Building");
        sc_progressbar_finish(b, 92.0, 100.0);
        sc_progressbar_free(b);
    }
}


/* ───────────────────────────────────────── 2. quick-start snippet ── */

static void shot_quickstart(void) {
    sc_markup_println("[bold green]Hello[/], [italic]sparcli[/]!");

    sc_panel_str("Welcome aboard.", (ScPanelOpts){
        .border  = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE },
        .title   = {
            .text  = " Greeting ",
            .halign = SC_ALIGN_CENTER,
            .pad   = 1,
            .pos   = SC_POSITION_TOP,
        },
        .padding = { 0, 2, 0, 2 },
    });
}


/* ───────────────────────────────────────── 3. per-widget gallery ── */

static void gallery_panel(void) {
    sc_panel_str(
        "A bordered frame with a title, padding,\n"
        "and an optional background color.",
        (ScPanelOpts){
            .border  = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_CYAN },
            .title   = {
                .text  = " Panel ",
                .style = s_b_cyan,
                .halign = SC_ALIGN_LEFT,
                .pad   = 1,
                .pos   = SC_POSITION_TOP,
            },
            .padding       = { 0, 2, 0, 2 },
            .content_align = SC_ALIGN_LEFT,
        }
    );
}

static void gallery_table(void) {
    ScTableData *t = sc_table_new();
    sc_table_add_column(t, "Lang", (ScColOpts){
        0, 0, 10, SC_ALIGN_LEFT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
    });
    sc_table_add_column(t, "Year", (ScColOpts){
        0, 0, 6, SC_ALIGN_RIGHT, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
    });
    sc_table_add_column(t, "Typed", (ScColOpts){
        0, 0, 10, SC_ALIGN_CENTER, SC_VALIGN_TOP, 0, SC_ANSI_COLOR_NONE
    });
    sc_table_add_row(t, (ScCell[]){ sc_cell("C"),      sc_cell("1972"), sc_cell("Static")  }, 3);
    sc_table_add_row(t, (ScCell[]){ sc_cell("Python"), sc_cell("1991"), sc_cell("Dynamic") }, 3);
    sc_table_add_row(t, (ScCell[]){ sc_cell("Rust"),   sc_cell("2010"), sc_cell("Static")  }, 3);
    sc_table_add_row(t, (ScCell[]){ sc_cell("Zig"),    sc_cell("2016"), sc_cell("Static")  }, 3);

    sc_table_print(t, (ScTableOpts){
        .border       = { SC_BORDER_SINGLE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
                          SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE, 0, 0, 0 },
        .header.row   = true,
        .header.style = s_bold,
        .striped      = true,
        .stripe_bg    = sc_color_from_rgb(40, 40, 60),
        .title        = { .text = " Table ", .style = s_b_cyan,
                          .halign = SC_ALIGN_CENTER, .pad = 1,
                          .pos = SC_POSITION_TOP },
        .cell_pad     = { 0, 1, 0, 1 },
    });
    sc_table_free(t);
}

static void gallery_rule(void) {
    sc_rule_str("Rule with title", (ScRuleOpts){
        .type        = SC_BORDER_SINGLE,
        .color       = SC_ANSI_COLOR_CYAN,
        .title.style = s_b_cyan,
        .title.halign = SC_ALIGN_CENTER,
        .title.pad   = 1,
    });
}

static void gallery_columns(void) {
    ScText *left = sc_text_new();
    sc_text_append(left, "Left column\n",  s_b_cyan);
    sc_text_append(left, "auto width.",    s_plain);

    ScText *mid = sc_text_new();
    sc_text_append(mid, "Middle\n",        s_b_green);
    sc_text_append(mid, "vertical sep.",   s_plain);

    ScText *right = sc_text_new();
    sc_text_append(right, "Right column\n", s_b_yellow);
    sc_text_append(right, "valign top.",    s_plain);

    ScColumns *cl = sc_columns_new((ScColumnsOpts){
        .gap    = 2,
        .sep    = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
        .valign = SC_VALIGN_TOP,
    });
    sc_columns_add_text(cl, left,  (ScColItem){ 0 });
    sc_columns_add_text(cl, mid,   (ScColItem){ 0 });
    sc_columns_add_text(cl, right, (ScColItem){ 0 });
    sc_columns_print(cl);

    sc_columns_free(cl);
    sc_text_free(left);
    sc_text_free(mid);
    sc_text_free(right);
}

static void gallery_list(void) {
    ScList *l = sc_list_new((ScListOpts){
        .marker        = SC_LIST_NUMBER,
        .marker_suffix = ".",
        .marker_style  = s_b_cyan,
        .indent        = 0,
    });
    sc_list_add_str(l, "Top-level item",                     s_plain);
    ScListItem *parent = sc_list_add_str(l, "With nested children:", s_plain);
    ScList *sub = sc_list_add_sub(parent, (ScListOpts){
        .marker = SC_LIST_BULLET,
        .indent = 2,
    });
    sc_list_add_str(sub, "Bulleted child",  s_dim);
    sc_list_add_str(sub, "Hanging indent",  s_dim);
    sc_list_add_str(l,   "Back to numbers", s_plain);
    sc_list_print(l);
    sc_list_free(l);
}

static void gallery_tree(void) {
    ScTree *tree = sc_tree_new((ScTreeOpts){
        .type            = SC_BORDER_SINGLE,
        .connector_color = SC_ANSI_COLOR_CYAN,
        .indent          = 1,
    });
    ScTreeNode *root = sc_tree_add_str(tree, NULL, "project/", s_b_cyan, NULL, s_plain);
    ScTreeNode *src  = sc_tree_add_str(tree, root, "src/",     s_b_cyan, NULL, s_plain);
    sc_tree_add_str(tree, src,  "main.c",        s_plain, NULL, s_plain);
    sc_tree_add_str(tree, src,  "util.c",        s_plain, NULL, s_plain);
    sc_tree_add_str(tree, root, "Makefile",      s_plain, NULL, s_plain);
    sc_tree_add_str(tree, root, "README.md",     s_plain, NULL, s_plain);
    sc_tree_print(tree);
    sc_tree_free(tree);
}

static void gallery_kv(void) {
    ScKV *kv = sc_kv_new((ScKVOpts){
        .sep       = "  ",
        .key_width = 0,
        .key_style = s_b_cyan,
        .val_style = s_plain,
    });
    sc_kv_add(kv, "host",    "localhost");
    sc_kv_add(kv, "port",    "8080");
    sc_kv_add(kv, "timeout", "30s");
    sc_kv_add(kv, "scheme",  "https");
    sc_kv_print(kv);
    sc_kv_free(kv);
}

static void gallery_alerts(void) {
    sc_alert_info   ("Build started.");
    sc_alert_success("All 142 tests passed.");
    sc_alert_warning("Disk usage at 87%.");
    sc_alert_error  ("Connection refused on :5432.");
}

static void gallery_badges(void) {
    ScBadgeOpts ok      = { .left_cap = "[", .right_cap = "]",
                            .text_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                                            SC_ANSI_COLOR_GREEN },
                            .pad = 1 };
    ScBadgeOpts warn    = { .left_cap = "[", .right_cap = "]",
                            .text_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                                            SC_ANSI_COLOR_YELLOW },
                            .pad = 1 };
    ScBadgeOpts err     = { .left_cap = "[", .right_cap = "]",
                            .text_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                            SC_ANSI_COLOR_RED },
                            .pad = 1 };
    ScBadgeOpts info    = { .left_cap = "[", .right_cap = "]",
                            .text_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                            SC_ANSI_COLOR_BLUE },
                            .pad = 1 };

    sc_print_badge("DONE",  ok);    printf("  ");
    sc_print_badge("WARN",  warn);  printf("  ");
    sc_print_badge("ERROR", err);   printf("  ");
    sc_print_badge("INFO",  info);  printf("\n");
}

static void gallery_progressbar(void) {
    ScProgressBar *b = sc_progressbar_new((ScProgressBarOpts){
        .type         = SC_PROGRESS_BLOCK,
        .left_cap     = "[",
        .right_cap    = "]",
        .show_percent = true,
        .show_value   = true,
        .width        = 60,
        .label_width  = 12,
        .label_style  = s_bold,
        .thresholds   = {
            .enabled    = true,
            .color_low  = SC_ANSI_COLOR_RED,
            .color_mid  = SC_ANSI_COLOR_YELLOW,
            .color_high = SC_ANSI_COLOR_GREEN,
        },
    });
    sc_progressbar_set_label(b, "Downloading");
    sc_progressbar_finish(b, 78.0, 100.0);
    sc_progressbar_free(b);
}

static void gallery_spinner(void) {
    /*
     * Spinners are inherently animated. For a static screenshot we render
     * only the final success line, which uses the same style as a real
     * spinner finish.
     */
    ScSpinner *s = sc_spinner_new("Initialising…", (ScSpinnerOpts){
        .type        = SC_SPINNER_BRAILLE,
        .color       = SC_ANSI_COLOR_CYAN,
        .label_style = s_bold,
    });
    sc_spinner_finish(s, true, "Initialised");
    sc_spinner_free(s);
}

static void shot_gallery(void) {
    gallery_panel();        printf("\n");
    gallery_table();        printf("\n");
    gallery_rule();         printf("\n");
    gallery_columns();      printf("\n");
    gallery_list();         printf("\n");
    gallery_tree();         printf("\n");
    gallery_kv();           printf("\n");
    gallery_alerts();       printf("\n");
    gallery_badges();       printf("\n");
    gallery_progressbar();
    gallery_spinner();
}


/* ───────────────────────────────────────── 4. markup before / after ── */

static void shot_markup(void) {
    static const char *snippets[] = {
        "[bold red]Error:[/] file not found",
        "[on cyan] 200 OK [/]   [dim]42ms[/]",
        "[bold]sparcli[/] supports [italic]markup[/]",
        "[rgb(120,200,255)]custom[/] color",
    };
    const size_t n = sizeof(snippets) / sizeof(snippets[0]);

    /* Build a single ScText holding the literal markup strings, line by line. */
    ScText *raw = sc_text_new();
    for (size_t i = 0; i < n; i++) {
        sc_text_append(raw, snippets[i], s_dim);
        if (i + 1 < n) { sc_text_append(raw, "\n", s_dim); }
    }

    /* And another that holds the rendered (parsed) versions. */
    ScText *rendered = sc_text_new();
    for (size_t i = 0; i < n; i++) {
        sc_markup_append(rendered, snippets[i]);
        if (i + 1 < n) { sc_text_append(rendered, "\n", s_plain); }
    }

    ScPanelOpts left_opts = {
        .border = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE },
        .title  = { .text = " Markup source ", .style = s_b_cyan,
                    .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding       = { 0, 2, 0, 2 },
        .content_align = SC_ALIGN_LEFT,
    };
    ScPanelOpts right_opts = {
        .border = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_NONE },
        .title  = { .text = " Rendered ", .style = s_b_green,
                    .halign = SC_ALIGN_LEFT, .pad = 1, .pos = SC_POSITION_TOP },
        .padding       = { 0, 2, 0, 2 },
        .content_align = SC_ALIGN_LEFT,
    };

    ScColumns *cl = sc_columns_new((ScColumnsOpts){
        .gap    = 2,
        .sep    = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
        .valign = SC_VALIGN_TOP,
    });
    sc_columns_add_panel_text(cl, raw,      left_opts,  (ScColItem){ 0 });
    sc_columns_add_panel_text(cl, rendered, right_opts, (ScColItem){ 0 });
    sc_columns_print(cl);

    sc_columns_free(cl);
    sc_text_free(raw);
    sc_text_free(rendered);
}


/* ─────────────────────────────────────────────────────────── main ── */

int main(int argc, char **argv) {
    int only = 0;
    if (argc > 1) { only = (int)strtol(argv[1], NULL, 10); }

    if (only == 0 || only == 1) {
        section_rule(" 1. Hero ");
        shot_hero();
    }
    if (only == 0 || only == 2) {
        section_rule(" 2. Quick start ");
        shot_quickstart();
    }
    if (only == 0 || only == 3) {
        section_rule(" 3. Widget gallery ");
        shot_gallery();
    }
    if (only == 0 || only == 4) {
        section_rule(" 4. Rich-compatible markup ");
        shot_markup();
    }

    (void)s_italic;
    (void)s_b_magenta;
    return 0;
}
