/*
 * readme_screenshots_input.c
 *
 * Generates the INPUT-widget screenshots referenced in README.md — the input
 * counterpart of readme_screenshots_output.c. It is fully NON-interactive: it
 * renders each prompt through the internal snapshot frame builders
 * (`sc_*_frame`, declared in src/input/input_internal.h), exactly as the style
 * snapshot suite does, so there is no raw mode and no keystrokes. Sections:
 *
 *   1  hero    – a collage of input-widget frames composed side by side
 *   2  gallery – one representative frame per input widget
 *
 * Build & run (after `make`):
 *   make run-example EX=readme_screenshots_input
 * or:
 *   cc -std=c11 -Iinclude -Isrc examples/readme_screenshots_input.c \
 *       libsparcli.a -o readme_shots_input
 *
 * Run a single section by passing its number:
 *   ./readme_shots_input      # both sections, separated by rules
 *   ./readme_shots_input 1    # only the hero collage
 *   ./readme_shots_input 2    # only the per-widget gallery
 */

#include "sparcli.h"
#include "input/input_internal.h"   /* internal sc_*_frame snapshot builders */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>


/* ─────────────────────────────────────────────────────────── styles ── */

static const ScTextStyle s_bold   = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                                       SC_ANSI_COLOR_NONE };
static const ScTextStyle s_dim    = { SC_TEXT_ATTR_DIM,  SC_ANSI_COLOR_NONE,
                                       SC_ANSI_COLOR_NONE };
static const ScTextStyle s_b_cyan = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                                       SC_ANSI_COLOR_NONE };


/* ─────────────────────────────────────────────────────────── helpers ── */

static void section_rule(const char *title) {
    sc_rule_str(title, (ScRuleOpts){
        .type         = SC_BORDER_DOUBLE,
        .color        = SC_ANSI_COLOR_NONE,
        .title.style  = s_bold,
        .title.halign = SC_ALIGN_CENTER,
        .title.pad    = 1,
        .margin       = { 1, 0, 1, 0 },
    });
}

/* Prints a dim caption, then the captured frame indented by two columns, and
 * frees the frame (which the frame builders hand to us as an owned value). */
static void show(const char *caption, ScRendered *frame) {
    sc_println(caption, s_dim);
    if (frame) {
        sc_pad_print(frame, (ScPadOpts){ .left = 2 });
        sc_rendered_free(frame);
    }
    printf("\n");
}

/* Fixed seed so the month grid is deterministic across runs: 2026-05-15. */
static struct tm seed_date(void) {
    struct tm t = { 0 };
    t.tm_year = 2026 - 1900;
    t.tm_mon  = 4;    /* May */
    t.tm_mday = 15;
    return t;
}

/* Builds the vertical-separator column layout shared by all hero rows. */
static ScColumns *hero_columns(void) {
    return sc_columns_new((ScColumnsOpts){
        .gap    = 2,
        .sep    = { .type = SC_BORDER_SINGLE, .color = SC_ANSI_COLOR_NONE },
        .valign = SC_VALIGN_TOP,
    });
}


/* ───────────────────────────────────────────────── 1. hero collage ── */

static void hero_intro_panel(void) {
    sc_panel_str(
        "Interactive prompts, rendered statically — "
        "confirm · text · number · select · fuzzy · date.",
        (ScPanelOpts){
            .border = { .type = SC_BORDER_ROUNDED, .color = SC_ANSI_COLOR_CYAN },
            .title  = {
                .text   = " sparcli · input widgets ",
                .style  = s_b_cyan,
                .halign = SC_ALIGN_CENTER,
                .pad    = 1,
                .pos    = SC_POSITION_TOP,
            },
            .padding       = { 0, 2, 0, 2 },
            .content_align = SC_ALIGN_CENTER,
            .full_width    = 1,
        });
}

