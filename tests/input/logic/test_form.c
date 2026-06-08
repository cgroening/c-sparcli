#include "test_input.h"
#include "sparcli.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


/* Internal frame builders (declared in input_internal.h, exported symbols). */
ScRendered *sc_form_frame(ScForm *form);
ScRendered *sc_form_frame_edit(ScForm *form, int field);
int sc_form_solve_columns_test(ScForm *form, int term_w,
                               int *colw, int *colx);
/* External editor runner (declared in input_internal.h). */
bool sc_run_editor(const char *cmd, const char *initial, const char *suffix,
                   char **out);


/*
 * Drives `sc_run_editor` with a tiny `sh` script that records the temp-file path
 * it is handed (so the test can assert the suffix) and rewrites the file (so the
 * round-trip can be checked). Returns the captured path (heap; caller frees) and
 * the editor's round-trip output via `*out_body`; NULL on setup/run failure.
 * Invoked as `sh <script>` so no execute bit / shebang is required (CI-safe).
 */
static char *run_editor_capture(const char *suffix, char **out_body) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) { tmp = "/tmp"; }
    char script[512], capture[512], cmd[600];
    snprintf(script, sizeof script, "%s/sparcli_ed_%d.sh", tmp, (int)getpid());
    snprintf(capture, sizeof capture, "%s/sparcli_cap_%d", tmp, (int)getpid());
    unlink(capture);

    FILE *sf = fopen(script, "w");
    if (!sf) { return NULL; }
    fprintf(sf,
        "printf '%%s' \"$1\" > '%s'\n"       /* record the temp path */
        "printf 'edited-body' > \"$1\"\n",   /* round-trip: new content */
        capture);
    fclose(sf);
    snprintf(cmd, sizeof cmd, "sh %s", script);

    *out_body = NULL;
    bool ok = sc_run_editor(cmd, "seed", suffix, out_body);
    unlink(script);
    if (!ok) { unlink(capture); return NULL; }

    FILE *cf = fopen(capture, "r");
    char *path = NULL;
    if (cf) {
        char buf[1024];
        size_t got = fread(buf, 1, sizeof buf - 1, cf);
        buf[got] = '\0';
        fclose(cf);
        path = strdup(buf);
    }
    unlink(capture);
    return path;
}

