#include "test_input.h"
#include "sparcli.h"


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
