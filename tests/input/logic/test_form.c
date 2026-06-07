#include "test_input.h"
#include "sparcli.h"

#include <string.h>


/* Internal frame builders (declared in input_internal.h, exported symbols). */
ScRendered *sc_form_frame(ScForm *form);
ScRendered *sc_form_frame_edit(ScForm *form, int field);
int sc_form_solve_columns_test(ScForm *form, int term_w,
                               int *colw, int *colx);


void test_form(void) {
    ScForm *form = sc_form_new((ScFormOpts){ .title = "Demo" });

    /* Row 1: two fields. */
    sc_form_row_begin(form);
    int name = sc_form_add_text(form, "Name", "Ada",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50 });
    int qty = sc_form_add_number(form, "Qty", 7,
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    /* Row 2: a rowspan field beside a bool. */
    sc_form_row_begin(form);
    int notes = sc_form_add_text(form, "Notes", "hi",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50,
                       .row_span = 2, .height = 2 });
    int active = sc_form_add_bool(form, "Active", true,
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
    sc_form_row_begin(form);
    sc_form_add_skip(form);   // covered by the rowspan "Notes"
    int city = sc_form_add_text(form, "City", "Linz",
        (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });

    CHECK(name == 0 && qty == 1 && notes == 2 && active == 3 && city == 4,
          "field indices are assigned in add order");

    /* Initial value read-back. */
    CHECK(strcmp(sc_form_get_string(form, name), "Ada") == 0,
          "text field returns its initial value");
    CHECK(sc_form_get_number(form, qty) == 7.0,
          "number field returns its initial value");
    CHECK(sc_form_get_bool(form, active) == true,
          "bool field returns its initial value");
    CHECK(strcmp(sc_form_get_string(form, notes), "hi") == 0,
          "rowspan text field returns its initial value");

    /* Type-mismatched / out-of-range getters are safe. */
    CHECK(sc_form_get_number(form, name) == 0.0,
          "get_number on a text field returns 0");
    CHECK(sc_form_get_bool(form, qty) == false,
          "get_bool on a number field returns false");
    CHECK(sc_form_get_string(form, 99) == NULL,
          "out-of-range get_string returns NULL");

    /* Frame renders a non-empty mosaic no wider than the terminal. */
    ScRendered *frame = sc_form_frame(form);
    CHECK(frame != NULL && frame->line_count > 0,
          "frame renders at least one line");
    if (frame) {
        int term = 400;   /* generous upper bound; real width is <= terminal */
        CHECK(frame->max_column_width > 0 && frame->max_column_width <= term,
              "frame width is positive and bounded");
        /* The rowspan layout produces a tall mosaic: title + 3 grid rows
           (each >= 3 lines) + blank gaps + edit/hint lines => many lines. */
        CHECK(frame->line_count >= 9,
              "rowspan mosaic stacks the expected number of lines");
        sc_rendered_free(frame);
    }

    sc_form_free(form);

    /* ── Select / multiselect ── */
    {
        static const char *const opt[] = { "Low", "Med", "High" };
        static const char *const tags[] = { "a", "b", "c", "d" };
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        int sel = sc_form_add_select(f, "Pri", opt, 3, 2, (ScFieldOpts){ 0 });
        int ms = sc_form_add_multiselect(f, "Tags", tags, 4,
                                         (const size_t[]){ 1, 3 }, 2,
                                         (ScFieldOpts){ 0 });
        CHECK(sc_form_get_choice(f, sel) == 2,
              "select returns the preselected index");

        size_t out[4] = { 9, 9, 9, 9 };
        size_t n = sc_form_get_checked(f, ms, out, 4);
        CHECK(n == 2 && out[0] == 1 && out[1] == 3,
              "multiselect returns prechecked indices in order");
        CHECK(sc_form_get_checked(f, ms, NULL, 0) == 2,
              "get_checked(NULL) returns the count only");
        CHECK(sc_form_get_choice(f, ms) == 0,
              "get_choice on a multiselect returns 0");

        /* The list-edit render path is reachable without a TTY. */
        ScRendered *ef = sc_form_frame_edit(f, sel);
        CHECK(ef != NULL && ef->line_count > 0,
              "edit-state frame (select list) renders");
        sc_rendered_free(ef);

        sc_form_free(f);
    }

    /* ── Date ── */
    {
        struct tm seed = { 0 };
        seed.tm_year = 2026 - 1900; seed.tm_mon = 4; seed.tm_mday = 15;
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        int label = sc_form_add_text(f, "Label", "x", (ScFieldOpts){ 0 });
        int d = sc_form_add_date(f, "When", seed, (ScFieldOpts){ 0 });

        struct tm got = { 0 };
        CHECK(sc_form_get_date(f, d, &got) && got.tm_year == 2026 - 1900
              && got.tm_mon == 4 && got.tm_mday == 15,
              "date field returns its seeded value");
        CHECK(sc_form_get_date(f, label, &got) == false,
              "get_date on a non-date field returns false");

        ScRendered *ef = sc_form_frame_edit(f, d);
        CHECK(ef != NULL && ef->line_count > 0,
              "edit-state frame (month grid) renders");
        sc_rendered_free(ef);

        sc_form_free(f);
    }

    /* ── Optional date (no-date) ── */
    {
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        int empty = sc_form_add_date(f, "Due", (struct tm){ 0 },
            (ScFieldOpts){ .date_optional = true });
        struct tm seed = { 0 };
        seed.tm_year = 2026 - 1900; seed.tm_mon = 4; seed.tm_mday = 15;
        int set = sc_form_add_date(f, "Since", seed,
            (ScFieldOpts){ .date_optional = true });

        struct tm got = { 0 };
        CHECK(sc_form_get_date(f, empty, &got) == false,
              "optional date with zeroed initial starts empty (get_date false)");
        CHECK(sc_form_get_date(f, set, &got) && got.tm_mday == 15,
              "optional date with a real initial returns it");
        CHECK(sc_date_is_empty((struct tm){ 0 })
              && !sc_date_is_empty(got),
              "sc_date_is_empty detects the no-date sentinel");

        /* The empty field renders (placeholder) without a TTY. */
        ScRendered *fr = sc_form_frame(f);
        CHECK(fr != NULL && fr->line_count > 0, "optional-date form renders");
        sc_rendered_free(fr);

        sc_form_free(f);
    }

    /* ── Multiline text field ── */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .editor = "true" });
        sc_form_row_begin(f);
        int ml = sc_form_add_text(f, "Notes", "line one\nline two",
            (ScFieldOpts){ .row_span = 1, .height = 3, .multiline = true });
        CHECK(strcmp(sc_form_get_string(f, ml), "line one\nline two") == 0,
              "multiline field keeps embedded newlines");
        /* The nav frame renders the multi-line value across the box. */
        ScRendered *fr = sc_form_frame(f);
        CHECK(fr != NULL && fr->line_count > 0, "multiline form frame renders");
        sc_rendered_free(fr);
        sc_form_free(f);
    }

    /* ── Column-width solving (cumulative PCT rounding) ── */
    {
        int colw[8], colx[8];

        /* 5x20% collectively fill the full width with no gap. */
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        for (int c = 0; c < 5; c++)
            sc_form_add_text(f, "F", "",
                (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 20 });
        int total = sc_form_solve_columns_test(f, 100, colw, colx);
        CHECK(total == 100, "5x20% fills the full width exactly");
        CHECK(colw[0] == 20 && colw[4] == 20, "each 20% column is 20 cols");
        sc_form_free(f);

        /* Cumulative flooring recovers the column independent flooring drops:
           33/33/34 at width 80 -> 26+26+28 = 80, not 26+26+27 = 79. */
        f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "A", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
        sc_form_add_text(f, "B", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
        sc_form_add_text(f, "C", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 34 });
        total = sc_form_solve_columns_test(f, 80, colw, colx);
        CHECK(total == 80, "33/33/34% at width 80 fills exactly (no lost col)");
        sc_form_free(f);

        /* A single 60% column stays 60%. */
        f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Wide", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 60 });
        sc_form_add_text(f, "Rest", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
        sc_form_solve_columns_test(f, 100, colw, colx);
        CHECK(colw[0] == 60, "60% column is 60 cols of 100");
        CHECK(colw[1] == 40, "the AUTO column takes the remaining 40");
        sc_form_free(f);

        /* Mixed FIXED + PCT + AUTO: fixed takes its width, PCT its share of the
           terminal, AUTO the rest. */
        f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Fix", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_FIXED, .width = 20 });
        sc_form_add_text(f, "Pct", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50 });
        sc_form_add_text(f, "Auto", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_AUTO });
        total = sc_form_solve_columns_test(f, 100, colw, colx);
        CHECK(colw[0] == 20 && colw[1] == 50 && colw[2] == 30,
              "FIXED 20 + PCT 50% + AUTO 30 fill the width");
        CHECK(total == 100, "mixed FIXED/PCT/AUTO row fills the full width");
        sc_form_free(f);

        /* A tiny PCT is clamped up to FORM_MIN_COL (6). */
        f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Tiny", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 1 });
        sc_form_solve_columns_test(f, 100, colw, colx);
        CHECK(colw[0] == 6, "1% column clamps up to FORM_MIN_COL");
        sc_form_free(f);
    }

    /* ── fill_height is ignored outside fullscreen ── */
    {
        /* A non-fullscreen form with a fill_height field must NOT grow to the
           terminal height; the box stays at its normal (small) size. */
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Body", "x",
            (ScFieldOpts){ .multiline = true, .fill_height = true });
        ScRendered *fr = sc_form_frame(f);
        CHECK(fr != NULL && fr->line_count > 0 && fr->line_count < 12,
              "fill_height is a no-op without fullscreen (box stays small)");
        sc_rendered_free(fr);
        sc_form_free(f);
    }

    /* NULL-safety. */
    sc_form_free(NULL);
    CHECK(sc_form_get_string(NULL, 0) == NULL, "get_string(NULL) is safe");
    CHECK(sc_form_run(NULL) == SC_INPUT_ERROR, "run(NULL) returns error");
}
