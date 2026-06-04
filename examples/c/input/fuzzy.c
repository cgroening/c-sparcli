/*
 * fuzzy.c - fuzzy finder: filter a list or a table while typing.
 *
 * Run (after `make`) in a real terminal:
 *   make run-example EX=c/input/fuzzy
 *
 * Type to filter, arrows move the highlight, Enter picks, Esc cancels.
 */

#include <sparcli.h>

#include <stdio.h>


static void run_list_finder(void);
static void run_table_finder(void);
static void show_pure_matcher(void);


int main(void) {
    if (!sc_input_available()) {
        sc_alert_warning("Run this example in a real terminal "
                         "(not under a pipe).");
        // The match scorer is pure and works without a terminal.
        show_pure_matcher();
        return 0;
    }

    run_list_finder();
    run_table_finder();
    show_pure_matcher();
    return 0;
}

/** Simple list mode: one label per entry. */
static void run_list_finder(void) {
    static const char *const BRANCHES[] = {
        "main", "develop", "feature/fuzzy-finder", "feature/live-display",
        "fix/table-wrap", "release/1.2",
    };

    ScFuzzy *finder = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Switch branch",
        .accent = SC_ANSI_COLOR_CYAN,
        // Rounded frame + dark content background: every result row inherits
        // the background, and the cursor row is a full-width highlight bar.
        // The box fits the content width by default.
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            sc_color_from_rgb(255, 121, 198) },
        .box = {
            .enabled = true,
            .border  = { .type = SC_BORDER_ROUNDED,
                         .color = sc_color_from_rgb(255, 121, 198) },
            .bg      = sc_color_from_rgb(30, 30, 46),
            .padding = { .left = 1, .right = 1 },
        },
    });
    for (size_t i = 0; i < sizeof BRANCHES / sizeof BRANCHES[0]; i++) {
        sc_fuzzy_add(finder, BRANCHES[i]);
    }

    size_t picked = 0;
    ScInputStatus status = sc_fuzzy_run(finder, &picked);
    if (status == SC_INPUT_OK) {
        // The index refers to the original add order, not the filtered view.
        printf("  -> %s\n", BRANCHES[picked]);
    }
    sc_fuzzy_free(finder);
}

/** Table mode: the query searches and highlights across all columns. */
static void run_table_finder(void) {
    static const char *const HEADERS[] = { "Host", "Region", "Status" };

    ScFuzzy *finder = sc_fuzzy_new((ScFuzzyOpts){
        .prompt  = "Pick a host",
        .table   = true,
        .headers = HEADERS,
        .n_cols  = 3,
        .accent  = SC_ANSI_COLOR_BLUE,
        // .search_columns = 0x1,  // bitmask; 0 = search all columns
        // Override the cursor-row highlight (default = accent background):
        // white text on a blue row.
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            SC_ANSI_COLOR_BLUE },
        // Frame the whole finder (query + table) in a panel.
        .box = {
            .enabled = true,
            .border  = { .type = SC_BORDER_ROUNDED,
                         .color = SC_ANSI_COLOR_BLUE },
            .padding = { .left = 1, .right = 1 },
        },
        .table_opts = {
            .border = { .type = SC_BORDER_ROUNDED },
            .header = { .row = true, .style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE,
            } },
        },
    });

    sc_fuzzy_add_row(finder,
        (const char *[]){ "web-01", "eu-central", "healthy" }, 3);
    sc_fuzzy_add_row(finder,
        (const char *[]){ "web-02", "eu-central", "degraded" }, 3);
    sc_fuzzy_add_row(finder,
        (const char *[]){ "db-01", "us-east", "healthy" }, 3);
    sc_fuzzy_add_row(finder,
        (const char *[]){ "cache-01", "ap-south", "healthy" }, 3);

    size_t picked = 0;
    ScInputStatus status = sc_fuzzy_run(finder, &picked);
    if (status == SC_INPUT_OK) {
        printf("  -> row %zu\n", picked);
    }
    sc_fuzzy_free(finder);
}

/** sc_fuzzy_match is the pure scoring function behind the widget. */
static void show_pure_matcher(void) {
    int score = 0;
    bool hit = sc_fuzzy_match("ffind", "feature/fuzzy-finder", &score);
    printf("  match \"ffind\" in \"feature/fuzzy-finder\": %s (score %d)\n",
           hit ? "yes" : "no", score);
}
