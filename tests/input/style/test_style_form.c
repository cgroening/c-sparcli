#include "test_style.h"
#include "input/input_internal.h"


/** Renders `form`'s current frame under `caption`, then frees the form. */
static void show_form(const char *caption, ScForm *form) {
    style_show_flush(caption, sc_form_frame(form));
    sc_form_free(form);
}


void style_form(void) {
    /* Three equal-width fields in one row (active = first, accent border). */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Row of three" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "First", "Apple",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
        sc_form_add_text(f, "Last", "Inc.",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 33 });
        sc_form_add_text(f, "Role", "Vendor", (ScFieldOpts){ 0 });
        show_form("form: three columns, percent widths", f);
    }

    /* A 60/40 split with a number field and an empty placeholder. */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Split + types" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Email", "",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 60 });
        sc_form_add_number(f, "Qty", 42, (ScFieldOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_bool(f, "Active", true, (ScFieldOpts){ 0 });
        sc_form_add_bool(f, "Archived", false, (ScFieldOpts){ 0 });
        show_form("form: 60/40 split, number + empty + bool fields", f);
    }

    /* The rowspan layout: a tall field beside two stacked rows. */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Spanning grid",
                                              .accent = SC_ANSI_COLOR_GREEN });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Notes", "preferred supplier",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50,
                           .row_span = 2, .height = 2 });
        sc_form_add_text(f, "Phone", "+1 555 0100", (ScFieldOpts){ 0 });
        sc_form_add_bool(f, "VIP", true, (ScFieldOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_skip(f);   // covered by the rowspan "Notes"
        sc_form_add_text(f, "City", "Cupertino", (ScFieldOpts){ 0 });
        sc_form_add_text(f, "ZIP", "95014", (ScFieldOpts){ 0 });
        show_form("form: rowspan field beside stacked rows", f);
    }

    /* Fixed-width column + help line in the editor region. */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Fixed width" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Code", "AB12",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_FIXED, .width = 16,
                           .help = "two letters + two digits" });
        sc_form_add_text(f, "Label", "Widget", (ScFieldOpts){ 0 });
        show_form("form: fixed column width + field help", f);
    }

    /* Select + multiselect fields (nav mode shows the chosen values). */
    {
        static const char *const pri[] = { "Low", "Medium", "High", "Urgent" };
        static const char *const tags[] = { "backend", "frontend", "docs" };
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Choices" });
        sc_form_row_begin(f);
        sc_form_add_select(f, "Priority", pri, 4, 2, (ScFieldOpts){ 0 });
        sc_form_add_multiselect(f, "Tags", tags, 3,
            (const size_t[]){ 0, 2 }, 2, (ScFieldOpts){ 0 });
        show_form("form: select + multiselect (chosen values)", f);
    }

    /* The select list editor, open below the grid. */
    {
        static const char *const pri[] = { "Low", "Medium", "High", "Urgent" };
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Editing a select" });
        sc_form_row_begin(f);
        sc_form_add_select(f, "Priority", pri, 4, 2, (ScFieldOpts){ 0 });
        sc_form_add_text(f, "Note", "x", (ScFieldOpts){ 0 });
        style_show_flush("form: select list open below the grid",
                         sc_form_frame_edit(f, 0));
        sc_form_free(f);
    }

    /* The multiselect checkbox list editor. */
    {
        static const char *const tags[] = {
            "backend", "frontend", "docs", "urgent" };
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Editing tags" });
        sc_form_row_begin(f);
        sc_form_add_multiselect(f, "Tags", tags, 4,
            (const size_t[]){ 0, 3 }, 2, (ScFieldOpts){ 0 });
        style_show_flush("form: multiselect checkbox list open",
                         sc_form_frame_edit(f, 0));
        sc_form_free(f);
    }

    /* Date field: nav value + the month-grid editor (fixed seed). */
    {
        struct tm seed = { 0 };
        seed.tm_year = 2026 - 1900; seed.tm_mon = 4; seed.tm_mday = 15;
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Booking" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Guest", "Ada",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 50 });
        sc_form_add_date(f, "Check-in", seed, (ScFieldOpts){ 0 });
        show_form("form: date field (YYYY-MM-DD value)", f);
    }
    {
        struct tm seed = { 0 };
        seed.tm_year = 2026 - 1900; seed.tm_mon = 4; seed.tm_mday = 15;
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Editing a date" });
        sc_form_row_begin(f);
        sc_form_add_date(f, "Check-in", seed, (ScFieldOpts){ 0 });
        sc_form_add_text(f, "Guest", "Ada", (ScFieldOpts){ 0 });
        style_show_flush("form: month-grid editor open below the grid",
                         sc_form_frame_edit(f, 0));
        sc_form_free(f);
    }

    /* Text input: the boxed accent editor open below the grid. */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Editing text" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Name", "Ada Lovelace", (ScFieldOpts){ 0 });
        sc_form_add_text(f, "Role", "Pioneer", (ScFieldOpts){ 0 });
        style_show_flush("form: boxed accent text editor open",
                         sc_form_frame_edit(f, 0));
        sc_form_free(f);
    }

    /* Multiline field: value shown across the box; nav help names the key. */
    {
        ScForm *f = sc_form_new((ScFormOpts){ .title = "Multiline",
                                              .editor = "nvim" });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Notes", "first line\nsecond line\nthird line",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 60,
                           .height = 3, .multiline = true });
        sc_form_add_text(f, "Tag", "vip", (ScFieldOpts){ 0 });
        show_form("form: multiline field (newlines shown in the box)", f);
    }

    /* Fullscreen: a pinned header + valign MIDDLE. The grid is centered within
       the terminal height (24 off-TTY) - the leading blank rows are the top
       half of the free space; the borrowed header sits above the grid. */
    {
        ScRendered *hdr = sc_capture_str("== form header ==");
        ScForm *f = sc_form_new((ScFormOpts){
            .fullscreen = true, .valign = SC_VALIGN_MIDDLE, .header = hdr });
        sc_form_row_begin(f);
        sc_form_add_text(f, "Title", "Apple",
            (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 100 });
        show_form("form: fullscreen header + valign middle", f);
        sc_rendered_free(hdr);
    }
}
