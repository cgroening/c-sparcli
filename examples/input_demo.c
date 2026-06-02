/*
 * input_demo.c
 *
 * Interactive walkthrough of every sparcli input widget. Run it in a real
 * terminal and step through the prompts; the rendered frames and the summary
 * lines it leaves behind are what the README screenshots capture.
 *
 * Build (after `make`):
 *   make run-example EX=input_demo
 * or:
 *   cc -std=c11 -Iinclude examples/input_demo.c libsparcli.a -o input_demo
 *
 * Esc or Ctrl-C cancels the current prompt; the demo then moves on to the next.
 */

#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/* Prints a centered section header so each widget is easy to frame. */
static void section(const char *title) {
    printf("\n");
    sc_rule_str(title, (ScRuleOpts){
        .type = SC_BORDER_DOUBLE,
        .title.style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                         SC_ANSI_COLOR_NONE },
        .title.halign = SC_ALIGN_CENTER,
    });
}

/* Common outcome line for a cancelled / no-TTY result. */
static void note_cancel(ScInputStatus st) {
    if (st == SC_INPUT_CANCELLED) {
        sc_markup_println("  [dim]› cancelled[/]");
    } else if (st == SC_INPUT_ERROR) {
        sc_markup_println("  [dim]› skipped (no interactive terminal)[/]");
    }
}


static void demo_confirm(void) {
    section("Confirm");
    bool ok = false;
    ScInputStatus st = sc_confirm(
        "Deploy to production?", &ok,
        (ScConfirmOpts){ .default_yes = false, .accent = SC_ANSI_COLOR_GREEN }
    );
    if (st == SC_INPUT_OK) {
        sc_markup_println(ok ? "  [green]› yes[/]" : "  [yellow]› no[/]");
    }
    note_cancel(st);
}

static bool not_empty(const char *value, void *ctx, const char **err) {
    (void)ctx;
    if (value[0] == '\0') { *err = "Please enter a value"; return false; }
    return true;
}

static void demo_text(void) {
    section("Text Input");

    /* Plain inline entry with placeholder + validation. Ctrl-G opens $EDITOR
     * (newlines in the result are collapsed to spaces for a single line). */
    char *name = NULL;
    ScInputStatus st = sc_text_input(
        "Your name (Ctrl-G editor)", &name,
        (ScTextInputOpts){ .placeholder = "Ada Lovelace", .validate = not_empty,
                           .external_editor = true }
    );
    if (st == SC_INPUT_OK) { printf("  -> \"%s\"\n", name); free(name); }
    note_cancel(st);

    /* Autocomplete ghost + a no-space filter; Tab accepts the suggestion. */
    static const char *const cmds[] = {
        "commit", "checkout", "cherry-pick", "clone", "config"
    };
    char *cmd = NULL;
    st = sc_text_input(
        "Git command", &cmd,
        (ScTextInputOpts){ .suggestions = cmds, .n_suggestions = 5,
                           .char_filter = sc_filter_no_space }
    );
    if (st == SC_INPUT_OK) { printf("  -> \"%s\"\n", cmd); free(cmd); }
    note_cancel(st);

    /* Autocomplete dropdown: fuzzy-matched list below the field; arrows
     * navigate, Tab/Enter accept the highlighted suggestion. */
    char *fuzzy_cmd = NULL;
    st = sc_text_input(
        "Git command (dropdown)", &fuzzy_cmd,
        (ScTextInputOpts){
            .suggestions = cmds, .n_suggestions = 5,
            .suggest = {
                .mode = SC_SUGGEST_DROPDOWN,
                .match = SC_SUGGEST_MATCH_FUZZY,
                .border = { .type = SC_BORDER_ROUNDED },
                .selected_style = {
                    SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK, SC_ANSI_COLOR_CYAN,
                },
            },
        }
    );
    if (st == SC_INPUT_OK) { printf("  -> \"%s\"\n", fuzzy_cmd); free(fuzzy_cmd); }
    note_cancel(st);

    /* Boxed mode: prompt on the top border, counter on the bottom-right. */
    char *title = NULL;
    st = sc_text_input(
        "Title", &title,
        (ScTextInputOpts){ .boxed = true, .width = 40, .max_chars = 32,
                           .placeholder = "Enter a title…" }
    );
    if (st == SC_INPUT_OK) { printf("  -> \"%s\"\n", title); free(title); }
    note_cancel(st);
}

static void demo_password(void) {
    section("Password");
    char *secret = NULL;
    ScInputStatus st = sc_password_input(
        "Password", &secret,
        (ScPasswordOpts){ .placeholder = "••••••", .max_chars = 64 }
    );
    if (st == SC_INPUT_OK) {
        printf("  -> captured %zu bytes (not echoed)\n", strlen(secret));
        free(secret);
    }
    note_cancel(st);
}

static void demo_number(void) {
    section("Number Input");
    double price = 9.99;
    ScInputStatus st = sc_number_input(
        "Price", &price,
        (ScNumberOpts){ .initial = 9.99, .min = 0, .max = 1000,
                        .step = 0.5, .decimals = 2, .boxed = true, .width = 28 }
    );
    if (st == SC_INPUT_OK) { printf("  -> %.2f\n", price); }
    note_cancel(st);

    /* Calculator mode: type "=" then an expression like =1,5+2*3; Enter
     * shows the result in the field, a second Enter submits it. The field
     * displays the rounded value, the submitted value keeps full precision.
     * Try "=1/3": a dim " = 0,3333333333" indicator marks the pending exact
     * result; editing the field discards it and raises a yellow warning. */
    double amount = 0;
    char *exact = NULL;
    st = sc_number_input(
        "Amount (try =1/3)", &amount,
        (ScNumberOpts){ .decimals = 2, .decimal_sep = ',', .start_empty = true,
                        .calculator = true, .out_text = &exact }
    );
    if (st == SC_INPUT_OK) {
        printf("  -> %.2f (exact: %s)\n", amount, exact ? exact : "?");
        free(exact);
    }
    note_cancel(st);
}

