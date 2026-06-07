#include "test_input.h"
#include "sparcli.h"
#include "input/input_internal.h"   /* sc_fuzzy_frame for the height-fit checks */

#include <string.h>


void test_fuzzy(void) {
    /* Sanity-check the pure matcher first (runs even without a TTY). */
    int score = 0;
    CHECK(sc_fuzzy_match("to", "Tokyo", &score) && score > 0,
          "fuzzy_match: 'to' matches 'Tokyo'");
    CHECK(!sc_fuzzy_match("zzz", "Tokyo", NULL),
          "fuzzy_match: 'zzz' rejects 'Tokyo'");
    CHECK(sc_fuzzy_match("", "anything", NULL),
          "fuzzy_match: empty pattern always matches");

    /* has_selection is false without a live run (no filtered matches yet). */
    CHECK(!sc_fuzzy_has_selection(NULL),
          "has_selection: NULL has no selection");

    /* List view. */
    ScFuzzy *f = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Pick a city" });
    CHECK(!sc_fuzzy_has_selection(f),
          "has_selection: false before a run allocates matches");
    const char *cities[] = { "Tokyo", "Toronto", "London", "Berlin",
                             "Boston", "Lisbon", "Amsterdam" };
    for (size_t i = 0; i < sizeof cities / sizeof cities[0]; i++) {
        sc_fuzzy_add(f, cities[i]);
    }
    size_t pick = 0;
    ScInputStatus st = sc_fuzzy_run(f, &pick);
    if (st == SC_INPUT_OK) { printf("  → list: %s\n", cities[pick]); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → list: cancelled\n"); }
    else { printf("  → no TTY (skipped)\n"); }
    sc_fuzzy_free(f);

    /* Table view. */
    const char *headers[] = { "City", "Country", "Pop. (M)" };
    ScFuzzy *ft = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search cities", .table = true,
        .headers = headers, .n_cols = 3 });
    sc_fuzzy_add_row(ft, (const char *[]){ "Tokyo",  "Japan", "37.4" }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "London", "UK",    "9.0"  }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "Berlin", "Germany", "3.7" }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "Lisbon", "Portugal", "0.5" }, 3);
    st = sc_fuzzy_run(ft, &pick);
    if (st == SC_INPUT_OK)        { printf("  → table: row %zu\n", pick); }
    else if (st == SC_INPUT_CANCELLED) { printf("  → table: cancelled\n"); }
    sc_fuzzy_free(ft);
}

