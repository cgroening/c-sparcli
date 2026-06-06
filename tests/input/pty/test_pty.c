/*
 * Self-driving interactive test: forks each input widget onto a pseudo-terminal
 * and feeds it a canned keystroke script, asserting the returned value. Unlike
 * the manual interactive suite this needs no human, so it can run the
 * interactive code paths under AddressSanitizer in CI:
 *
 *     make test-input-pty           # functional self-check on a PTY
 *     (built with the sanitizer, it also gives interactive ASan coverage)
 */

#include "sparcli.h"

#if defined(__APPLE__)
#  include <util.h>
#else
#  include <pty.h>
#endif

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>


/* Path to a stub "editor" script (created in main): writes a fixed two-line
 * value to its file argument, so the external-editor path can be tested with no
 * human and no real editor. */
static char g_editor[256];

/* A stub "editor" that exits non-zero without changing the file (like `:cq`),
 * so the widget must keep its original value. */
static char g_editor_fail[256];


/* ── The widget under test, run in the forked child ─────────────────────── */

/** Callback-mode shortcut: counts fires via `user`, keeps the prompt open. */
static bool count_cb(int id, void *user) {
    (void)id;
    (*(int *)user)++;
    return true;   // keep the prompt open
}

/** Callback-mode shortcut: removes the highlighted select item, stays open. */
static bool remove_cb(int id, void *user) {
    (void)id;
    ScSelect *sl = user;
    sc_select_remove(sl, sc_select_cursor(sl));
    return true;
}