/* Row 1: a confirm prompt, a single-select menu and a multi-select list. */
static void hero_row_choices(void) {
    ScRendered *r_confirm = sc_confirm_frame("Deploy to production?", true,
        (ScConfirmOpts){ .accent = SC_ANSI_COLOR_GREEN });

    ScSelect *single = sc_select_new((ScSelectOpts){
        .prompt = "Environment", .accent = SC_ANSI_COLOR_CYAN });
    sc_select_add(single, "staging");
    sc_select_add(single, "production");
    sc_select_add(single, "local");
    sc_select_set_cursor(single, 1);
    ScRendered *r_single = sc_select_frame(single);

    ScSelect *multi = sc_select_new((ScSelectOpts){
        .prompt = "Targets", .multi = true, .accent = SC_ANSI_COLOR_GREEN });
    sc_select_add(multi, "web");
    sc_select_add(multi, "api");
    sc_select_add(multi, "worker");
    sc_select_add(multi, "db");
    sc_select_set_checked(multi, 0, true);
    sc_select_set_checked(multi, 2, true);
    ScRendered *r_multi = sc_select_frame(multi);

    ScColumns *cols = hero_columns();
    sc_columns_add_rendered(cols, r_confirm, (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_single,  (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_multi,   (ScColItem){ 0 });
    sc_columns_print(cols);

    sc_columns_free(cols);
    sc_rendered_free(r_confirm);
    sc_rendered_free(r_single);
    sc_rendered_free(r_multi);
    sc_select_free(single);
    sc_select_free(multi);
}

/* Row 2: boxed text, password and number fields (a uniform card look). */
static void hero_row_fields(void) {
    ScRendered *r_text = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Service", .initial = "api-gateway",
        .boxed = true, .width = 26 });

    ScRendered *r_password = sc_text_entry_frame(&(ScTextEntryCfg){
        .prompt = "Password", .initial = "hunter2", .mask = "*",
        .boxed = true, .width = 22 });

    ScRendered *r_number = sc_number_frame("Replicas", 3,
        (ScNumberOpts){ .min = 1, .max = 10, .boxed = true, .width = 20 });

    ScColumns *cols = hero_columns();
    sc_columns_add_rendered(cols, r_text,     (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_password, (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_number,   (ScColItem){ 0 });
    sc_columns_print(cols);

    sc_columns_free(cols);
    sc_rendered_free(r_text);
    sc_rendered_free(r_password);
    sc_rendered_free(r_number);
}

/* Row 3: a textarea, a fuzzy finder (list view) and a date picker. */
static void hero_row_rich(void) {
    struct tm seed = seed_date();

    ScRendered *r_textarea = sc_textarea_frame("Notes",
        "first line\nsecond line", (ScTextareaOpts){ .boxed = true,
                                                     .width = 28 });

    const char *cities[] = { "Tokyo", "Toronto", "London", "Boston", "Lisbon" };
    ScFuzzy *fuzzy = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "City" });
    for (size_t i = 0; i < sizeof cities / sizeof cities[0]; i++) {
        sc_fuzzy_add(fuzzy, cities[i]);
    }
    ScRendered *r_fuzzy = sc_fuzzy_frame(fuzzy, "to");

    ScRendered *r_date = sc_datepicker_frame(&seed, (ScDatePickerOpts){
        .prompt = "Release date", .accent = SC_ANSI_COLOR_MAGENTA });

    ScColumns *cols = hero_columns();
    sc_columns_add_rendered(cols, r_textarea, (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_fuzzy,    (ScColItem){ 0 });
    sc_columns_add_rendered(cols, r_date,     (ScColItem){ 0 });
    sc_columns_print(cols);

    sc_columns_free(cols);
    sc_rendered_free(r_textarea);
    sc_rendered_free(r_fuzzy);
    sc_rendered_free(r_date);
    sc_fuzzy_free(fuzzy);
}

static void shot_hero(void) {
    hero_intro_panel();
    printf("\n");
    hero_row_choices();
    printf("\n");
    hero_row_fields();
    printf("\n");
    hero_row_rich();
}


/* ───────────────────────────────────────── 2. per-widget gallery ── */

static void gallery_confirm(void) {
    show("confirm — yes / no",
        sc_confirm_frame("Apply changes?", true,
            (ScConfirmOpts){ .accent = SC_ANSI_COLOR_GREEN }));
}

static void gallery_text(void) {
    show("text input — value with a character counter",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Name:", .initial = "Ada Lovelace", .max_chars = 40 }));
}

static void gallery_password(void) {
    show("password — masked input",
        sc_text_entry_frame(&(ScTextEntryCfg){
            .prompt = "Password:", .initial = "hunter2", .mask = "*" }));
}

static void gallery_number(void) {
    show("number — bounded value with step",
        sc_number_frame("Quantity", 10,
            (ScNumberOpts){ .min = 0, .max = 100, .step = 5 }));
}

static void gallery_textarea(void) {
    show("textarea — multi-line, boxed",
        sc_textarea_frame("Notes", "first line\nsecond line\nthird line",
            (ScTextareaOpts){ .boxed = true, .width = 36 }));
}

static void gallery_select(void) {
    const char *items[] = { "Apple", "Banana", "Cherry", "Date" };
    ScSelect *select = sc_select_new((ScSelectOpts){
        .prompt = "Pick one", .accent = SC_ANSI_COLOR_CYAN });
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        sc_select_add(select, items[i]);
    }
    show("select — single choice", sc_select_frame(select));
    sc_select_free(select);
}

static void gallery_multiselect(void) {
    const char *items[] = { "Apple", "Banana", "Cherry", "Date" };
    ScSelect *select = sc_select_new((ScSelectOpts){
        .prompt = "Pick many", .multi = true, .accent = SC_ANSI_COLOR_GREEN });
    for (size_t i = 0; i < sizeof items / sizeof items[0]; i++) {
        sc_select_add(select, items[i]);
    }
    sc_select_set_checked(select, 0, true);
    sc_select_set_checked(select, 2, true);
    show("multi-select — checkboxes, two checked", sc_select_frame(select));
    sc_select_free(select);
}

static void gallery_fuzzy_list(void) {
    const char *cities[] = { "Tokyo", "Toronto", "London", "Boston", "Lisbon" };
    ScFuzzy *fuzzy = sc_fuzzy_new((ScFuzzyOpts){ .prompt = "City" });
    for (size_t i = 0; i < sizeof cities / sizeof cities[0]; i++) {
        sc_fuzzy_add(fuzzy, cities[i]);
    }
    show("fuzzy finder — list view, query 'to' (matches highlighted)",
        sc_fuzzy_frame(fuzzy, "to"));
    sc_fuzzy_free(fuzzy);
}

static void gallery_fuzzy_table(void) {
    const char *headers[] = { "City", "Country", "Pop." };
    ScFuzzy *fuzzy = sc_fuzzy_new((ScFuzzyOpts){
        .prompt = "Search", .table = true, .headers = headers, .n_cols = 3 });
    sc_fuzzy_add_row(fuzzy, (const char *[]){ "Tokyo",  "Japan",    "37.4" }, 3);
    sc_fuzzy_add_row(fuzzy, (const char *[]){ "London", "UK",       "9.0"  }, 3);
    sc_fuzzy_add_row(fuzzy, (const char *[]){ "Lisbon", "Portugal", "0.5"  }, 3);
    show("fuzzy finder — table view", sc_fuzzy_frame(fuzzy, ""));
    sc_fuzzy_free(fuzzy);
}

static void gallery_datepicker(void) {
    struct tm seed = seed_date();
    show("date picker — month grid",
        sc_datepicker_frame(&seed, (ScDatePickerOpts){ .prompt = "Pick a date" }));
}

static void shot_gallery(void) {
    gallery_confirm();
    gallery_text();
    gallery_password();
    gallery_number();
    gallery_textarea();
    gallery_select();
    gallery_multiselect();
    gallery_fuzzy_list();
    gallery_fuzzy_table();
    gallery_datepicker();
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
        section_rule(" 2. Widget gallery ");
        shot_gallery();
    }
    return 0;
}