/* Non-interactive coverage (runs in CI via `--logic`). */
void test_fuzzy_logic(void) {
    /* Pure matcher. */
    int score = 0;
    CHECK(sc_fuzzy_match("to", "Tokyo", &score) && score > 0,
          "fuzzy_match: 'to' matches 'Tokyo'");
    CHECK(!sc_fuzzy_match("zzz", "Tokyo", NULL),
          "fuzzy_match: 'zzz' rejects 'Tokyo'");
    CHECK(sc_fuzzy_match("", "anything", NULL),
          "fuzzy_match: empty pattern always matches");

    /* Stable ids + label / set_label / set_row. */
    ScFuzzy *g = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "T" });
    sc_fuzzy_add(g, "alpha");
    sc_fuzzy_add(g, "beta");
    sc_fuzzy_set_id(g, 0, 100);
    sc_fuzzy_set_id(g, 1, 200);
    CHECK(sc_fuzzy_id_at(g, 0) == 100 && sc_fuzzy_id_at(g, 1) == 200,
          "fuzzy: set_id/id_at round-trip");
    CHECK(sc_fuzzy_id_at(g, 99) == 0, "fuzzy: id_at out of range = 0");
    CHECK(sc_fuzzy_label(g, 0) && strcmp(sc_fuzzy_label(g, 0), "alpha") == 0,
          "fuzzy: label reads field 0");
    sc_fuzzy_set_label(g, 0, "ALPHA");
    CHECK(strcmp(sc_fuzzy_label(g, 0), "ALPHA") == 0,
          "fuzzy: set_label replaces field 0");
    sc_fuzzy_set_row(g, 1, (const char *[]){ "b2" }, 1);
    CHECK(strcmp(sc_fuzzy_label(g, 1), "b2") == 0,
          "fuzzy: set_row replaces the row fields");
    sc_fuzzy_free(g);

    /* Multi-select checked set (no run needed). */
    ScFuzzy *m = sc_fuzzy_new((ScFuzzyOpts){ .multi = true });
    for (int i = 0; i < 5; i++) {
        char buf[8];
        snprintf(buf, sizeof buf, "i%d", i);
        sc_fuzzy_add(m, buf);
    }
    CHECK(sc_fuzzy_checked_count(m) == 0, "fuzzy multi: none checked initially");
    sc_fuzzy_set_checked(m, 1, true);
    sc_fuzzy_set_checked(m, 3, true);
    CHECK(sc_fuzzy_is_checked(m, 1) && sc_fuzzy_is_checked(m, 3)
          && !sc_fuzzy_is_checked(m, 0),
          "fuzzy multi: set_checked / is_checked");
    CHECK(sc_fuzzy_checked_count(m) == 2, "fuzzy multi: checked_count");
    sc_fuzzy_check_all(m, true);
    CHECK(sc_fuzzy_checked_count(m) == 5, "fuzzy multi: check_all");
    sc_fuzzy_check_all(m, false);
    CHECK(sc_fuzzy_checked_count(m) == 0, "fuzzy multi: uncheck all");
    sc_fuzzy_free(m);

    /* Disabled rows are never checkable. */
    ScFuzzy *d = sc_fuzzy_new((ScFuzzyOpts){ .multi = true });
    sc_fuzzy_add(d, "active");
    sc_fuzzy_add(d, "done");
    sc_fuzzy_set_disabled(d, 1, true);
    sc_fuzzy_set_checked(d, 1, true);
    CHECK(!sc_fuzzy_is_checked(d, 1), "fuzzy: disabled row can't be checked");
    sc_fuzzy_check_all(d, true);
    CHECK(sc_fuzzy_is_checked(d, 0) && !sc_fuzzy_is_checked(d, 1),
          "fuzzy: check_all skips disabled rows");
    sc_fuzzy_free(d);

    /* add_row_rich flattens its span text into the match / label key. */
    ScText *badge = sc_text_new();
    sc_text_append(badge, "HI", (ScTextStyle){ 0 });
    sc_text_append(badge, "GH", (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                   SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    ScFuzzy *rich = sc_fuzzy_new((ScFuzzyOpts){ .table = true, .n_cols = 1 });
    sc_fuzzy_add_row_rich(rich, (ScText *[]){ badge }, 1);
    sc_text_free(badge);
    CHECK(sc_fuzzy_label(rich, 0)
          && strcmp(sc_fuzzy_label(rich, 0), "HIGH") == 0,
          "fuzzy: add_row_rich flattens span text for matching");
    sc_fuzzy_free(rich);

    /* Height budget: a fullscreen TABLE finder with many rows + a header + a
       labeled shortcut footer must fit the terminal (no clip). The frame
       builder does not stack the engine footer, so leave one row for it. */
    int term_h = sc_term_height();   /* 24 fallback under the non-TTY test env */
    ScShortcut sk[] = {
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd' },
          .id = 1, .hint_label = "done" },
    };
    const char *th[] = { "Task", "Due" };
    ScRendered *hdr = sc_capture_str("mdtask\nheader");   /* 2-line header */
    ScFuzzy *tall = sc_fuzzy_new((ScFuzzyOpts){
        .table = true, .headers = th, .n_cols = 2,
        .fullscreen = true, .header = hdr,
        .shortcuts = sk, .n_shortcuts = 1,
        .box = { .enabled = true } });
    for (int i = 0; i < 40; i++) {
        char a[16], b[16];
        snprintf(a, sizeof a, "task %d", i);
        snprintf(b, sizeof b, "2026-%02d", i % 12 + 1);
        sc_fuzzy_add_row(tall, (const char *[]){ a, b }, 2);
    }
    ScRendered *tf = sc_fuzzy_frame(tall, "");
    CHECK(tf && (int)tf->line_count <= term_h - 1,
          "fuzzy: fullscreen table frame fits terminal (leaves footer row)");
    sc_rendered_free(tf);
    sc_fuzzy_free(tall);

    /* List view, fullscreen: one row per entry, also bounded. */
    ScFuzzy *tl = sc_fuzzy_new((ScFuzzyOpts){
        .fullscreen = true, .header = hdr, .box = { .enabled = true } });
    for (int i = 0; i < 60; i++) {
        char a[16];
        snprintf(a, sizeof a, "item %d", i);
        sc_fuzzy_add(tl, a);
    }
    ScRendered *lf = sc_fuzzy_frame(tl, "");
    CHECK(lf && (int)lf->line_count <= term_h,
          "fuzzy: fullscreen list frame fits terminal");
    sc_rendered_free(lf);
    sc_fuzzy_free(tl);

    /* A footer wider than the terminal soft-wraps to >1 row; the frame must
       leave room for ALL of those rows (else the wrapped footer is clipped). */
    ScShortcut wide[] = {
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'a' }, .hint_label =
          "alpha action" },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'b' }, .hint_label =
          "bravo action" },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'c' }, .hint_label =
          "charlie action" },
        { .chord = { .key = SC_KEY_CHAR, .codepoint = 'd' }, .hint_label =
          "delta action longer label" },
    };
    int footer_rows = sc_shortcut_hint_rows(wide, 4, 0, sc_term_width());
    ScFuzzy *wf = sc_fuzzy_new((ScFuzzyOpts){
        .fullscreen = true, .header = hdr, .box = { .enabled = true },
        .shortcuts = wide, .n_shortcuts = 4 });
    for (int i = 0; i < 60; i++) {
        char a[16];
        snprintf(a, sizeof a, "row %d", i);
        sc_fuzzy_add(wf, a);
    }
    ScRendered *wff = sc_fuzzy_frame(wf, "");
    CHECK(footer_rows >= 2,
          "fuzzy: wide footer soft-wraps to >1 row at the test width");
    CHECK(wff && (int)wff->line_count <= term_h - footer_rows,
          "fuzzy: fullscreen frame leaves room for the wrapped footer");
    sc_rendered_free(wff);
    sc_fuzzy_free(wf);
    sc_rendered_free(hdr);
}
