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
}
