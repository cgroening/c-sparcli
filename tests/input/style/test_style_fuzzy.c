#include "test_style.h"
#include "input/input_internal.h"


void style_fuzzy(void) {
    const char *cities[] = { "Tokyo", "Toronto", "London", "Boston", "Lisbon" };
    size_t n = sizeof cities / sizeof cities[0];

    /* List view, default style, query "to". */
    ScFuzzy *a = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "City" });
    for (size_t i = 0; i < n; i++) { sc_fuzzy_add(a, cities[i]); }
    style_show("fuzzy list: default, query='to'", sc_fuzzy_frame(a, "to"));
    sc_fuzzy_free(a);

    /* List view, custom prompt/cursor/marker styling. */
    ScFuzzy *b = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City",
        .accent = SC_ANSI_COLOR_MAGENTA,
        .prompt_style = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
                          SC_ANSI_COLOR_MAGENTA, SC_ANSI_COLOR_NONE },
        .cursor_marker = "\xc2\xbb ",  /* » */
        .cursor_style = {
            SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_MAGENTA,
        },
    });
    for (size_t i = 0; i < n; i++) { sc_fuzzy_add(b, cities[i]); }
    style_show("fuzzy list: magenta accent, » marker, query='o'",
               sc_fuzzy_frame(b, "o"));
    sc_fuzzy_free(b);

    /* Table view, default border. */
    const char *headers[] = { "City", "Country", "Pop." };
    ScFuzzy *c = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3 });
    sc_fuzzy_add_row(c, (const char *[]){ "Tokyo",  "Japan",    "37.4" }, 3);
    sc_fuzzy_add_row(c, (const char *[]){ "London", "UK",       "9.0"  }, 3);
    sc_fuzzy_add_row(c, (const char *[]){ "Lisbon", "Portugal", "0.5"  }, 3);
    style_show("fuzzy table: default single border", sc_fuzzy_frame(c, ""));
    sc_fuzzy_free(c);

    /* Table view, double border passed through table_opts. */
    ScFuzzy *d = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3,
        .accent = SC_ANSI_COLOR_BLUE,
        .table_opts = { .border = { .type = SC_BORDER_DOUBLE } } });
    sc_fuzzy_add_row(d, (const char *[]){ "Tokyo",  "Japan",    "37.4" }, 3);
    sc_fuzzy_add_row(d, (const char *[]){ "London", "UK",       "9.0"  }, 3);
    style_show("fuzzy table: double border via table_opts",
               sc_fuzzy_frame(d, ""));
    sc_fuzzy_free(d);

    /* Hint layout on the fuzzy path: stacked footer. */
    ScFuzzy *e = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .hint_layout = SC_HINT_STACKED });
    for (size_t i = 0; i < n; i++) { sc_fuzzy_add(e, cities[i]); }
    style_show("fuzzy list: hint_layout stacked (one hint per line)",
               sc_fuzzy_frame(e, "to"));
    sc_fuzzy_free(e);

    /* search_columns default (all): query 'stat' matches the 2nd ("Typed")
     * column, filtering the table to the statically-typed languages. */
    const char *lang_headers[] = { "Programming language", "Typed" };
    ScFuzzy *f = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Language", .table = true,
        .headers = lang_headers, .n_cols = 2,
        .selected_style = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,
                            SC_ANSI_COLOR_NONE } });
    sc_fuzzy_add_row(f, (const char *[]){ "C",      "Static"  }, 2);
    sc_fuzzy_add_row(f, (const char *[]){ "Rust",   "Static"  }, 2);
    sc_fuzzy_add_row(f, (const char *[]){ "Python", "Dynamic" }, 2);
    sc_fuzzy_add_row(f, (const char *[]){ "Ruby",   "Dynamic" }, 2);
    style_show("fuzzy table: 'stat' highlights Static; cursor row black text",
               sc_fuzzy_frame(f, "stat"));
    sc_fuzzy_free(f);

    /* Table view, cursor-row background overridden via selected_style.bg. */
    ScFuzzy *g = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Language", .table = true,
        .headers = lang_headers, .n_cols = 2,
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            SC_ANSI_COLOR_MAGENTA } });
    sc_fuzzy_add_row(g, (const char *[]){ "C",      "Static"  }, 2);
    sc_fuzzy_add_row(g, (const char *[]){ "Rust",   "Static"  }, 2);
    style_show("fuzzy table: cursor row white-on-magenta (selected_style.bg)",
               sc_fuzzy_frame(g, ""));
    sc_fuzzy_free(g);

    /* Boxed list view: rounded frame, fixed width. */
    ScFuzzy *h = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .box = { .enabled = true, .width = 30 } });
    for (size_t i = 0; i < n; i++) { sc_fuzzy_add(h, cities[i]); }
    style_show("fuzzy list: boxed rounded frame, width 30",
               sc_fuzzy_frame(h, "to"));
    sc_fuzzy_free(h);

    /* Boxed table view: thick border + content background + padding. */
    ScFuzzy *i = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3,
        .box = {
            .enabled = true,
            .border = { .type = SC_BORDER_THICK, .color = SC_ANSI_COLOR_GREEN },
            .bg = SC_ANSI_COLOR_BLACK,
            .padding = { .left = 2, .right = 2 },
        } });
    sc_fuzzy_add_row(i, (const char *[]){ "Tokyo",  "Japan", "37.4" }, 3);
    sc_fuzzy_add_row(i, (const char *[]){ "London", "UK",    "9.0"  }, 3);
    style_show("fuzzy table: boxed thick green border, black bg, padding h2",
               sc_fuzzy_frame(i, ""));
    sc_fuzzy_free(i);

    /* Boxed table, fixed width 60: column 0 stretches to fill the frame. */
    ScFuzzy *is = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3,
        .stretch_columns = (uint64_t)1 << 0,
        .box = { .enabled = true, .width = 60,
                 .border = { .type = SC_BORDER_ROUNDED } } });
    sc_fuzzy_add_row(is, (const char *[]){ "Tokyo",  "Japan", "37.4" }, 3);
    sc_fuzzy_add_row(is, (const char *[]){ "London", "UK",    "9.0"  }, 3);
    style_show("fuzzy table: width 60, col 0 stretches to fill",
               sc_fuzzy_frame(is, ""));
    sc_fuzzy_free(is);

    /* Full-width box: the stretched table fills the frame's interior exactly
     * (regression: the FULL interior must match the panel's content area). */
    ScFuzzy *fw = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3,
        .stretch_columns = (uint64_t)1 << 0,
        .box = { .enabled = true, .padding = { .left = 1, .right = 1 },
                 .border = { .type = SC_BORDER_ROUNDED },
                 .width_mode = SC_WIDTH_FULL } });
    sc_fuzzy_add_row(fw, (const char *[]){ "Tokyo",  "Japan", "37.4" }, 3);
    sc_fuzzy_add_row(fw, (const char *[]){ "London", "UK",    "9.0"  }, 3);
    style_show("fuzzy table: full-width box, col 0 fills to the frame",
               sc_fuzzy_frame(fw, ""));
    sc_fuzzy_free(fw);

    /* List view, widget background without a frame: rows inherit it and the
     * cursor row is a full-width bar (selected_style.bg). */
    ScFuzzy *j = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City",
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            SC_ANSI_COLOR_MAGENTA },
        .box = { .bg = SC_ANSI_COLOR_BLACK } });
    for (size_t k = 0; k < n; k++) { sc_fuzzy_add(j, cities[k]); }
    style_show("fuzzy list: black widget bg, magenta full-width cursor bar",
               sc_fuzzy_frame(j, "o"));
    sc_fuzzy_free(j);

    /* List view, fixed width 28 with a content background. */
    ScFuzzy *k = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City",
        .box = { .width_mode = SC_WIDTH_FIXED, .width = 28,
                 .bg = SC_ANSI_COLOR_BLUE } });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(k, cities[m]); }
    style_show("fuzzy list: borderless blue bg, fixed width 28",
               sc_fuzzy_frame(k, "to"));
    sc_fuzzy_free(k);

    /* Sections (list): non-selectable headers + per-group counts. */
    ScFuzzy *s1 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Tasks", .section_counts = true });
    sc_fuzzy_add_section(s1, "Monday");
    sc_fuzzy_add(s1, "Buy milk");
    sc_fuzzy_add(s1, "Call Bob");
    sc_fuzzy_add_section(s1, "Tuesday");
    sc_fuzzy_add(s1, "Ship release");
    style_show("fuzzy list: sections + counts, query=''",
               sc_fuzzy_frame(s1, ""));
    sc_fuzzy_free(s1);

    /* Sections with a custom section_style: bold white on a filled bar; the
     * background spans the whole row width. */
    ScFuzzy *s1b = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Tasks", .section_counts = true,
        .section_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                           SC_ANSI_COLOR_BLUE } });
    sc_fuzzy_add_section(s1b, "Monday");
    sc_fuzzy_add(s1b, "Buy milk");
    sc_fuzzy_add_section(s1b, "Tuesday");
    sc_fuzzy_add(s1b, "Ship release");
    style_show("fuzzy list: colored section bar (bold white on blue)",
               sc_fuzzy_frame(s1b, ""));
    sc_fuzzy_free(s1b);

    /* Disabled (greyed, non-selectable) row. */
    ScFuzzy *s2 = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Tasks" });
    sc_fuzzy_add(s2, "Active task");
    sc_fuzzy_add(s2, "Done task");
    sc_fuzzy_set_disabled(s2, 1, true);
    style_show("fuzzy list: second row disabled (greyed)",
               sc_fuzzy_frame(s2, ""));
    sc_fuzzy_free(s2);

    /* Multi-select (list): two rows pre-checked. */
    ScFuzzy *s3 = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Pick", .multi = true });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(s3, cities[m]); }
    sc_fuzzy_set_checked(s3, 0, true);
    sc_fuzzy_set_checked(s3, 2, true);
    style_show("fuzzy list: multi-select, Tokyo + London checked",
               sc_fuzzy_frame(s3, ""));
    sc_fuzzy_free(s3);

    /* Multi-select table: dedicated checkbox column, sections + counts,
     * ordered by the time column. */
    const char *todo_headers[] = { "Time", "Task" };
    ScFuzzy *s4 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Agenda", .table = true,
        .headers = todo_headers, .n_cols = 2,
        .multi = true, .checkbox_column = true, .section_counts = true,
        .order = SC_FUZZY_ORDER_COLUMN, .order_column = 0 });
    sc_fuzzy_add_section(s4, "Monday");
    sc_fuzzy_add_row(s4, (const char *[]){ "14:00", "Review PR" }, 2);
    sc_fuzzy_add_row(s4, (const char *[]){ "09:00", "Standup"  }, 2);
    sc_fuzzy_set_checked(s4, 2, true);   /* the 09:00 standup */
    style_show("fuzzy table: multi checkbox column, sections, order=time",
               sc_fuzzy_frame(s4, ""));
    sc_fuzzy_free(s4);

    /* Per-cell base style (whole-cell color), highlight overlays on top. */
    const char *st_headers[] = { "Status", "Task" };
    ScTextStyle red   = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,
                          SC_ANSI_COLOR_NONE };
    ScTextStyle green = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_GREEN,
                          SC_ANSI_COLOR_NONE };
    ScFuzzy *s5 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Status", .table = true,
        .headers = st_headers, .n_cols = 2 });
    sc_fuzzy_add_row_styled(s5, (const char *[]){ "overdue", "Pay invoice" },
                            (ScTextStyle[]){ red, { 0 } }, 2);
    sc_fuzzy_add_row_styled(s5, (const char *[]){ "done", "Write report" },
                            (ScTextStyle[]){ green, { 0 } }, 2);
    style_show("fuzzy table: per-cell status colors (red/green)",
               sc_fuzzy_frame(s5, ""));
    sc_fuzzy_free(s5);

    /* Rich ScText cell: a colored priority badge in column 0. */
    const char *pr_headers[] = { "Prio", "Task" };
    ScText *badge = sc_text_new();
    sc_text_append(badge, "HIGH", (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                   SC_ANSI_COLOR_WHITE, SC_ANSI_COLOR_RED });
    ScText *plain = sc_text_from_str("Fix login bug");
    ScFuzzy *s6 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Prio", .table = true, .headers = pr_headers, .n_cols = 2 });
    sc_fuzzy_add_row_rich(s6, (ScText *[]){ badge, plain }, 2);
    sc_text_free(badge);
    sc_text_free(plain);
    style_show("fuzzy table: rich ScText cell (white-on-red badge)",
               sc_fuzzy_frame(s6, ""));
    sc_fuzzy_free(s6);

    /* Empty-state line when the query matches nothing. */
    ScFuzzy *s7 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .empty_text = "No matching cities" });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(s7, cities[m]); }
    style_show("fuzzy list: empty-state text, query='zzz'",
               sc_fuzzy_frame(s7, "zzz"));
    sc_fuzzy_free(s7);

    /* Modal mode: NORMAL badge (default colors) + field tint. */
    ScFuzzy *m1 = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "City", .modal = true });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(m1, cities[m]); }
    style_show("fuzzy list: modal NORMAL badge, query='to'",
               sc_fuzzy_frame(m1, "to"));
    sc_fuzzy_free(m1);

    /* Modal mode: INSERT badge (start_in_insert) + field tint. */
    ScFuzzy *m2 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .modal = true, .start_in_insert = true });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(m2, cities[m]); }
    style_show("fuzzy list: modal INSERT badge, query='to'",
               sc_fuzzy_frame(m2, "to"));
    sc_fuzzy_free(m2);

    /* Modal mode: custom label + style (black on yellow). */
    ScFuzzy *m3 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .modal = true,
        .normal_label = "CMD",
        .mode_normal_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                               SC_ANSI_COLOR_YELLOW } });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(m3, cities[m]); }
    style_show("fuzzy list: modal custom CMD badge (black on yellow)",
               sc_fuzzy_frame(m3, "o"));
    sc_fuzzy_free(m3);

    /* Modal mode with the badge hidden: the field is still tinted per mode. */
    ScFuzzy *m4 = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "City", .modal = true, .hide_mode_badge = true });
    for (size_t m = 0; m < n; m++) { sc_fuzzy_add(m4, cities[m]); }
    style_show("fuzzy list: modal badge hidden (field tint only)",
               sc_fuzzy_frame(m4, "to"));
    sc_fuzzy_free(m4);
}