static void demo_textarea(void) {
    section("Textarea");
    char *text = NULL;
    ScInputStatus st = sc_textarea(
        "Notes (Ctrl-D submit · Ctrl-G editor)", &text,
        (ScTextareaOpts){ .placeholder = "Type multiple lines…",
                          .boxed = true, .width = 48,
                          /* Ctrl-G → $EDITOR (nvim) */
                          .external_editor = true }
    );
    if (st == SC_INPUT_OK) {
        printf("  -> %zu bytes\n", strlen(text));
        free(text);
    }
    note_cancel(st);
}

static const char *const FRUITS[] = {
    "Apple", "Banana", "Cherry", "Date", "Elderberry", "Fig", "Grape"
};

static void demo_select(void) {
    section("Select (single)");
    ScSelect *single = sc_select_new((ScSelectOpts){
        .prompt = "Pick a fruit", .accent = SC_ANSI_COLOR_MAGENTA });
    for (size_t i = 0; i < sizeof FRUITS / sizeof FRUITS[0]; i++) {
        sc_select_add(single, FRUITS[i]);
    }
    size_t idx[8];
    size_t n = 8;
    ScInputStatus st = sc_select_run(single, idx, &n);
    if (st == SC_INPUT_OK) {
        printf("  -> %s\n", FRUITS[idx[0]]);
    }
    note_cancel(st);
    sc_select_free(single);

    section("Select (multi)");
    ScSelect *multi = sc_select_new((ScSelectOpts){
        .prompt = "Pick toppings (space to toggle)", .multi = true,
        .accent = SC_ANSI_COLOR_YELLOW });
    for (size_t i = 0; i < sizeof FRUITS / sizeof FRUITS[0]; i++) {
        sc_select_add(multi, FRUITS[i]);
    }
    sc_select_set_checked(multi, 0, true);
    sc_select_set_checked(multi, 2, true);
    n = 8;
    st = sc_select_run(multi, idx, &n);
    if (st == SC_INPUT_OK) {
        printf("  -> %zu chosen:", n);
        for (size_t i = 0; i < n; i++) { printf(" %s", FRUITS[idx[i]]); }
        printf("\n");
    }
    note_cancel(st);
    sc_select_free(multi);
}

static void demo_fuzzy(void) {
    section("Fuzzy Finder (list)");
    const char *cities[] = { "Tokyo", "Toronto", "London", "Berlin",
                             "Boston", "Lisbon", "Amsterdam" };
    ScFuzzy *f = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Pick a city" });
    for (size_t i = 0; i < sizeof cities / sizeof cities[0]; i++) {
        sc_fuzzy_add(f, cities[i]);
    }
    size_t pick = 0;
    ScInputStatus st = sc_fuzzy_run(f, &pick);
    if (st == SC_INPUT_OK) { printf("  -> %s\n", cities[pick]); }
    note_cancel(st);
    sc_fuzzy_free(f);

    section("Fuzzy Finder (table)");
    const char *headers[] = { "City", "Country", "Pop. (M)" };
    ScFuzzy *ft = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search cities", .table = true,
        .headers = headers, .n_cols = 3, .accent = SC_ANSI_COLOR_BLUE });
    sc_fuzzy_add_row(ft, (const char *[]){ "Tokyo",  "Japan",    "37.4" }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "London", "UK",       "9.0"  }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "Berlin", "Germany",  "3.7"  }, 3);
    sc_fuzzy_add_row(ft, (const char *[]){ "Lisbon", "Portugal", "0.5"  }, 3);
    st = sc_fuzzy_run(ft, &pick);
    if (st == SC_INPUT_OK) { printf("  -> row %zu\n", pick); }
    note_cancel(st);
    sc_fuzzy_free(ft);
}

static void demo_datepicker(void) {
    section("Date Picker");
    struct tm picked = { 0 };   /* zeroed → seeds today */
    ScInputStatus st = sc_datepicker(
        &picked,
        (ScDatePickerOpts){ .prompt = "Pick a date",
                            .week_start = SC_WEEK_START_MONDAY,
                            .accent = SC_ANSI_COLOR_BLUE }
    );
    if (st == SC_INPUT_OK) {
        char buf[64];
        strftime(buf, sizeof buf, "%A, %d %B %Y", &picked);
        printf("  -> %s\n", buf);
    }
    note_cancel(st);
}


int main(void) {
    sc_markup_println(
        "\n[bold]sparcli[/] input widgets - step through each prompt "
        "([dim]Esc/Ctrl-C to skip one[/])");

    if (!sc_input_available()) {
        sc_alert_warning(
            "No interactive terminal detected.\n"
            "Run this directly in a terminal (not under a pipe) to try the "
            "widgets.");
        return 0;
    }

    demo_confirm();
    demo_text();
    demo_password();
    demo_number();
    demo_textarea();
    demo_select();
    demo_fuzzy();
    demo_datepicker();

    section("Done");
    sc_markup_println("  [green]✔[/] You have seen every input widget.");
    return 0;
}