/** Returns 0 when the widget produced the expected value, else non-zero. */
static int child_case(int c) {
    switch (c) {
        case 0: {
            bool b = false;
            ScInputStatus s = sc_confirm("Proceed?", &b, (ScConfirmOpts){ 0 });
            return (s == SC_INPUT_OK && b) ? 0 : 1;
        }
        case 1: {
            char *t = NULL;
            ScInputStatus s = sc_text_input("Name", &t, (ScTextInputOpts){ 0 });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "hi") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 2: {
            double v = 0;
            ScInputStatus s = sc_number_input("Qty", &v,
                (ScNumberOpts){ .initial = 10, .step = 5 });
            return (s == SC_INPUT_OK && v == 20.0) ? 0 : 1;
        }
        case 3: {
            ScSelect *sl = sc_select_new((ScSelectOpts){ 0 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 1);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 4: {
            char *t = NULL;
            ScInputStatus s = sc_textarea("Notes", &t, (ScTextareaOpts){ 0 });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "a\nb") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 5: {
            time_t now = time(NULL);
            struct tm *lt = localtime(&now);
            int want_year = lt ? lt->tm_year + 1 : 0;
            struct tm picked = { 0 };  /* zeroed -> seeds today */
            ScInputStatus s = sc_datepicker(&picked, (ScDatePickerOpts){ 0 });
            return (s == SC_INPUT_OK && picked.tm_year == want_year) ? 0 : 1;
        }
        case 6: {
            /* RETURN-mode shortcut: F2 ends the prompt, reports id 42, and the
               widget still returns the typed value. */
            int fired = -1;
            ScShortcut sc[] = {
                { .chord = sc_key_fn(2), .id = 42, .mode = SC_SHORTCUT_RETURN,
                  .hint_label = "submit" },   /* exercises the hint footer */
            };
            char *t = NULL;
            ScTextInputOpts o = {
                .shortcuts = sc, .n_shortcuts = 1, .out_shortcut_id = &fired,
            };
            ScInputStatus s = sc_text_input("Name", &t, o);
            int ok = (s == SC_INPUT_OK && fired == 42 && t
                      && strcmp(t, "ab") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 7: {
            /* CALLBACK-mode shortcut: Ctrl-R fires twice, keeps the prompt
               open, then Enter selects the (unmoved) first item. */
            int count = 0;
            ScShortcut sc[] = {
                { .chord = sc_key_ctrl('r'), .id = 0,
                  .mode = SC_SHORTCUT_CALLBACK,
                  .on_fire = count_cb, .user = &count, .hint_label = "reload" },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && count == 2 && idx[0] == 0);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 8: {
            /* Callback removes the highlighted item live, twice, then Enter
               selects the remaining one. Exercises sc_select_remove + the
               empty-tolerant run tail under ASan. */
            ScShortcut sc[] = {
                { .chord = sc_key_ctrl('x'), .id = 0,
                  .mode = SC_SHORTCUT_CALLBACK,
                  .on_fire = remove_cb, .user = NULL, .hint_label = "delete" },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc[0].user = sl;   // same array the opts pointer references
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 0);  /* "c" remains at 0 */
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 9: {
            /* Esc must still cancel even with shortcuts configured. */
            ScShortcut sc[] = {
                { .chord = sc_key_fn(2), .id = 1, .mode = SC_SHORTCUT_RETURN },
            };
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .shortcuts = sc, .n_shortcuts = 1 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            sc_select_free(sl);
            return (s == SC_INPUT_CANCELLED) ? 0 : 1;
        }
        case 10: {
            /* Textarea: Ctrl-G opens the stub editor (writes a two-line
               value), then Ctrl-D submits. The editor output replaces it. */
            char *t = NULL;
            ScInputStatus s = sc_textarea("Notes", &t, (ScTextareaOpts){
                .external_editor = true, .editor = g_editor });
            int ok = (s == SC_INPUT_OK && t
                      && strcmp(t, "from-editor\nsecond") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 11: {
            /* Text-input: Ctrl-G opens the editor; a multi-line result has
               its newlines collapsed to spaces (single line). Enter submits. */
            char *t = NULL;
            ScInputStatus s = sc_text_input("Name", &t, (ScTextInputOpts){
                .external_editor = true, .editor = g_editor });
            int ok = (s == SC_INPUT_OK && t
                      && strcmp(t, "from-editor second") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 12: {
            /* Editor exits non-zero → original value is kept. */
            char *t = NULL;
            ScInputStatus s = sc_textarea("Notes", &t, (ScTextareaOpts){
                .initial = "keep", .external_editor = true,
                .editor = g_editor_fail });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "keep") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 13: {
            /* Decimal comma + exact text out: clear the seeded value, type
               "7,50", expect *out == 7.5 and out_text == "7.50" ('.'-form). */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v,
                (ScNumberOpts){ .decimals = 2, .decimal_sep = ',',
                                .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 7.5 && text
                      && strcmp(text, "7.50") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 14: {
            /* Clamp + out_text agreement: typed 150 exceeds max 100, so both
               *out and out_text must reflect the clamped value. */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Qty", &v,
                (ScNumberOpts){ .min = 0, .max = 100,
                                .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 100.0 && text
                      && strcmp(text, "100") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 15: {
            /* start_empty: the field starts empty and Enter on the empty
               field is ignored; only the typed "5" submits. */
            double v = -1;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v,
                (ScNumberOpts){ .decimals = 2, .start_empty = true,
                                .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 5.0 && text
                      && strcmp(text, "5.00") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 16: {
            /* Opts strings are copied: the heap prompt is freed right after
               construction; under ASan a kept pointer is a use-after-free. */
            char *prompt = strdup("Category");
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .prompt = prompt });
            free(prompt);
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 0);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 17: {
            /* Theme glyph strings are copied: the heap marker is freed right
               after set_theme; the themed select must still render it. */
            char *marker = strdup("=> ");
            sc_input_set_theme(&(ScInputTheme){ .cursor_marker = marker });
            free(marker);
            ScSelect *sl = sc_select_new((ScSelectOpts){ 0 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            sc_select_free(sl);
            sc_input_set_theme(NULL);
            return (s == SC_INPUT_OK && idx[0] == 0) ? 0 : 1;
        }
        case 18: {
            /* Dropdown autocomplete: type "ch", Down highlights the first
               match ("checkout"), Enter accepts it into the field, the second
               Enter submits the accepted value. */
            static const char *const cmds[] = {
                "commit", "checkout", "cherry-pick", "clone", "config",
            };
            char *t = NULL;
            ScInputStatus s = sc_text_input("Cmd", &t, (ScTextInputOpts){
                .suggestions = cmds, .n_suggestions = 5,
                .suggest = { .mode = SC_SUGGEST_DROPDOWN } });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "checkout") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 19: {
            /* Dropdown + fuzzy matching: "cp" matches only "cherry-pick";
               Tab accepts the best match without a highlighted row, then
               Enter submits it. */
            static const char *const cmds[] = {
                "commit", "checkout", "cherry-pick", "clone", "config",
            };
            char *t = NULL;
            ScInputStatus s = sc_text_input("Cmd", &t, (ScTextInputOpts){
                .suggestions = cmds, .n_suggestions = 5,
                .suggest = { .mode = SC_SUGGEST_DROPDOWN,
                             .match = SC_SUGGEST_MATCH_FUZZY } });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "cherry-pick") == 0);
            free(t);
            return ok ? 0 : 1;
        }
        case 20: {
            /* Calculator: "=1,5+2*3", Enter accepts the result into the
               field, the second Enter submits it. 7.5 is exactly
               representable, so value and text agree at full precision. */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v, (ScNumberOpts){
                .calculator = true, .decimals = 1, .decimal_sep = ',',
                .start_empty = true, .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 7.5 && text
                      && strcmp(text, "7.5") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 21: {
            /* Calculator: Enter on an invalid expression ("=1+") shows the
               error and must NOT submit; completing it to "=1+2" then
               accepting + submitting yields 3. */
            double v = 0;
            ScInputStatus s = sc_number_input("Qty", &v, (ScNumberOpts){
                .calculator = true, .start_empty = true });
            return (s == SC_INPUT_OK && v == 3.0) ? 0 : 1;
        }
        case 22: {
            /* Calculator default precision: "=10/3" displays rounded
               (decimals=2) but submits the full-precision result. */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v, (ScNumberOpts){
                .calculator = true, .decimals = 2, .start_empty = true,
                .out_text = &text });
            int ok = (s == SC_INPUT_OK && v > 3.33 && v < 3.34
                      && text && strncmp(text, "3.3333333333", 12) == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 23: {
            /* calc_store_rounded: the displayed (rounded) value is what
               gets submitted. */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v, (ScNumberOpts){
                .calculator = true, .calc_store_rounded = true, .decimals = 2,
                .start_empty = true, .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 3.33 && text
                      && strcmp(text, "3.33") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 24: {
            /* Editing after accepting a calc result discards the pending
               exact value: "=2*3" -> accept shows "6", typing '9' makes
               "69", Enter submits the edited text. */
            double v = 0;
            ScInputStatus s = sc_number_input("Qty", &v, (ScNumberOpts){
                .calculator = true, .start_empty = true });
            return (s == SC_INPUT_OK && v == 69.0) ? 0 : 1;
        }
        case 25: {
            /* Discard warning path: "=10/3" -> accept shows "3.33" with the
               exact value pending; backspace ("3.3") raises the warning and
               discards it, Enter submits the edited display value - never
               the exact 3.333... result. */
            double v = 0;
            char *text = NULL;
            ScInputStatus s = sc_number_input("Amount", &v, (ScNumberOpts){
                .calculator = true, .decimals = 2, .start_empty = true,
                .out_text = &text });
            int ok = (s == SC_INPUT_OK && v == 3.3 && text
                      && strcmp(text, "3.30") == 0);
            free(text);
            return ok ? 0 : 1;
        }
        case 26: {
            /* History recall: Up walks to "second", Up again to "first",
               Enter submits the recalled entry. Auto-add then appends the
               submitted line (different from the newest entry). */
            ScHistory *h = sc_history_new((ScHistoryOpts){ 0 });
            sc_history_add(h, "first");
            sc_history_add(h, "second");
            char *t = NULL;
            ScInputStatus s = sc_text_input("repl", &t,
                (ScTextInputOpts){ .history = h });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "first") == 0
                      && sc_history_count(h) == 3);
            free(t);
            sc_history_free(h);
            return ok ? 0 : 1;
        }
        case 27: {
            /* Recall + edit: Up recalls "list", typing " -a" appends to the
               recalled text, Enter submits "list -a". */
            ScHistory *h = sc_history_new((ScHistoryOpts){ 0 });
            sc_history_add(h, "list");
            char *t = NULL;
            ScInputStatus s = sc_text_input("repl", &t,
                (ScTextInputOpts){ .history = h });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "list -a") == 0);
            free(t);
            sc_history_free(h);
            return ok ? 0 : 1;
        }
        case 28: {
            /* Live-line restore: type "draft", Up recalls "older", Down
               returns to the preserved "draft", Enter submits it. */
            ScHistory *h = sc_history_new((ScHistoryOpts){ 0 });
            sc_history_add(h, "older");
            char *t = NULL;
            ScInputStatus s = sc_text_input("repl", &t,
                (ScTextInputOpts){ .history = h });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "draft") == 0
                      && sc_history_count(h) == 2);
            free(t);
            sc_history_free(h);
            return ok ? 0 : 1;
        }
        case 29: {
            /* no_history_add: the submitted line must NOT be recorded. */
            ScHistory *h = sc_history_new((ScHistoryOpts){ 0 });
            sc_history_add(h, "existing");
            char *t = NULL;
            ScInputStatus s = sc_text_input("repl", &t,
                (ScTextInputOpts){ .history = h, .no_history_add = true });
            int ok = (s == SC_INPUT_OK && t && strcmp(t, "new") == 0
                      && sc_history_count(h) == 1);
            free(t);
            sc_history_free(h);
            return ok ? 0 : 1;
        }
        case 30: {
            /* Boxed select: a framed list with bg + padding still submits the
               highlighted index (exercises sc_box_wrap under ASan/UBSan). */
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .prompt = "Pick",
                .box = {
                    .enabled = true, .width = 30,
                    .border = { .type = SC_BORDER_DOUBLE },
                    .bg = SC_ANSI_COLOR_BLUE,
                    .padding = { .top = 1, .right = 2, .bottom = 1, .left = 2 },
                    .margin = { .left = 1 },
                } });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 1);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 31: {
            /* Boxed fuzzy list: framed finder still narrows + picks. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .prompt = "Find",
                .box = {
                    .enabled = true,
                    .border = { .type = SC_BORDER_ROUNDED },
                    .padding = { .left = 1, .right = 1 },
                } });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 32: {
            /* Widget bg + content width + full-width cursor bar: exercises the
               cursor-row padding and the content min/max clamp under ASan. */
            ScSelect *sl = sc_select_new((ScSelectOpts){
                .prompt = "Pick",
                .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                    SC_ANSI_COLOR_MAGENTA },
                .box = { .bg = SC_ANSI_COLOR_BLACK,
                         .width_mode = SC_WIDTH_CONTENT, .min_width = 20 } });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 1);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 33: {
            /* Fuzzy with bg_extent=text (no cursor padding) + fixed width. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .prompt = "Find",
                .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                                    SC_ANSI_COLOR_MAGENTA },
                .box = { .bg_extent = SC_BG_EXTENT_TEXT,
                         .width_mode = SC_WIDTH_FIXED, .width = 24 } });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 34: {
            /* Default cycles: Up on the first row jumps to the last (index 2). */
            ScSelect *sl = sc_select_new((ScSelectOpts){ 0 });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 2);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 35: {
            /* Default cycles: Up on the first match jumps to the last (Rent). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ 0 });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 36: {
            /* no_cycle: Up on the first row stays put (index 0). */
            ScSelect *sl = sc_select_new((ScSelectOpts){ .no_cycle = true });
            sc_select_add(sl, "a");
            sc_select_add(sl, "b");
            sc_select_add(sl, "c");
            size_t idx[1] = { 0 }, n = 1;
            ScInputStatus s = sc_select_run(sl, idx, &n);
            int ok = (s == SC_INPUT_OK && idx[0] == 0);
            sc_select_free(sl);
            return ok ? 0 : 1;
        }
        case 37: {
            /* Sections: the cursor skips the non-selectable header. Two Downs
               from the first item land on the item after the second header. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Tasks" });
            sc_fuzzy_add_section(fz, "A");   /* idx 0 */
            sc_fuzzy_add(fz, "x");           /* idx 1 */
            sc_fuzzy_add(fz, "y");           /* idx 2 */
            sc_fuzzy_add_section(fz, "B");   /* idx 3 */
            sc_fuzzy_add(fz, "z");           /* idx 4 */
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 4);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 38: {
            /* Disabled rows are skipped by the cursor: Down from a lands on c. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "Tasks" });
            sc_fuzzy_add(fz, "a");
            sc_fuzzy_add(fz, "b");
            sc_fuzzy_add(fz, "c");
            sc_fuzzy_set_disabled(fz, 1, true);
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 2);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 39: {
            /* Multi-select: Space checks a, Down, Space checks b, Enter. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .multi = true });
            sc_fuzzy_add(fz, "a");
            sc_fuzzy_add(fz, "b");
            sc_fuzzy_add(fz, "c");
            size_t idx[8], n = 8;
            ScInputStatus s = sc_fuzzy_run_multi(fz, idx, &n);
            int ok = (s == SC_INPUT_OK && n == 2 && idx[0] == 0 && idx[1] == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 40: {
            /* Toggle-all (Ctrl-A) checks every selectable row. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .multi = true, .toggle_all_key = sc_key_ctrl('a') });
            sc_fuzzy_add(fz, "a");
            sc_fuzzy_add(fz, "b");
            sc_fuzzy_add(fz, "c");
            size_t idx[8], n = 8;
            ScInputStatus s = sc_fuzzy_run_multi(fz, idx, &n);
            int ok = (s == SC_INPUT_OK && n == 3);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 41: {
            /* Toggle-section (Ctrl-S) checks the cursor's group only. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .multi = true, .toggle_section_key = sc_key_ctrl('s') });
            sc_fuzzy_add_section(fz, "A");   /* idx 0 */
            sc_fuzzy_add(fz, "x");           /* idx 1 */
            sc_fuzzy_add(fz, "y");           /* idx 2 */
            sc_fuzzy_add_section(fz, "B");   /* idx 3 */
            sc_fuzzy_add(fz, "z");           /* idx 4 */
            size_t idx[8], n = 8;
            ScInputStatus s = sc_fuzzy_run_multi(fz, idx, &n);
            int ok = (s == SC_INPUT_OK && n == 2
                      && idx[0] == 1 && idx[1] == 2);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 42: {
            /* order = COLUMN sorts the 09:00 row before 14:00; Enter picks it
               (its original add-order index 1). */
            const char *h[] = { "Time", "Task" };
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .table = true, .headers = h, .n_cols = 2,
                .order = SC_FUZZY_ORDER_COLUMN, .order_column = 0 });
            sc_fuzzy_add_row(fz, (const char *[]){ "14:00", "Late" }, 2);
            sc_fuzzy_add_row(fz, (const char *[]){ "09:00", "Early" }, 2);
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 43: {
            /* initial_query pre-filters to "Rent"; Enter picks it (index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .initial_query = "Ren" });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 44: {
            /* Multi-select + RETURN shortcut (the todo action loop): Space
               checks item 0, Ctrl-D fires id 7 and ends the run with the
               checked set intact. */
            int act = -1;
            ScShortcut sk[] = {
                { .chord = sc_key_ctrl('d'), .id = 7,
                  .mode = SC_SHORTCUT_RETURN },
            };
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .multi = true, .shortcuts = sk, .n_shortcuts = 1,
                .out_shortcut_id = &act });
            sc_fuzzy_add(fz, "a");
            sc_fuzzy_add(fz, "b");
            size_t idx[8], n = 8;
            ScInputStatus s = sc_fuzzy_run_multi(fz, idx, &n);
            int ok = (s == SC_INPUT_OK && act == 7 && n == 1 && idx[0] == 0);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 45: {
            /* Modal, normal mode: bare 'j' navigates down (does not type),
               Enter selects -> Rent (index 1). The heap badge label is freed
               right after new() - it must have been copied (ASan check). */
            char *label = strdup("NORM");
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true, .normal_label = label });
            free(label);
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 46: {
            /* Modal: 'i' enters insert, "Ren" filters to Rent, Enter picks
               it (index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .modal = true });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 47: {
            /* Modal, start in insert: Esc returns to normal (does NOT cancel),
               then Down + Enter selects Rent (index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true, .start_in_insert = true });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 48: {
            /* Modal, normal mode: a bare 'x' RETURN shortcut (id 5) fires
               without a modifier. */
            int act = -1;
            ScShortcut sk[] = {
                { .chord = { .key = SC_KEY_CHAR, .codepoint = 'x' },
                  .id = 5, .mode = SC_SHORTCUT_RETURN },
            };
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true, .shortcuts = sk, .n_shortcuts = 1,
                .out_shortcut_id = &act });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && act == 5);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 49: {
            /* Modal, insert mode: the bare 'x' shortcut is suppressed - 'x'
               types into the query and filters to "xtra" (index 1); the
               shortcut never fires (act stays -1). */
            int act = -1;
            ScShortcut sk[] = {
                { .chord = { .key = SC_KEY_CHAR, .codepoint = 'x' },
                  .id = 5, .mode = SC_SHORTCUT_RETURN },
            };
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true, .start_in_insert = true,
                .shortcuts = sk, .n_shortcuts = 1, .out_shortcut_id = &act });
            sc_fuzzy_add(fz, "alpha");
            sc_fuzzy_add(fz, "xtra");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && act == -1 && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 50: {
            /* Non-modal: Ctrl-N moves the cursor down (the documented
               behavior), Enter selects Rent (index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ 0 });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 51: {
            /* Modal, normal mode: starts pre-filtered to "Gro" (Groceries);
               the bare clear key 'c' empties the query so all rows show again,
               the cursor snaps to the first row (Rent, index 0), Enter picks
               it - proving the clear happened (else it would pick index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true, .initial_query = "Gro",
                .clear_key = { .key = SC_KEY_CHAR, .codepoint = 'c' } });
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 0);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 52: {
            /* Modal, normal mode: bare 'k' moves up; from the first row it
               cycles to the last (Utilities, index 2). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .modal = true });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 2);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 53: {
            /* Modal, normal mode: bare 'G' jumps to the last row (index 2). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .modal = true });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 2);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 54: {
            /* Modal, normal mode: j,j moves to the last row, then 'g' jumps
               back to the first (index 0) - so 'g' is what makes it 0. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ .modal = true });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 0);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 55: {
            /* Non-modal: Ctrl-P moves the cursor up; from the first row it
               cycles to the last (index 2). Complements fuzzy-ctrl-n-nav. */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){ 0 });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 2);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 56: {
            /* Modal with an overridden insert key 'a': 'a' enters insert mode,
               "Ren" filters to Rent, Enter picks it (index 1). */
            ScFuzzy *fz = sc_fuzzy_new((ScFuzzyOpts){
                .modal = true,
                .insert_key = { .key = SC_KEY_CHAR, .codepoint = 'a' } });
            sc_fuzzy_add(fz, "Groceries");
            sc_fuzzy_add(fz, "Rent");
            sc_fuzzy_add(fz, "Utilities");
            size_t pick = 99;
            ScInputStatus s = sc_fuzzy_run(fz, &pick);
            int ok = (s == SC_INPUT_OK && pick == 1);
            sc_fuzzy_free(fz);
            return ok ? 0 : 1;
        }
        case 57: {
            /* Form: Enter opens the editor (seeded "a"), type "X" -> "aX",
               Enter saves, Ctrl-D submits. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && v && strcmp(v, "aX") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 58: {
            /* Form: Right moves to the bool field, Space toggles it on, Ctrl-D
               submits. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            sc_form_add_bool(f, "B", false, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            int ok = (s == SC_INPUT_OK && sc_form_get_bool(f, 1) == true);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 59: {
            /* Form: edit, type, Esc discards the edit, value stays "a". */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && v && strcmp(v, "a") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 60: {
            /* Form: Esc in navigation cancels the whole form. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            sc_form_free(f);
            return (s == SC_INPUT_CANCELLED) ? 0 : 1;
        }
        case 61: {
            /* Form number field: edit, Ctrl-U clear, type 42, save, submit. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_number(f, "N", 1, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            int ok = (s == SC_INPUT_OK && sc_form_get_number(f, 0) == 42.0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 62: {
            /* Form select: Enter opens the list, Down moves to index 1, Enter
               selects, Ctrl-D submits. */
            static const char *const opt[] = { "a", "b", "c" };
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_select(f, "P", opt, 3, 0, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            int ok = (s == SC_INPUT_OK && sc_form_get_choice(f, 0) == 1);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 63: {
            /* Form multiselect: open, Space checks index 0, Down, Space checks
               index 1, Enter confirms, Ctrl-D submits. */
            static const char *const opt[] = { "a", "b", "c" };
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_multiselect(f, "T", opt, 3, NULL, 0, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            size_t idx[3] = { 9, 9, 9 };
            size_t n = sc_form_get_checked(f, 0, idx, 3);
            int ok = (s == SC_INPUT_OK && n == 2 && idx[0] == 0 && idx[1] == 1);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 64: {
            /* Form select Esc keeps the original selection (index 1). */
            static const char *const opt[] = { "a", "b", "c" };
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_select(f, "P", opt, 3, 1, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            int ok = (s == SC_INPUT_OK && sc_form_get_choice(f, 0) == 1);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 65: {
            /* Required: first Ctrl-D is blocked (empty), then edit to "x" and
               Ctrl-D submits. A premature submit would end with an empty
               value and fail. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "", (ScFieldOpts){ .required = true });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && v && strcmp(v, "x") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 66: {
            /* Form date: seeded 2026-05-15, open, Right (→16), Enter picks,
               Ctrl-D submits. */
            struct tm d = { 0 };
            d.tm_year = 2026 - 1900; d.tm_mon = 4; d.tm_mday = 15;
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_date(f, "D", d, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            struct tm got = { 0 };
            int ok = (s == SC_INPUT_OK && sc_form_get_date(f, 0, &got)
                      && got.tm_mday == 16 && got.tm_mon == 4
                      && got.tm_year == 2026 - 1900);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 67: {
            /* Multiline field: Ctrl-G opens the stub editor (writes a two-line
               value), Ctrl-D submits. The newlines are preserved. */
            ScForm *f = sc_form_new((ScFormOpts){ .editor = g_editor });
            sc_form_add_text(f, "Notes", "old", (ScFieldOpts){
                .multiline = true });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && v
                      && strcmp(v, "from-editor\nsecond") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 68: {
            /* Multiline field: Enter ALSO opens the editor (besides Ctrl-G),
               then Ctrl-D submits. */
            ScForm *f = sc_form_new((ScFormOpts){ .editor = g_editor });
            sc_form_add_text(f, "Notes", "old", (ScFieldOpts){
                .multiline = true });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && v
                      && strcmp(v, "from-editor\nsecond") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 69: {
            /* Tab while editing: edit A (seed "a") -> "aX", Tab commits A and
               immediately opens B (seed "b") -> "bY", Enter saves, Ctrl-D. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            sc_form_add_text(f, "B", "b", (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            const char *a = sc_form_get_string(f, 0);
            const char *b = sc_form_get_string(f, 1);
            int ok = (s == SC_INPUT_OK && a && b
                      && strcmp(a, "aX") == 0 && strcmp(b, "bY") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 70: {
            /* Tab onto a bool only focuses it (no auto-toggle): edit A -> "aX",
               Tab lands on B in nav mode, Space toggles it on, Ctrl-D. A single
               Space yielding `true` proves Tab did not toggle first. */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            sc_form_add_bool(f, "B", false, (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            const char *a = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && a && strcmp(a, "aX") == 0
                      && sc_form_get_bool(f, 1) == true);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 71: {
            /* Robust nav: Title spans visually but occupies only grid col 0.
               Down->Priority, Right->Due, Right->Someday, Up must still reach
               Title (the old same-column scan found nothing). Edit Title to
               prove arrival: "t" -> "tZ". */
            static const char *const opt[] = { "lo", "hi" };
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "Title", "t",
                (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 100 });
            sc_form_row_begin(f);
            sc_form_add_select(f, "Priority", opt, 2, 0, (ScFieldOpts){ 0 });
            sc_form_add_text(f, "Due", "d", (ScFieldOpts){ 0 });
            sc_form_add_bool(f, "Someday", false, (ScFieldOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "Description", "desc",
                (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 100 });
            ScInputStatus s = sc_form_run(f);
            const char *title = sc_form_get_string(f, 0);
            int ok = (s == SC_INPUT_OK && title && strcmp(title, "tZ") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 72: {
            /* Right must step A -> B -> C across the row, NOT into the wide
               col_span select E below. From B, Right reaches C (col 2); a
               center-distance metric would wrongly pick E. Edit C: "c" -> "cZ". */
            static const char *const opt[] = { "x", "y" };
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            sc_form_add_text(f, "B", "b", (ScFieldOpts){ 0 });
            int fc = sc_form_add_text(f, "C", "c", (ScFieldOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "D", "d", (ScFieldOpts){ 0 });
            sc_form_add_select(f, "E", opt, 2, 0,
                (ScFieldOpts){ .col_span = 2 });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, fc);
            int ok = (s == SC_INPUT_OK && v && strcmp(v, "cZ") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        case 73: {
            /* Down must step A -> G -> N down column 0, landing on the row_span
               field N (rows 2-3, col 0), NOT its col-1 neighbour P. A
               center-distance metric shifts N's center down and wrongly picks P.
               Edit N: "n" -> "nZ". */
            ScForm *f = sc_form_new((ScFormOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "A", "a", (ScFieldOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_text(f, "G", "g", (ScFieldOpts){ 0 });
            sc_form_row_begin(f);
            int n = sc_form_add_text(f, "N", "n",
                (ScFieldOpts){ .row_span = 2, .height = 2 });
            sc_form_add_text(f, "P", "p", (ScFieldOpts){ 0 });
            sc_form_row_begin(f);
            sc_form_add_skip(f);
            sc_form_add_text(f, "Q", "q", (ScFieldOpts){ 0 });
            ScInputStatus s = sc_form_run(f);
            const char *v = sc_form_get_string(f, n);
            int ok = (s == SC_INPUT_OK && v && strcmp(v, "nZ") == 0);
            sc_form_free(f);
            return ok ? 0 : 1;
        }
        default: return 2;
    }
}


/* ── Parent: drive the PTY ──────────────────────────────────────────────── */

typedef struct Case {
    const char *name;
    const char *keys;
} Case;

static const Case CASES[] = {
    { "confirm",  "y" },
    { "text",     "hi\r" },
    { "number",   "\x1b[A\x1b[A\r" },   /* up, up, enter -> 20 */
    { "select",   "\x1b[B\r" },         /* down, enter -> index 1 */
    { "textarea", "a\rb\x04" },         /* a, newline, b, Ctrl-D -> "a\nb" */
    { "datepicker", "\x1b[6;2~\r" },    /* shift+pgdn (year+1), enter */
    { "shortcut-return", "ab\x1b[12~" },  /* type ab, F2 fires (id 42) */
    { "shortcut-callback", "\x12\x12\r" },  /* Ctrl-R x2, then enter */
    { "shortcut-remove", "\x18\x18\r" },  /* Ctrl-X x2 (remove), enter */
    { "esc-cancels", "\x1b" },  /* Esc still cancels with shortcuts */
    { "editor-textarea", "\x07\x04" },  /* Ctrl-G editor, Ctrl-D submits */
    { "editor-text-input", "\x07\r" },  /* Ctrl-G editor, Enter submits */
    { "editor-cancel-keeps", "\x07\x04" },  /* editor nonzero → keep value */
    { "number-decimal-comma", "\x15" "7,50\r" },  /* Ctrl-U clear, type, enter */
    { "number-clamp-text", "\x15" "150\r" },      /* typed 150 clamps to 100 */
    { "number-start-empty", "\r5\r" },  /* enter ignored while empty, then 5 */
    { "fuzzy-heap-prompt", "Gro\r" },   /* prompt freed after new() -> copied */
    { "theme-heap-strings", "\r" },     /* theme marker freed after set_theme */
    { "suggest-dropdown-enter", "ch\x1b[B\r\r" },  /* down, accept, submit */
    { "suggest-dropdown-fuzzy-tab", "cp\t\r" },    /* tab accepts best match */
    { "calc-accept-submit", "=1,5+2*3\r\r" },      /* accept 7,5 then submit */
    { "calc-invalid-then-fix", "=1+\r2\r\r" },     /* error, fix, accept, submit */
    { "calc-full-precision", "=10/3\r\r" },        /* rounded display, exact out */
    { "calc-store-rounded", "=10/3\r\r" },         /* rounded display == out */
    { "calc-edit-after-accept", "=2*3\r9\r" },     /* accept "6", edit to "69" */
    { "calc-discard-warning", "=10/3\r\x7f\r" },   /* edit discards exact value */
    { "history-recall", "\x1b[A\x1b[A\r" },        /* up, up -> oldest entry */
    { "history-edit-recalled", "\x1b[A -a\r" },    /* up, then append " -a" */
    { "history-live-restore", "draft\x1b[A\x1b[B\r" },  /* up, down -> draft */
    { "history-no-auto-add", "new\r" },            /* submit is not recorded */
    { "select-boxed", "\x1b[B\r" },                /* boxed list: down, enter */
    { "fuzzy-boxed", "Ren\r" },                    /* boxed fuzzy: query, enter */
    { "select-bg-bar", "\x1b[B\r" },               /* widget bg + cursor bar */
    { "fuzzy-bg-extent-text", "Ren\r" },           /* bg_extent=text + fixed */
    { "select-cycle-up", "\x1b[A\r" },             /* default: Up first -> last */
    { "fuzzy-cycle-up", "\x1b[A\r" },              /* default: Up first -> last */
    { "select-no-cycle-up", "\x1b[A\r" },          /* no_cycle: Up first stays */
    { "fuzzy-section-skip", "\x1b[B\x1b[B\r" },     /* 2x down skips header -> z */
    { "fuzzy-disabled-skip", "\x1b[B\r" },          /* down skips disabled -> c */
    { "fuzzy-multi-toggle", " \x1b[B \r" },         /* space, down, space, enter */
    { "fuzzy-toggle-all", "\x01\r" },               /* Ctrl-A checks all, enter */
    { "fuzzy-toggle-section", "\x13\r" },           /* Ctrl-S checks group, enter */
    { "fuzzy-order-column", "\r" },                 /* sorted: 09:00 first */
    { "fuzzy-initial-query", "\r" },                /* pre-filtered to Rent */
    { "fuzzy-multi-shortcut", " \x04" },            /* space, Ctrl-D (id 7) */
    { "modal-normal-nav", "j\r" },                  /* normal: j down -> Rent */
    { "modal-insert-filter", "iRen\r" },            /* i, type, enter -> Rent */
    { "modal-esc-normal", "\x1b\x1b[B\r" },         /* esc->normal, down, enter */
    { "modal-bare-shortcut", "x" },                 /* normal: bare 'x' fires */
    { "modal-insert-types-key", "x\r" },            /* insert: 'x' types -> xtra */
    { "fuzzy-ctrl-n-nav", "\x0e\r" },               /* Ctrl-N down -> Rent */
    { "modal-clear-query", "c\r" },                 /* normal: 'c' clears query */
    { "modal-vim-k", "k\r" },                       /* normal: k up cycles -> last */
    { "modal-vim-G", "G\r" },                       /* normal: G -> last */
    { "modal-vim-g", "jjg\r" },                     /* j,j to last, g -> first */
    { "fuzzy-ctrl-p-nav", "\x10\r" },               /* Ctrl-P up cycles -> last */
    { "modal-custom-keys", "aRen\r" },              /* custom insert key 'a' */
    { "form-edit-text", "\rX\r\x04" },     /* edit, type X, save, Ctrl-D */
    { "form-bool-toggle", "\x1b[C \x04" }, /* right, space toggles, Ctrl-D */
    { "form-edit-cancel", "\rZ\x1b\x04" }, /* edit, type, Esc discard, Ctrl-D */
    { "form-esc-cancels", "\x1b" },         /* Esc in nav cancels */
    { "form-number-edit", "\r\x15" "42\r\x04" }, /* edit, clear, 42, save, submit */
    { "form-select", "\r\x1b[B\r\x04" },         /* open, down, select, submit */
    { "form-multiselect", "\r \x1b[B \r\x04" },  /* open, check 0, down, check 1 */
    { "form-select-esc-keeps", "\r\x1b[B\x1b\x04" }, /* move, Esc discards */
    { "form-required-blocks", "\x04\rx\r\x04" }, /* Ctrl-D blocked, then edit */
    { "form-date", "\r\x1b[C\r\x04" },           /* open, right (+1 day), pick */
    { "form-multiline-editor", "\x07\x04" },     /* Ctrl-G editor, Ctrl-D submit */
    { "form-multiline-enter", "\r\x04" },        /* Enter opens editor, Ctrl-D */
    { "form-tab-edit-advance", "\rX\tY\r\x04" }, /* edit A, Tab commits+edits B */
    { "form-tab-bool-focus", "\rX\t \x04" },     /* Tab onto bool: focus, not toggle */
    { "form-nav-robust", "\x1b[B\x1b[C\x1b[C\x1b[A\rZ\r\x04" }, /* up reaches title */
    { "form-nav-right-spans", "\x1b[C\x1b[C\rZ\r\x04" }, /* right skips wide col_span */
    { "form-nav-down-spans", "\x1b[B\x1b[B\rZ\r\x04" },  /* down hits row_span field */
};
#define N_CASES ((int)(sizeof CASES / sizeof CASES[0]))

static void msleep(int ms) {
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    nanosleep(&ts, NULL);
}

/** Reads and discards whatever the child rendered (keeps the PTY drained). */
static void drain(int fd) {
    for (;;) {
        struct timeval tv = { 0, 40000 };
        fd_set r;
        FD_ZERO(&r);
        FD_SET(fd, &r);
        if (select(fd + 1, &r, NULL, NULL, &tv) <= 0) { return; }
        char buf[4096];
        if (read(fd, buf, sizeof buf) <= 0) { return; }
    }
}

/** Runs one case; returns 1 on pass, 0 on fail. */
static int run_case(const Case *cs, int index) {
    int master;
    struct winsize ws = { .ws_row = 24, .ws_col = 80 };
    pid_t pid = forkpty(&master, NULL, NULL, &ws);
    if (pid < 0) { perror("forkpty"); return 0; }

    if (pid == 0) {
        _exit(child_case(index));  /* child: run the widget on the slave PTY */
    }

    msleep(200);                   /* let the child enter raw mode */
    drain(master);
    /* Send each keystroke atomically: an escape sequence (ESC + trailing bytes
     * up to the next ESC) goes in one write, otherwise a single byte. Real
     * terminals deliver a sequence as one burst; writing it byte-by-byte would
     * let the reader's 25 ms lone-Esc timeout split it. */
    for (const char *k = cs->keys; *k; ) {
        size_t len = 1;
        if ((unsigned char)*k == 0x1b) {
            const char *end = k + 1;
            while (*end && (unsigned char)*end != 0x1b) { end++; }
            len = (size_t)(end - k);
        }
        ssize_t w = write(master, k, len);
        (void)w;
        k += len;
        msleep(60);
        drain(master);
    }

    /* Wait for the child, draining so it never blocks on a full PTY buffer. */
    int status = 0;
    for (int i = 0; i < 100; i++) {
        if (waitpid(pid, &status, WNOHANG) == pid) { break; }
        drain(master);
        msleep(20);
    }
    if (!WIFEXITED(status)) { waitpid(pid, &status, 0); }
    close(master);

    int pass = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    const char *mark = pass ? "\033[32m✔\033[0m" : "\033[31m✘\033[0m";
    printf("  %s %s\n", mark, cs->name);
    return pass;
}

/** Writes the stub "editor" script to `g_editor`; returns 0 on success. */
static int make_stub_editor(void) {
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) { dir = "/tmp"; }
    snprintf(g_editor, sizeof g_editor, "%s/sparcli-test-editor-XXXXXX", dir);
    int fd = mkstemp(g_editor);
    if (fd < 0) { return -1; }
    /* Overwrite the file it is given with a fixed two-line value. */
    const char *script = "#!/bin/sh\nprintf 'from-editor\\nsecond' > \"$1\"\n";
    if (write(fd, script, strlen(script)) < 0) { close(fd); return -1; }
    close(fd);
    if (chmod(g_editor, 0700) != 0) { return -1; }

    /* A second stub that fails without writing (editor "cancel"). */
    snprintf(g_editor_fail, sizeof g_editor_fail,
             "%s/sparcli-test-edfail-XXXXXX", dir);
    int fd2 = mkstemp(g_editor_fail);
    if (fd2 < 0) { return -1; }
    const char *fail = "#!/bin/sh\nexit 1\n";
    if (write(fd2, fail, strlen(fail)) < 0) { close(fd2); return -1; }
    close(fd2);
    return chmod(g_editor_fail, 0700);
}

int main(void) {
    /* The parent needs no controlling terminal - forkpty creates a fresh PTY
     * for each child, which becomes that child's controlling terminal. So this
     * runs headless (CI) as well as from an interactive shell. */
    if (make_stub_editor() != 0) {
        fprintf(stderr, "could not create stub editor\n");
        return 1;
    }
    printf("\nDriving input widgets over a pseudo-terminal:\n");
    int passed = 0;
    for (int i = 0; i < N_CASES; i++) { passed += run_case(&CASES[i], i); }
    unlink(g_editor);
    unlink(g_editor_fail);
    printf("%d/%d cases passed\n", passed, N_CASES);
    return passed == N_CASES ? 0 : 1;
}
