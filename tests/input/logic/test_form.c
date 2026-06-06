#include "test_input.h"
#include "sparcli.h"

#include <string.h>


/* Internal frame builders (declared in input_internal.h, exported symbols). */
ScRendered *sc_form_frame(ScForm *form);
ScRendered *sc_form_frame_edit(ScForm *form, int field);


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

    /* NULL-safety. */
    sc_form_free(NULL);
    CHECK(sc_form_get_string(NULL, 0) == NULL, "get_string(NULL) is safe");
    CHECK(sc_form_run(NULL) == SC_INPUT_ERROR, "run(NULL) returns error");
}