/* First line index (ANSI-stripped) of `frame` containing `needle`, or -1. */
static int form_line_of(const ScRendered *frame, const char *needle) {
    if (!frame) { return -1; }
    for (size_t i = 0; i < frame->line_count; i++) {
        char *plain = sc_strip_ansi(frame->lines[i]);
        bool hit = plain && strstr(plain, needle) != NULL;
        free(plain);
        if (hit) { return (int)i; }
    }
    return -1;
}


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

    /* ── read_only / not_selectable fields ── */
    {
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        int ro = sc_form_add_text(f, "Repeat", "weekly",
            (ScFieldOpts){ .read_only = true });
        int ns = sc_form_add_text(f, "Hidden", "x",
            (ScFieldOpts){ .read_only = true, .not_selectable = true });
        int ed = sc_form_add_text(f, "Title", "t", (ScFieldOpts){ 0 });
        CHECK(ro == 0 && ns == 1 && ed == 2,
              "read_only/not_selectable fields are added like any other");

        /* The dimmed render path (disabled title/value) renders without a TTY. */
        ScRendered *fr = sc_form_frame(f);
        CHECK(fr != NULL && fr->line_count > 0,
              "form with read_only + not_selectable fields renders");
        CHECK(form_line_of(fr, "Repeat") >= 0 && form_line_of(fr, "Hidden") >= 0,
              "disabled fields are still drawn (labels visible)");
        sc_rendered_free(fr);

        /* Getters still read their (display-only) values. */
        CHECK(strcmp(sc_form_get_string(f, ro), "weekly") == 0,
              "read_only field exposes its value via the getter");
        sc_form_free(f);
    }

    /* A form whose every field is not_selectable still renders (no crash). */
    {
        ScForm *f = sc_form_new((ScFormOpts){ 0 });
        sc_form_row_begin(f);
        sc_form_add_text(f, "A", "a", (ScFieldOpts){ .not_selectable = true });
        sc_form_add_text(f, "B", "b", (ScFieldOpts){ .not_selectable = true });
        ScRendered *fr = sc_form_frame(f);
        CHECK(fr != NULL && fr->line_count > 0,
              "all-non-selectable form renders without hanging");
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

    /* ── Fullscreen CONTENT valign scope ── */
    {
        int th = sc_term_height();
        ScRendered *hdr = sc_capture_str("HDRMARK");
        ScVAlign vs[3] = { SC_VALIGN_TOP, SC_VALIGN_MIDDLE, SC_VALIGN_BOTTOM };
        int grid_at[3];

        for (int k = 0; k < 3; k++) {
            ScForm *f = sc_form_new((ScFormOpts){
                .fullscreen = true, .valign = vs[k],
                .valign_scope = SC_VALIGN_SCOPE_CONTENT,
                .header = hdr, .hint = "FOOTMARK" });
            sc_form_row_begin(f);
            sc_form_add_text(f, "BodyField", "x",
                (ScFieldOpts){ .width_mode = SC_FWIDTH_PCT, .width = 100 });
            ScRendered *fr = sc_form_frame(f);
            CHECK(fr && (int)fr->line_count == th,
                  "content scope fills the full terminal height exactly");
            CHECK(form_line_of(fr, "HDRMARK") == 0,
                  "content scope pins the header to line 0");
            CHECK(fr && form_line_of(fr, "FOOTMARK")
                       == (int)fr->line_count - 1,
                  "content scope pins the footer to the last line");
            grid_at[k] = form_line_of(fr, "BodyField");
            sc_rendered_free(fr);
            sc_form_free(f);
        }
        CHECK(grid_at[0] == 1,
              "TOP places the grid right below the 1-line header");
        CHECK(grid_at[0] < grid_at[1] && grid_at[1] < grid_at[2],
              "content scope shifts the grid down TOP < MIDDLE < BOTTOM");

        /* ALL scope (default) centers the whole block instead: with MIDDLE the
           header is pushed off line 0 by the top pad (unchanged behavior). */
        ScForm *fa = sc_form_new((ScFormOpts){
            .fullscreen = true, .valign = SC_VALIGN_MIDDLE,
            .header = hdr, .hint = "FOOTMARK" });   /* scope = ALL (zero-init) */
        sc_form_row_begin(fa);
        sc_form_add_text(fa, "BodyField", "x", (ScFieldOpts){ 0 });
        ScRendered *fra = sc_form_frame(fa);
        CHECK(form_line_of(fra, "HDRMARK") > 0,
              "ALL scope MIDDLE does not pin the header to line 0");
        sc_rendered_free(fra);
        sc_form_free(fa);

        /* NULL header in content scope is safe: footer still bottom-pinned. */
        ScForm *fn = sc_form_new((ScFormOpts){
            .fullscreen = true, .valign = SC_VALIGN_MIDDLE,
            .valign_scope = SC_VALIGN_SCOPE_CONTENT, .hint = "FOOTMARK" });
        sc_form_row_begin(fn);
        sc_form_add_text(fn, "BodyField", "x", (ScFieldOpts){ 0 });
        ScRendered *frn = sc_form_frame(fn);
        CHECK(frn && (int)frn->line_count == th,
              "content scope with NULL header still fills the screen");
        CHECK(frn && form_line_of(frn, "FOOTMARK") == (int)frn->line_count - 1,
              "content scope NULL header keeps the footer bottom-pinned");
        sc_rendered_free(frn);
        sc_form_free(fn);

        sc_rendered_free(hdr);
    }

    /* ── External editor temp-file suffix ── */
    {
        /* With a suffix the temp path ends with it; the round-trip works and the
           file is unlinked afterwards. */
        char *body = NULL;
        char *path = run_editor_capture(".md", &body);
        CHECK(path != NULL, "editor ran with a .md suffix");
        if (path) {
            size_t pl = strlen(path);
            CHECK(pl >= 3 && strcmp(path + pl - 3, ".md") == 0,
                  "temp file path ends with the configured suffix");
            CHECK(strstr(path, "sparcli-edit-") != NULL,
                  "temp file keeps the sparcli-edit- prefix");
            CHECK(access(path, F_OK) != 0,
                  "temp file is unlinked after the editor returns");
            free(path);
        }
        CHECK(body && strcmp(body, "edited-body") == 0,
              "editor round-trip returns the rewritten content");
        free(body);

        /* Without a suffix the name is sparcli-edit-XXXXXX with no extension. */
        body = NULL;
        path = run_editor_capture(NULL, &body);
        CHECK(path != NULL, "editor ran with no suffix");
        if (path) {
            const char *slash = strrchr(path, '/');
            const char *base = slash ? slash + 1 : path;
            CHECK(strncmp(base, "sparcli-edit-", 13) == 0
                  && strlen(base) == 13 + 6
                  && strchr(base, '.') == NULL,
                  "no-suffix temp file is sparcli-edit-XXXXXX (no dot)");
            free(path);
        }
        free(body);
    }

    /* NULL-safety. */
    sc_form_free(NULL);
    CHECK(sc_form_get_string(NULL, 0) == NULL, "get_string(NULL) is safe");
    CHECK(sc_form_run(NULL) == SC_INPUT_ERROR, "run(NULL) returns error");
}
