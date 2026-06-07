#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>


/*
 * Grid-layout form widget.
 *
 * The whole form is ONE interactive session (a single ScPromptVTable over
 * sc_prompt_run), modally split into navigation and editing. The fields are
 * laid out like a table - rows of cells with col/row spans and skip
 * placeholders - and rendered as a 2D mosaic of bordered panels. Editing of
 * the active field happens in a region BELOW the grid; the box keeps showing
 * the committed value until the edit is saved.
 *
 * See include/input/sparcli_form.h for the public model and key bindings.
 */


/** Minimum outer width of a grid column (border + one content column). */
#define FORM_MIN_COL 6

/** A single form field. */
typedef struct Field {
    ScFieldType type;
    char       *label;       /**< Owned. */
    char       *text;        /**< Owned text/number display value. */
    double      num;         /**< Numeric value (number fields). */
    bool        bval;        /**< Boolean value (bool fields). */

    /* Select / multiselect. */
    char      **choices;     /**< Owned array of owned strings. */
    size_t      n_choices;
    size_t      sel;         /**< Selected index (single-select). */
    bool       *checked;     /**< Parallel to choices (multiselect). */

    /* Date. */
    struct tm   date;        /**< Picked date (date fields). */
    bool        date_set;    /**< Date fields: false = empty (date_optional). */

    ScFieldOpts opts;        /**< Copied (help/label are owned separately). */

    /* Grid placement (resolved at finalize). */
    int row, col;
    int col_span, row_span;
} Field;

struct ScForm {
    Field      *fields;
    size_t      count, cap;

    /* Construction cursor. */
    int         cur_row;     /**< -1 before the first row. */
    int         cur_col;     /**< Next free column in the current row. */

    /* Resolved grid (finalize). */
    bool        finalized;
    int         rows, cols;
    int        *grid;        /**< rows*cols, field index or -1. */

    ScFormOpts  opts;
    char       *title;       /**< Owned. */
    char       *hint;        /**< Owned. */
    char       *editor;      /**< Owned external-editor command, or NULL. */
    ScColor     accent;
    ScColor     edit_bg;     /**< Background of the below-grid editor box. */

    /* Run state. */
    int          active;
    int          goal_col;   /**< Preferred column kept across vertical moves. */
    int          goal_row;   /**< Preferred row kept across horizontal moves. */
    bool         editing;
    ScLineEditor ed;
    bool         ed_active;
    const char  *edit_err;   /**< Borrowed validation message. */

    /* Select/multiselect edit sub-state. */
    size_t       list_cursor;
    size_t       list_top;
    bool        *list_checked;   /**< Working copy during a multiselect edit. */

    /* Date edit sub-state. */
    struct tm    date_edit;      /**< Working copy during a date edit. */

    const char  *form_err;       /**< Required-field message (nav mode). */
};

/** Rows of the choice list shown at once while editing a select field. */
#define FORM_LIST_VISIBLE 6


/* ── Date helpers (self-contained; mirror datepicker.c's math) ───────────── */

static bool date_is_leap(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

static int date_days_in_month(int year, int month0) {
    static const int DAYS[] = {
        31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31,
    };
    if (month0 == 1 && date_is_leap(year)) { return 29; }
    return DAYS[month0];
}

/** Weekday (0 = Sunday) of the first day of `cur`'s month. */
static int date_first_weekday(struct tm cur) {
    cur.tm_mday = 1;
    cur.tm_hour = 12;
    mktime(&cur);
    return cur.tm_wday;
}

/** Returns `seed` normalized to noon; a zeroed seed becomes today. */
static struct tm date_seed(struct tm seed) {
    bool empty = (seed.tm_year == 0 && seed.tm_mon == 0 && seed.tm_mday == 0);
    if (empty) {
        time_t now = time(NULL);
        struct tm *lt = localtime(&now);
        if (lt) { seed = *lt; }
    }
    seed.tm_hour = 12;   // avoid DST edge effects
    mktime(&seed);
    return seed;
}

static void date_shift_day(struct tm *cur, int delta) {
    cur->tm_mday += delta;
    cur->tm_hour = 12;
    mktime(cur);
}

static void date_shift_month(struct tm *cur, int delta) {
    int month = cur->tm_mon + delta;
    int year = cur->tm_year + 1900;
    while (month < 0)  { month += 12; year--; }
    while (month > 11) { month -= 12; year++; }
    int last = date_days_in_month(year, month);
    if (cur->tm_mday > last) { cur->tm_mday = last; }
    cur->tm_mon = month;
    cur->tm_year = year - 1900;
    cur->tm_hour = 12;
    mktime(cur);
}

static void date_shift_year(struct tm *cur, int delta) {
    int year = cur->tm_year + 1900 + delta;
    int last = date_days_in_month(year, cur->tm_mon);
    if (cur->tm_mday > last) { cur->tm_mday = last; }
    cur->tm_year = year - 1900;
    cur->tm_hour = 12;
    mktime(cur);
}

static const char *const DEFAULT_HINT =
    "\xe2\x86\x91\xe2\x86\x93\xe2\x86\x90\xe2\x86\x92 move \xc2\xb7 "
    "enter edit \xc2\xb7 ctrl-d submit \xc2\xb7 esc cancel";


static void form_finalize(ScForm *self);
static ScRendered *form_render(void *state);
    static void solve_columns(const ScForm *self, int term_w, int *colw,
                              int *colx, int *total_w);
    static void solve_rows(const ScForm *self, int *rowh, int *rowy,
                           int *total_lines);
    static int  field_content_lines(const Field *f);
    static ScRendered *capture_field(const ScForm *self, const Field *f,
                                     int fieldw, int fieldh, bool active);
        static char *value_display(const Field *f, bool *is_placeholder);
    static ScRendered *compose_grid(const ScForm *self,
                                    ScRendered *const *panels,
                                    const int *colx, const int *colw,
                                    const int *rowy, const int *rowh,
                                    int total_w, int total_lines);
    static ScRendered *build_edit_region(ScForm *self, int box_w);
static void form_on_key(void *state, ScKey key, bool *done, bool *cancel);
    static int  neighbor(const ScForm *self, int dir);
    static void begin_edit(ScForm *self);
    static void commit_edit(ScForm *self);
static int  add_field(ScForm *self, ScFieldType type, const char *label,
                      const char *initial, ScFieldOpts opts);


/* ── Construction ────────────────────────────────────────────────────────── */

ScForm *sc_form_new(ScFormOpts opts) {
    sc_theme_apply_form(&opts);
    ScForm *self = calloc(1, sizeof *self);
    if (!self) {
        return NULL;
    }
    self->opts = opts;
    self->title = sc_dup_opt_str(opts.title);
    self->hint = sc_dup_opt_str(opts.hint);
    self->editor = sc_dup_opt_str(opts.editor);
    self->accent = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN;
    self->edit_bg = opts.edit_bg.index ? opts.edit_bg
                                       : sc_color_from_rgb(43, 43, 43);  // gray
    self->cur_row = -1;
    self->active = 0;
    return self;
}

void sc_form_row_begin(ScForm *self) {
    if (!self) {
        return;
    }
    self->cur_row++;
    self->cur_col = 0;
}

/** Shared field constructor; returns the new field index or -1. */
static int add_field(ScForm *self, ScFieldType type, const char *label,
                     const char *initial, ScFieldOpts opts) {
    if (!self) {
        return -1;
    }
    if (self->cur_row < 0) {
        sc_form_row_begin(self);   // implicit first row
    }
    if (self->count == self->cap) {
        size_t cap = self->cap ? self->cap * 2 : 8;
        Field *grown = realloc(self->fields, cap * sizeof *grown);
        if (!grown) {
            return -1;
        }
        self->fields = grown;
        self->cap = cap;
    }
    Field *f = &self->fields[self->count];
    memset(f, 0, sizeof *f);
    f->type = type;
    f->label = sc_dup_str(label);
    f->text = sc_dup_str(initial);
    f->opts = opts;
    f->opts.help = sc_dup_opt_str(opts.help);   // own the help string
    f->col_span = opts.col_span > 0 ? opts.col_span : 1;
    f->row_span = opts.row_span > 0 ? opts.row_span : 1;
    f->row = self->cur_row;
    f->col = self->cur_col;
    self->cur_col += f->col_span;
    self->finalized = false;
    return (int)self->count++;
}

int sc_form_add_text(ScForm *self, const char *label, const char *initial,
                     ScFieldOpts opts) {
    return add_field(self, SC_FIELD_TEXT, label, initial ? initial : "", opts);
}

int sc_form_add_number(ScForm *self, const char *label, double initial,
                       ScFieldOpts opts) {
    char buf[64];
    snprintf(buf, sizeof buf, "%g", initial);
    int idx = add_field(self, SC_FIELD_NUMBER, label, buf, opts);
    if (idx >= 0) {
        self->fields[idx].num = initial;
    }
    return idx;
}

int sc_form_add_bool(ScForm *self, const char *label, bool initial,
                     ScFieldOpts opts) {
    int idx = add_field(self, SC_FIELD_BOOL, label, "", opts);
    if (idx >= 0) {
        self->fields[idx].bval = initial;
    }
    return idx;
}

/** Copies `n` choice strings into a fresh array; returns NULL on OOM/empty. */
static char **dup_choices(const char *const *choices, size_t n) {
    if (n == 0) { return NULL; }
    char **out = calloc(n, sizeof *out);
    if (!out) { return NULL; }
    for (size_t i = 0; i < n; i++) {
        out[i] = sc_dup_str(choices && choices[i] ? choices[i] : "");
    }
    return out;
}

int sc_form_add_select(ScForm *self, const char *label,
                       const char *const *choices, size_t n, size_t initial,
                       ScFieldOpts opts) {
    int idx = add_field(self, SC_FIELD_SELECT, label, "", opts);
    if (idx < 0) { return -1; }
    Field *f = &self->fields[idx];
    f->choices = dup_choices(choices, n);
    f->n_choices = f->choices ? n : 0;
    f->sel = (f->n_choices && initial < f->n_choices) ? initial : 0;
    return idx;
}

int sc_form_add_multiselect(ScForm *self, const char *label,
                            const char *const *choices, size_t n,
                            const size_t *checked_indices, size_t n_checked,
                            ScFieldOpts opts) {
    int idx = add_field(self, SC_FIELD_MULTISELECT, label, "", opts);
    if (idx < 0) { return -1; }
    Field *f = &self->fields[idx];
    f->choices = dup_choices(choices, n);
    f->n_choices = f->choices ? n : 0;
    f->checked = f->n_choices ? calloc(f->n_choices, sizeof(bool)) : NULL;
    if (f->checked && checked_indices) {
        for (size_t i = 0; i < n_checked; i++) {
            if (checked_indices[i] < f->n_choices) {
                f->checked[checked_indices[i]] = true;
            }
        }
    }
    return idx;
}

int sc_form_add_date(ScForm *self, const char *label, struct tm initial,
                     ScFieldOpts opts) {
    int idx = add_field(self, SC_FIELD_DATE, label, "", opts);
    if (idx >= 0) {
        bool empty = opts.date_optional
            && initial.tm_year == 0 && initial.tm_mon == 0
            && initial.tm_mday == 0;
        /* Seed `date` to today either way (the calendar view when editing an
           empty field); `date_set` records whether a date is actually present. */
        self->fields[idx].date = date_seed(initial);
        self->fields[idx].date_set = !empty;
    }
    return idx;
}

void sc_form_add_skip(ScForm *self) {
    if (self && self->cur_row >= 0) {
        self->cur_col++;
    }
}

void sc_form_free(ScForm *self) {
    if (!self) {
        return;
    }
    if (self->ed_active) {
        sc_le_free(&self->ed);
    }
    free(self->list_checked);
    for (size_t i = 0; i < self->count; i++) {
        Field *f = &self->fields[i];
        free(f->label);
        free(f->text);
        free((char *)f->opts.help);
        for (size_t c = 0; c < f->n_choices; c++) { free(f->choices[c]); }
        free(f->choices);
        free(f->checked);
    }
    free(self->fields);
    free(self->grid);
    free(self->title);
    free(self->hint);
    free(self->editor);
    free(self);
}


/* ── Grid finalize ───────────────────────────────────────────────────────── */

/** Resolves the column count, row count and the cell→field map. */
static void form_finalize(ScForm *self) {
    if (self->finalized) {
        return;
    }
    int cols = 1, rows = 1;
    for (size_t i = 0; i < self->count; i++) {
        const Field *f = &self->fields[i];
        if (f->col + f->col_span > cols) { cols = f->col + f->col_span; }
        if (f->row + f->row_span > rows) { rows = f->row + f->row_span; }
    }
    if (self->cur_row + 1 > rows) { rows = self->cur_row + 1; }

    free(self->grid);
    self->grid = malloc((size_t)rows * (size_t)cols * sizeof(int));
    if (self->grid) {
        for (int i = 0; i < rows * cols; i++) { self->grid[i] = -1; }
        for (size_t i = 0; i < self->count; i++) {
            const Field *f = &self->fields[i];
            for (int r = f->row; r < f->row + f->row_span && r < rows; r++) {
                for (int c = f->col; c < f->col + f->col_span && c < cols; c++) {
                    self->grid[r * cols + c] = (int)i;
                }
            }
        }
    }
    self->rows = rows;
    self->cols = cols;
    self->finalized = true;
}


/* ── Layout solvers ──────────────────────────────────────────────────────── */

/**
 * Resolves per-column outer widths. A column's sizing comes from the
 * single-column (`col_span == 1`) fields anchored in it (last one wins);
 * FIXED/PCT columns take their share first, AUTO columns split the remainder.
 */
static void solve_columns(const ScForm *self, int term_w, int *colw,
                          int *colx, int *total_w) {
    int cols = self->cols;
    ScFieldWidthMode mode[64];
    int spec[64];
    for (int c = 0; c < cols && c < 64; c++) {
        mode[c] = SC_FWIDTH_AUTO;
        spec[c] = 0;
    }
    for (size_t i = 0; i < self->count; i++) {
        const Field *f = &self->fields[i];
        if (f->col_span == 1 && f->col < 64) {
            mode[f->col] = f->opts.width_mode;
            spec[f->col] = f->opts.width;
        }
    }

    int used = 0, autos = 0;
    for (int c = 0; c < cols; c++) {
        ScFieldWidthMode m = c < 64 ? mode[c] : SC_FWIDTH_AUTO;
        int s = c < 64 ? spec[c] : 0;
        if (m == SC_FWIDTH_FIXED) {
            colw[c] = s > FORM_MIN_COL ? s : FORM_MIN_COL;
            used += colw[c];
        } else if (m == SC_FWIDTH_PCT) {
            int w = term_w * (s > 0 ? s : 0) / 100;
            colw[c] = w > FORM_MIN_COL ? w : FORM_MIN_COL;
            used += colw[c];
        } else {
            colw[c] = 0;
            autos++;
        }
    }
    int rem = term_w - used;
    if (rem < 0) { rem = 0; }
    int each = autos ? rem / autos : 0;
    int extra = autos ? rem % autos : 0;
    for (int c = 0; c < cols; c++) {
        ScFieldWidthMode m = c < 64 ? mode[c] : SC_FWIDTH_AUTO;
        if (m != SC_FWIDTH_FIXED && m != SC_FWIDTH_PCT) {
            int w = each + (extra > 0 ? 1 : 0);
            if (extra > 0) { extra--; }
            colw[c] = w > FORM_MIN_COL ? w : FORM_MIN_COL;
        }
    }
    int x = 0;
    for (int c = 0; c < cols; c++) {
        colx[c] = x;
        x += colw[c];
    }
    *total_w = x;
}

/** Resolves per-row outer heights (content lines + 2 border lines). */
static void solve_rows(const ScForm *self, int *rowh, int *rowy,
                       int *total_lines) {
    int rows = self->rows;
    for (int r = 0; r < rows; r++) { rowh[r] = 0; }
    for (size_t i = 0; i < self->count; i++) {
        const Field *f = &self->fields[i];
        if (f->row_span == 1) {
            int cl = field_content_lines(f);
            if (cl > rowh[f->row]) { rowh[f->row] = cl; }
        }
    }
    int y = 0;
    for (int r = 0; r < rows; r++) {
        if (rowh[r] == 0) { rowh[r] = 1; }
        rowh[r] += 2;   // top + bottom border
        rowy[r] = y;
        y += rowh[r];
    }
    *total_lines = y;
}

static int field_content_lines(const Field *f) {
    return f->opts.height > 0 ? f->opts.height : 1;
}


/* ── Render ──────────────────────────────────────────────────────────────── */

static ScRendered *form_render(void *state) {
    ScForm *self = state;
    form_finalize(self);
    if (self->cols < 1 || self->rows < 1 || !self->grid) {
        return sc_capture_str("");
    }

    int term_w = sc_terminal_width();
    int colw[64], colx[64], rowh[256], rowy[256];
    int total_w = 0, total_lines = 0;
    int cols = self->cols < 64 ? self->cols : 64;
    int rows = self->rows < 256 ? self->rows : 256;
    ScForm clamped = *self;
    clamped.cols = cols;
    clamped.rows = rows;
    solve_columns(&clamped, term_w, colw, colx, &total_w);
    solve_rows(&clamped, rowh, rowy, &total_lines);

    /* Capture every field as a fixed-size panel. */
    ScRendered **panels = calloc(self->count, sizeof *panels);
    if (!panels) {
        return sc_capture_str("");
    }
    for (size_t i = 0; i < self->count; i++) {
        const Field *f = &self->fields[i];
        if (f->col >= cols || f->row >= rows) { continue; }
        int cspan_end = f->col + f->col_span;
        if (cspan_end > cols) { cspan_end = cols; }
        int rspan_end = f->row + f->row_span;
        if (rspan_end > rows) { rspan_end = rows; }
        int fieldw = 0;
        for (int c = f->col; c < cspan_end; c++) { fieldw += colw[c]; }
        int fieldh = 0;
        for (int r = f->row; r < rspan_end; r++) { fieldh += rowh[r]; }
        panels[i] = capture_field(self, f, fieldw, fieldh,
                                  (int)i == self->active);
    }

    ScRendered *grid = compose_grid(self, panels, colx, colw, rowy, rowh,
                                    total_w, total_lines);
    for (size_t i = 0; i < self->count; i++) { sc_rendered_free(panels[i]); }
    free(panels);

    /* Stack: [title] + grid + edit-region. */
    ScRendered *edit = build_edit_region(self, total_w);
    ScRendered *title = NULL;
    if (self->title && self->title[0]) {
        ScTextStyle ts = sc_style_set(self->opts.title_style)
            ? self->opts.title_style
            : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                             SC_ANSI_COLOR_NONE };
        ScText *tt = sc_text_new();
        sc_text_append(tt, self->title, ts);
        title = sc_capture_text(tt);
        sc_text_free(tt);
    }

    const ScRendered *parts[3];
    size_t n = 0;
    if (title) { parts[n++] = title; }
    if (grid)  { parts[n++] = grid; }
    if (edit)  { parts[n++] = edit; }
    ScRendered *body = n ? sc_vstack(parts, n, 1) : sc_capture_str("");
    sc_rendered_free(title);
    sc_rendered_free(grid);
    sc_rendered_free(edit);

    ScRendered *frame =
        sc_compose_hint(body, self->hint ? self->hint : DEFAULT_HINT,
                        self->opts.hint_layout, self->opts.hint_pos,
                        self->opts.hint_style, 0);   /* form has no box */
    if (self->opts.fullscreen) {
        frame = sc_fullscreen_compose(frame, self->opts.header,
                                      self->opts.valign);
    }
    return frame;
}

/** Builds the value string shown inside a field box. */
static char *value_display(const Field *f, bool *is_placeholder) {
    *is_placeholder = false;
    if (f->type == SC_FIELD_BOOL) {
        return sc_dup_str(f->bval ? "[x] yes" : "[ ] no");
    }
    if (f->type == SC_FIELD_SELECT) {
        if (f->n_choices == 0) { *is_placeholder = true; return sc_dup_str("\xe2\x80\x94"); }
        return sc_dup_str(f->choices[f->sel]);
    }
    if (f->type == SC_FIELD_MULTISELECT) {
        size_t total = 1;
        size_t k = 0;
        for (size_t i = 0; i < f->n_choices; i++) {
            if (f->checked[i]) { total += strlen(f->choices[i]) + 2; k++; }
        }
        if (k == 0) { *is_placeholder = true; return sc_dup_str("\xe2\x80\x94"); }
        char *out = malloc(total);
        if (!out) { return NULL; }
        out[0] = '\0';
        bool first = true;
        for (size_t i = 0; i < f->n_choices; i++) {
            if (!f->checked[i]) { continue; }
            if (!first) { strcat(out, ", "); }
            strcat(out, f->choices[i]);
            first = false;
        }
        return out;
    }
    if (f->type == SC_FIELD_DATE) {
        if (!f->date_set) {
            *is_placeholder = true;
            return sc_dup_str("\xe2\x80\x94");   // em dash
        }
        char buf[32];
        struct tm tmp = f->date;
        strftime(buf, sizeof buf, "%Y-%m-%d", &tmp);
        return sc_dup_str(buf);
    }
    if (!f->text || !f->text[0]) {
        *is_placeholder = true;
        return sc_dup_str("\xe2\x80\x94");   // em dash
    }
    return sc_dup_str(f->text);
}

/** Captures one field as a panel of exactly `fieldw` x `fieldh`. */
static ScRendered *capture_field(const ScForm *self, const Field *f,
                                 int fieldw, int fieldh, bool active) {
    int inner = fieldw - 2;
    if (inner < 1) { inner = 1; }
    int content_lines = fieldh - 2;
    if (content_lines < 1) { content_lines = 1; }

    bool placeholder = false;
    char *val = value_display(f, &placeholder);

    ScText *content = sc_text_new();
    ScTextStyle vstyle = placeholder
        ? (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE }
        : (ScTextStyle){ 0 };

    /* Lay the value across content lines, breaking on '\n' (multiline fields);
       single-line values keep their old one-line look. Extra lines are
       dropped; remaining lines are padded blank so the panel fills its box. */
    const char *p = val ? val : "";
    int line = 0;
    while (line < content_lines) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char *seg = strndup(p, len);
        char *shown = sc_truncate(seg ? seg : "", inner, "\xe2\x80\xa6");
        free(seg);
        if (line > 0) { sc_text_append(content, "\n", (ScTextStyle){ 0 }); }
        sc_text_append(content, shown ? shown : "", vstyle);
        free(shown);
        line++;
        if (!nl) { break; }
        p = nl + 1;
    }
    for (; line < content_lines; line++) {
        sc_text_append(content, "\n", (ScTextStyle){ 0 });
    }
    free(val);

    ScBorderType bt = f->opts.border.type ? f->opts.border.type
                                          : SC_BORDER_ROUNDED;
    ScColor border_col = active ? self->accent : f->opts.border.color;
    ScTextStyle title_style = active
        ? (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE }
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };

    ScPanelOpts po = {
        .border = { .type = bt, .color = border_col,
                    .bg = f->opts.border.bg },
        .title = { .text = f->label, .halign = SC_ALIGN_LEFT, .pad = 1,
                   .style = title_style },
        .width = fieldw,
        .content_align = SC_ALIGN_LEFT,
    };
    ScRendered *r = sc_capture_panel_text(content, po);
    sc_text_free(content);
    return r;
}

/** Appends `n` spaces to the open memstream `out`. */
static void put_spaces(FILE *out, int n) {
    for (int i = 0; i < n; i++) { fputc(' ', out); }
}

/** Blits every field panel into a `total_lines` x `total_w` line mosaic. */
static ScRendered *compose_grid(const ScForm *self,
                                ScRendered *const *panels,
                                const int *colx, const int *colw,
                                const int *rowy, const int *rowh,
                                int total_w, int total_lines) {
    (void)colw; (void)rowh;
    ScRendered *out = malloc(sizeof *out);
    if (!out) { return NULL; }
    out->lines = calloc((size_t)total_lines, sizeof(char *));
    out->column_widths = calloc((size_t)total_lines, sizeof(int));
    out->line_count = (size_t)total_lines;
    out->max_column_width = total_w;
    if (!out->lines || !out->column_widths) {
        sc_rendered_free(out);
        return NULL;
    }

    for (int y = 0; y < total_lines; y++) {
        char *buf = NULL;
        size_t size = 0;
        FILE *mem = open_memstream(&buf, &size);
        if (!mem) { out->lines[y] = strdup(""); continue; }

        int cur_x = 0;
        /* Walk columns left to right; emit each field once at its anchor x. */
        for (int c = 0; c < self->cols && c < 64; c++) {
            int idx = -1;
            /* Find which field, if any, anchors a segment covering (y, col c)
               at its left edge. */
            for (size_t i = 0; i < self->count; i++) {
                const Field *f = &self->fields[i];
                if (f->col != c) { continue; }
                int y0 = rowy[f->row];
                int fieldh_lines = 0;
                int rspan_end = f->row + f->row_span;
                if (rspan_end > self->rows) { rspan_end = self->rows; }
                for (int r = f->row; r < rspan_end && r < 256; r++) {
                    fieldh_lines += rowh[r];
                }
                if (y >= y0 && y < y0 + fieldh_lines && panels[i]) {
                    idx = (int)i;
                    break;
                }
            }
            if (idx < 0) { continue; }
            const Field *f = &self->fields[idx];
            int x0 = colx[c];
            if (x0 > cur_x) { put_spaces(mem, x0 - cur_x); cur_x = x0; }
            int line = y - rowy[f->row];
            ScRendered *p = panels[idx];
            int fieldw = 0;
            int cspan_end = f->col + f->col_span;
            if (cspan_end > self->cols) { cspan_end = self->cols; }
            for (int cc = f->col; cc < cspan_end && cc < 64; cc++) {
                fieldw += colw[cc];
            }
            if (line >= 0 && (size_t)line < p->line_count) {
                fputs(p->lines[line], mem);
                int w = p->column_widths[line];
                if (w < fieldw) { put_spaces(mem, fieldw - w); }
            } else {
                put_spaces(mem, fieldw);
            }
            cur_x += fieldw;
        }
        if (cur_x < total_w) { put_spaces(mem, total_w - cur_x); }
        fclose(mem);
        out->lines[y] = buf ? buf : strdup("");
        out->column_widths[y] = total_w;
    }
    return out;
}

/** Appends the choice-list body (rows + scroll indicator), no label/hint. */
static void append_choice_body(ScForm *self, const Field *a, ScText *t) {
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                        SC_ANSI_COLOR_NONE };
    ScTextStyle cur_style = { SC_TEXT_ATTR_BOLD, self->accent,
                              SC_ANSI_COLOR_NONE };
    bool multi = (a->type == SC_FIELD_MULTISELECT);

    size_t n = a->n_choices;
    size_t top = self->list_top;
    size_t end = top + FORM_LIST_VISIBLE < n ? top + FORM_LIST_VISIBLE : n;
    for (size_t i = top; i < end; i++) {
        bool cur = (i == self->list_cursor);
        sc_text_append(t, cur ? "\xe2\x96\xb8 " : "  ",
                       cur ? cur_style : (ScTextStyle){ 0 });
        if (multi) {
            sc_text_append(t, self->list_checked[i] ? "[x] " : "[ ] ",
                           cur ? cur_style : (ScTextStyle){ 0 });
        }
        sc_text_append(t, a->choices[i], cur ? cur_style : (ScTextStyle){ 0 });
        if (i + 1 < end || n > FORM_LIST_VISIBLE) {
            sc_text_append(t, "\n", (ScTextStyle){ 0 });
        }
    }
    if (n > FORM_LIST_VISIBLE) {
        char ind[64];
        snprintf(ind, sizeof ind, "\xe2\x86\x91 %zu\xe2\x80\x93%zu/%zu \xe2\x86\x93",
                 top + 1, end, n);
        sc_text_append(t, ind, dim);
    }
}

/** Appends the month-grid body (Monday week start), no label/hint. */
static void append_month_body(ScForm *self, const Field *a, ScText *t) {
    (void)a;
    static const char *const MONTHS[] = {
        "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    static const char *const WDAYS[] = {
        "Mo", "Tu", "We", "Th", "Fr", "Sa", "Su"
    };
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                        SC_ANSI_COLOR_NONE };
    ScTextStyle hdr = { SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    ScTextStyle sel = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK, self->accent };

    struct tm cur = self->date_edit;
    int year = cur.tm_year + 1900;

    char header[96];
    snprintf(header, sizeof header, "\xe2\x80\xb9 %s %d \xe2\x80\xba",
             MONTHS[cur.tm_mon], year);
    sc_text_append(t, header, hdr);
    sc_text_append(t, "\n", (ScTextStyle){ 0 });

    for (int i = 0; i < 7; i++) {
        sc_text_append(t, WDAYS[i], dim);
        sc_text_append(t, i < 6 ? " " : "\n", (ScTextStyle){ 0 });
    }

    int month_days = date_days_in_month(year, cur.tm_mon);
    int leading = (date_first_weekday(cur) - 1 + 7) % 7;   // Monday start
    int col = 0;
    for (int i = 0; i < leading; i++) {
        sc_text_append(t, col < 6 ? "   " : "  ", (ScTextStyle){ 0 });
        col++;
    }
    for (int day = 1; day <= month_days; day++) {
        char cell[8];
        snprintf(cell, sizeof cell, "%2d", day);
        sc_text_append(t, cell,
                       day == cur.tm_mday ? sel : (ScTextStyle){ 0 });
        if (col < 6) { sc_text_append(t, " ", (ScTextStyle){ 0 }); }
        col++;
        if (col == 7 && day < month_days) {
            sc_text_append(t, "\n", (ScTextStyle){ 0 });
            col = 0;
        }
    }
}

/** Editor field width / box inner width for text & number inputs. */
#define FORM_EDIT_INPUT_W 42

/** Name of the configured external-editor key (default Ctrl-G). */
static void editor_key_name(const ScForm *self, char *buf, size_t cap) {
    ScKeyChord chord = self->opts.editor_key;
    if (chord.key == SC_KEY_NONE && chord.codepoint == 0) {
        chord = sc_key_ctrl('g');
    }
    sc_key_chord_name(chord, buf, cap);
}

/** Nav-mode help/error line (ungeboxed). */
static ScRendered *build_nav_region(ScForm *self, const Field *a) {
    ScText *t = sc_text_new();
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                        SC_ANSI_COLOR_NONE };
    ScTextStyle red = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,
                        SC_ANSI_COLOR_NONE };
    if (self->form_err) {
        sc_text_append(t, self->form_err, red);
        sc_text_append(t, "\n", (ScTextStyle){ 0 });
    }
    const char *help = a->opts.help;
    char ml_help[48];
    if (!help) {
        if (a->type == SC_FIELD_BOOL) {
            help = "space/enter toggle";
        } else if (a->type == SC_FIELD_TEXT && a->opts.multiline) {
            char key[16];
            editor_key_name(self, key, sizeof key);
            snprintf(ml_help, sizeof ml_help, "enter / %s open in editor", key);
            help = ml_help;
        } else {
            help = "enter to edit";
        }
    }
    sc_text_append(t, help, dim);
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

/**
 * Edit-mode region: the active field's editor body framed in an accent panel
 * (same color as the active cell), with the key hint stacked dim below it.
 */
static ScRendered *build_edit_box(ScForm *self, const Field *a, int box_w) {
    ScText *body = sc_text_new();
    const char *hint = "enter save \xc2\xb7 esc cancel";

    if (a->type == SC_FIELD_SELECT || a->type == SC_FIELD_MULTISELECT) {
        append_choice_body(self, a, body);
        hint = (a->type == SC_FIELD_MULTISELECT)
            ? "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 space toggle \xc2\xb7 "
              "enter confirm \xc2\xb7 esc cancel"
            : "\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 enter select \xc2\xb7 "
              "esc cancel";
    } else if (a->type == SC_FIELD_DATE) {
        append_month_body(self, a, body);
        hint = "\xe2\x86\x90/\xe2\x86\x92/\xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 "
               "pgup/pgdn month \xc2\xb7 enter select \xc2\xb7 esc cancel";
    } else {   /* text / number: line editor spans the full box width */
        int inner = box_w - 4;   /* - border - padding */
        if (inner < 8) { inner = 8; }
        sc_le_render_into(&self->ed, body, (ScTextStyle){ 0 },
                          (ScTextStyle){ 0 }, NULL, NULL,
                          (ScTextStyle){ 0 }, inner);
    }

    ScRendered *r_body = sc_capture_text(body);
    sc_text_free(body);

    ScPanelOpts po = {
        .border = { .type = SC_BORDER_ROUNDED, .color = self->accent,
                    .bg = self->edit_bg },
        .bg = self->edit_bg,
        .title = { .text = a->label, .halign = SC_ALIGN_LEFT, .pad = 1,
                   .style = { SC_TEXT_ATTR_BOLD, self->accent,
                              self->edit_bg } },
        .padding = { .left = 1, .right = 1 },
        .width = box_w,
    };
    ScRendered *framed = sc_capture_panel_rendered(r_body, po);
    if (!framed) { return r_body; }   /* fall back to the unframed body */
    sc_rendered_free(r_body);

    /* Separator rule above the editor box (matches the box width + accent). */
    ScRendered *r_rule = sc_capture_rule_str(NULL, (ScRuleOpts){
        .type = SC_BORDER_SINGLE, .color = self->edit_bg, .width = box_w });

    /* Hint (+ validation error for text/number) stacked dim below the box. */
    ScText *below = sc_text_new();
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                        SC_ANSI_COLOR_NONE };
    if (self->edit_err
        && (a->type == SC_FIELD_TEXT || a->type == SC_FIELD_NUMBER)) {
        sc_text_append(below, self->edit_err,
                       (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_RED,
                                      SC_ANSI_COLOR_NONE });
        sc_text_append(below, "\n", (ScTextStyle){ 0 });
    }
    sc_text_append(below, hint, dim);
    ScRendered *r_hint = sc_capture_text(below);
    sc_text_free(below);

    /* Box + hint hug each other (gap 0); the rule sits one line above. */
    const ScRendered *bh[2] = { framed, r_hint };
    ScRendered *box = sc_vstack(bh, 2, 0);
    sc_rendered_free(framed);
    sc_rendered_free(r_hint);

    if (r_rule && box) {
        const ScRendered *parts[2] = { r_rule, box };
        ScRendered *out = sc_vstack(parts, 2, 1);
        sc_rendered_free(r_rule);
        sc_rendered_free(box);
        return out;
    }
    sc_rendered_free(r_rule);
    return box;
}

/**
 * Builds the region below the grid (boxed editor or a nav help line). The boxed
 * editor spans `box_w` columns (the full grid width) so it lines up with it.
 */
static ScRendered *build_edit_region(ScForm *self, int box_w) {
    if (self->count == 0) { return NULL; }
    const Field *a = &self->fields[self->active];
    bool list_edit = self->editing
        && (a->type == SC_FIELD_SELECT || a->type == SC_FIELD_MULTISELECT
            || a->type == SC_FIELD_DATE);
    bool text_edit = self->editing && self->ed_active
        && (a->type == SC_FIELD_TEXT || a->type == SC_FIELD_NUMBER);
    if (list_edit || text_edit) {
        return build_edit_box(self, a, box_w);
    }
    return build_nav_region(self, a);
}


/* ── Key handling ────────────────────────────────────────────────────────── */

enum { DIR_LEFT, DIR_RIGHT, DIR_UP, DIR_DOWN };

/** Distance from a goal column to field `f`'s column range; 0 when it covers
 *  the goal. Used as the cross-axis anchor for vertical moves so the original
 *  column is preserved while stepping past wide col_span cells. */
static int col_score(const Field *f, int goal) {
    if (f->col <= goal && goal < f->col + f->col_span) { return 0; }
    return goal < f->col ? f->col - goal : goal - (f->col + f->col_span - 1);
}

/** Distance from a goal row to field `f`'s row range; 0 when it covers it.
 *  Cross-axis anchor for horizontal moves (preserves the row past row_span). */
static int row_score(const Field *f, int goal) {
    if (f->row <= goal && goal < f->row + f->row_span) { return 0; }
    return goal < f->row ? f->row - goal : goal - (f->row + f->row_span - 1);
}

/** Returns the field index reached by moving `dir` from the active field.
 *
 *  Edge-based spatial navigation over the cell grid with a remembered "goal"
 *  column (vertical moves) / row (horizontal moves): the primary axis steps to
 *  the nearest cell beyond the active field's edge, while the cross axis keeps
 *  the goal coordinate so e.g. descending column 2 past a wide col_span cell
 *  lands back in column 2 rather than snapping to the left. Phase 0 searches
 *  strictly in `dir`; if nothing qualifies and cycling is enabled (the default,
 *  unless `ScFormOpts.no_cycle`), phase 1 wraps to the opposite edge. Ranking by
 *  cell edges (not centers) keeps spans correct, and works even when a field's
 *  visible box width (ScFieldWidthMode) differs from its grid col_span. */
static int neighbor(const ScForm *self, int dir) {
    const Field *a = &self->fields[self->active];
    int a_r0 = a->row, a_r1 = a->row + a->row_span;   /* rows [r0, r1) */
    int a_c0 = a->col, a_c1 = a->col + a->col_span;   /* cols [c0, c1) */
    bool vert = (dir == DIR_UP || dir == DIR_DOWN);
    int best = self->active;
    int max_phase = self->opts.no_cycle ? 1 : 2;
    for (int phase = 0; phase < max_phase; phase++) {
        long best_score = -1;
        for (size_t i = 0; i < self->count; i++) {
            if ((int)i == self->active) { continue; }
            const Field *f = &self->fields[i];
            int f_r0 = f->row, f_r1 = f->row + f->row_span;
            int f_c0 = f->col, f_c1 = f->col + f->col_span;
            long dist;
            if (phase == 0) {     /* strictly beyond the active edge in `dir` */
                if (dir == DIR_DOWN)       { if (f_r0 < a_r1) { continue; }
                                             dist = f_r0 - a_r1; }
                else if (dir == DIR_UP)    { if (f_r1 > a_r0) { continue; }
                                             dist = a_r0 - f_r1; }
                else if (dir == DIR_RIGHT) { if (f_c0 < a_c1) { continue; }
                                             dist = f_c0 - a_c1; }
                else                       { if (f_c1 > a_c0) { continue; }
                                             dist = a_c0 - f_c1; }
            } else {              /* wrap: nearest field at the opposite edge */
                if (dir == DIR_DOWN)       { if (f_r0 >= a_r1) { continue; }
                                             dist = f_r0; }
                else if (dir == DIR_UP)    { if (f_r1 <= a_r0) { continue; }
                                             dist = self->rows - f_r1; }
                else if (dir == DIR_RIGHT) { if (f_c0 >= a_c1) { continue; }
                                             dist = f_c0; }
                else                       { if (f_c1 <= a_c0) { continue; }
                                             dist = self->cols - f_c1; }
            }
            long cross = vert ? col_score(f, self->goal_col)
                              : row_score(f, self->goal_row);
            long minor = vert ? f_c0 : f_r0;
            long score = dist * 10000L + cross * 100L + minor;
            if (best_score < 0 || score < best_score) {
                best_score = score; best = (int)i;
            }
        }
        if (best != self->active) { break; }   /* found one this phase */
    }
    return best;
}

/** Keeps the choice-list cursor inside the visible window. */
static void clamp_list(ScForm *self) {
    if (self->list_cursor < self->list_top) {
        self->list_top = self->list_cursor;
    } else if (self->list_cursor >= self->list_top + FORM_LIST_VISIBLE) {
        self->list_top = self->list_cursor - FORM_LIST_VISIBLE + 1;
    }
}

/** Moves the list cursor by `delta` with wrap-around. */
static void move_list(ScForm *self, int delta) {
    size_t n = self->fields[self->active].n_choices;
    if (n == 0) { return; }
    if (delta < 0 && self->list_cursor == 0) {
        self->list_cursor = n - 1;
    } else if (delta > 0 && self->list_cursor + 1 >= n) {
        self->list_cursor = 0;
    } else {
        self->list_cursor = (size_t)((long)self->list_cursor + delta);
    }
    clamp_list(self);
}

/** Stores the chosen item(s) and leaves the list editor. */
static void commit_list(ScForm *self) {
    Field *a = &self->fields[self->active];
    if (a->type == SC_FIELD_SELECT) {
        a->sel = self->list_cursor;
    } else if (self->list_checked && a->checked) {
        memcpy(a->checked, self->list_checked, a->n_choices * sizeof(bool));
    }
    free(self->list_checked);
    self->list_checked = NULL;
    self->editing = false;
}

/** Discards a list edit and leaves the editor. */
static void cancel_list(ScForm *self) {
    free(self->list_checked);
    self->list_checked = NULL;
    self->editing = false;
}

/** True when a required field has no value yet (blocks submit). */
static bool field_required_unmet(const Field *f) {
    if (!f->opts.required) { return false; }
    if (f->type == SC_FIELD_TEXT || f->type == SC_FIELD_NUMBER) {
        return !f->text || !f->text[0];
    }
    if (f->type == SC_FIELD_MULTISELECT) {
        for (size_t i = 0; i < f->n_choices; i++) {
            if (f->checked[i]) { return false; }
        }
        return true;
    }
    if (f->type == SC_FIELD_DATE) {
        return !f->date_set;   // optional date required but still empty
    }
    return false;   // bool / select always carry a value
}

/** Enters edit mode for the active field (or toggles a bool in place). */
static void begin_edit(ScForm *self) {
    Field *a = &self->fields[self->active];
    self->form_err = NULL;
    if (a->type == SC_FIELD_BOOL) {
        a->bval = !a->bval;   // bool toggles in place, no editor
        return;
    }
    if (a->type == SC_FIELD_SELECT || a->type == SC_FIELD_MULTISELECT) {
        if (a->n_choices == 0) { return; }
        self->list_cursor = (a->type == SC_FIELD_SELECT) ? a->sel : 0;
        self->list_top = 0;
        free(self->list_checked);
        self->list_checked = NULL;
        if (a->type == SC_FIELD_MULTISELECT) {
            self->list_checked = malloc(a->n_choices * sizeof(bool));
            if (self->list_checked) {
                memcpy(self->list_checked, a->checked,
                       a->n_choices * sizeof(bool));
            }
        }
        clamp_list(self);
        self->editing = true;
        return;
    }
    if (a->type == SC_FIELD_DATE) {
        self->date_edit = a->date;
        self->editing = true;
        return;
    }
    if (a->type == SC_FIELD_TEXT && a->opts.multiline) {
        return;   // edited only via the external editor (editor_key)
    }
    sc_le_init(&self->ed, a->text ? a->text : "");
    self->ed_active = true;
    self->editing = true;
    self->edit_err = NULL;
}

/** Validates and stores the edited value, leaving edit mode on success. */
static void commit_edit(ScForm *self) {
    Field *a = &self->fields[self->active];
    const char *buf = self->ed.buf ? self->ed.buf : "";

    if (a->opts.validate) {
        const char *err = NULL;
        if (!a->opts.validate(buf, a->opts.validate_ctx, &err)) {
            self->edit_err = err ? err : "invalid value";
            return;
        }
    }
    if (a->type == SC_FIELD_NUMBER) {
        char *endp = NULL;
        double v = strtod(buf, &endp);
        if (buf[0] != '\0' && endp == buf) {
            self->edit_err = "not a number";
            return;
        }
        a->num = v;
    }
    free(a->text);
    a->text = sc_dup_str(buf);
    sc_le_free(&self->ed);
    self->ed_active = false;
    self->editing = false;
    self->edit_err = NULL;
}

/** Focuses field `idx` and resets the navigation goal to its top-left cell.
 *  Used for non-directional jumps (Tab, required-field jump, edit-advance,
 *  initial focus); arrow moves preserve the goal instead. */
static void form_focus(ScForm *self, int idx) {
    self->active = idx;
    self->goal_col = self->fields[idx].col;
    self->goal_row = self->fields[idx].row;
}

/** Arrow-key move: steps to the neighbour in `dir` and updates the goal of the
 *  axis we travelled along while preserving the cross-axis goal (so vertical
 *  moves keep the column, horizontal moves keep the row). */
static void nav_move(ScForm *self, int dir) {
    int next = neighbor(self, dir);
    if (next == self->active) { return; }
    self->active = next;
    if (dir == DIR_UP || dir == DIR_DOWN) {
        self->goal_row = self->fields[next].row;   /* goal_col preserved */
    } else {
        self->goal_col = self->fields[next].col;   /* goal_row preserved */
    }
}

/** Commits the field currently being edited; returns true when edit mode was
 *  left (success). Text/number validation can fail and keep the editor open. */
static bool edit_commit_current(ScForm *self) {
    Field *a = &self->fields[self->active];
    if (a->type == SC_FIELD_SELECT || a->type == SC_FIELD_MULTISELECT) {
        commit_list(self);
        return true;
    }
    if (a->type == SC_FIELD_DATE) {
        a->date = self->date_edit;
        a->date_set = true;
        self->editing = false;
        return true;
    }
    commit_edit(self);          /* text/number; sets edit_err on failure */
    return !self->editing;      /* false → validation failed, stay open */
}

/** Tab/Shift-Tab while editing: commit the current field, move to the next
 *  (`step` ±1, wrapping), and start editing it. Bool/multiline fields are only
 *  focused (begin_edit would toggle a bool; multiline opens no inline editor),
 *  so the user lands on them in navigation mode. */
static void edit_advance(ScForm *self, int step) {
    if (!edit_commit_current(self)) { return; }   /* stay on validation error */
    form_focus(self,
        (self->active + step + (int)self->count) % (int)self->count);
    if (self->fields[self->active].type != SC_FIELD_BOOL) {
        begin_edit(self);   /* multiline: begin_edit returns early = focus only */
    }
}

static void form_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    ScForm *self = state;
    if (self->count == 0) { *cancel = true; return; }

    if (self->editing) {
        Field *a = &self->fields[self->active];
        if (a->type == SC_FIELD_DATE) {
            switch (key.type) {
                case SC_KEY_LEFT:  date_shift_day(&self->date_edit, -1); return;
                case SC_KEY_RIGHT: date_shift_day(&self->date_edit, +1); return;
                case SC_KEY_UP:    date_shift_day(&self->date_edit, -7); return;
                case SC_KEY_DOWN:  date_shift_day(&self->date_edit, +7); return;
                case SC_KEY_PAGEUP:   date_shift_month(&self->date_edit, -1);
                    return;
                case SC_KEY_PAGEDOWN: date_shift_month(&self->date_edit, +1);
                    return;
                case SC_KEY_SHIFT_PAGEUP:   date_shift_year(&self->date_edit, -1);
                    return;
                case SC_KEY_SHIFT_PAGEDOWN: date_shift_year(&self->date_edit, +1);
                    return;
                case SC_KEY_ENTER:
                    a->date = self->date_edit;
                    a->date_set = true;
                    self->editing = false;
                    return;
                case SC_KEY_DELETE:
                case SC_KEY_BACKSPACE:
                    if (a->opts.date_optional) {   /* clear to "no date" */
                        a->date_set = false;
                        self->editing = false;
                    }
                    return;
                case SC_KEY_TAB:     edit_advance(self, +1); return;
                case SC_KEY_BACKTAB: edit_advance(self, -1); return;
                case SC_KEY_ESC: self->editing = false; return;
                case SC_KEY_CHAR:
                    if (key.mods != 0) { return; }
                    if (key.bytes[0] == '<') {
                        date_shift_month(&self->date_edit, -1);
                    } else if (key.bytes[0] == '>') {
                        date_shift_month(&self->date_edit, +1);
                    }
                    return;
                default: return;
            }
        }
        if (a->type == SC_FIELD_SELECT || a->type == SC_FIELD_MULTISELECT) {
            switch (key.type) {
                case SC_KEY_UP:    move_list(self, -1); return;
                case SC_KEY_DOWN:  move_list(self, +1); return;
                case SC_KEY_ENTER: commit_list(self);   return;
                case SC_KEY_TAB:     edit_advance(self, +1); return;
                case SC_KEY_BACKTAB: edit_advance(self, -1); return;
                case SC_KEY_ESC:   cancel_list(self);   return;
                case SC_KEY_CHAR:
                    if (key.mods != 0) { return; }
                    if (key.bytes[0] == 'k') { move_list(self, -1); }
                    else if (key.bytes[0] == 'j') { move_list(self, +1); }
                    else if (key.bytes[0] == ' '
                             && a->type == SC_FIELD_MULTISELECT
                             && self->list_checked) {
                        self->list_checked[self->list_cursor] =
                            !self->list_checked[self->list_cursor];
                    }
                    return;
                default: return;
            }
        }
        /* Text / number editor. */
        if (sc_le_handle(&self->ed, key)) {
            self->edit_err = NULL;
            return;
        }
        switch (key.type) {
            case SC_KEY_ENTER: commit_edit(self); return;
            case SC_KEY_TAB:     edit_advance(self, +1); return;
            case SC_KEY_BACKTAB: edit_advance(self, -1); return;
            case SC_KEY_ESC:
                sc_le_free(&self->ed);
                self->ed_active = false;
                self->editing = false;
                self->edit_err = NULL;
                return;
            default: return;
        }
    }

    /* Navigation mode. */
    switch (key.type) {
        case SC_KEY_LEFT:  nav_move(self, DIR_LEFT);  return;
        case SC_KEY_RIGHT: nav_move(self, DIR_RIGHT); return;
        case SC_KEY_UP:    nav_move(self, DIR_UP);    return;
        case SC_KEY_DOWN:  nav_move(self, DIR_DOWN);  return;
        case SC_KEY_TAB:
            form_focus(self, (self->active + 1) % (int)self->count);
            return;
        case SC_KEY_BACKTAB:
            form_focus(self,
                (self->active - 1 + (int)self->count) % (int)self->count);
            return;
        case SC_KEY_ENTER:
            begin_edit(self);
            return;
        case SC_KEY_CTRL_D:
            for (size_t i = 0; i < self->count; i++) {
                if (field_required_unmet(&self->fields[i])) {
                    self->form_err = "please fill in the required fields";
                    form_focus(self, (int)i);
                    return;
                }
            }
            *done = true;
            return;
        case SC_KEY_ESC:
            *cancel = true;
            return;
        case SC_KEY_CHAR:
            if (key.mods == 0 && key.bytes[0] == ' '
                && self->fields[self->active].type == SC_FIELD_BOOL) {
                Field *a = &self->fields[self->active];
                a->bval = !a->bval;
            }
            return;
        default:
            return;
    }
}


/* ── External editor (multiline fields) ──────────────────────────────────── */

/** Returns true when the active field is an editor-backed multiline text. */
static bool active_is_multiline(const ScForm *self) {
    if (self->count == 0) { return false; }
    const Field *a = &self->fields[self->active];
    return a->type == SC_FIELD_TEXT && a->opts.multiline;
}

/** Engine hook: heap copy of the active multiline field's text, else NULL. */
static char *form_edit_get(void *state) {
    ScForm *self = state;
    if (!active_is_multiline(self)) { return NULL; }
    const Field *a = &self->fields[self->active];
    return sc_dup_str(a->text ? a->text : "");
}

/** Engine hook: stores the editor result into the active multiline field. */
static void form_edit_set(void *state, const char *text) {
    ScForm *self = state;
    if (!active_is_multiline(self)) { return; }
    Field *a = &self->fields[self->active];
    free(a->text);
    a->text = sc_dup_str(text ? text : "");
}

/** Engine hook: Enter also opens the editor on a multiline field (+ Ctrl-G). */
static bool form_wants_editor(void *state, ScKey key) {
    ScForm *self = state;
    return key.type == SC_KEY_ENTER && active_is_multiline(self);
}

/** True when any field uses the external editor. */
static bool form_has_multiline(const ScForm *self) {
    for (size_t i = 0; i < self->count; i++) {
        if (self->fields[i].type == SC_FIELD_TEXT
            && self->fields[i].opts.multiline) {
            return true;
        }
    }
    return false;
}


/* ── Run ─────────────────────────────────────────────────────────────────── */

ScInputStatus sc_form_run(ScForm *self) {
    if (!self || self->count == 0) {
        return SC_INPUT_ERROR;
    }
    form_finalize(self);
    form_focus(self, 0);
    self->editing = false;
    self->ed_active = false;
    self->edit_err = NULL;
    self->form_err = NULL;
    free(self->list_checked);
    self->list_checked = NULL;

    if (self->opts.autoedit
        && self->fields[self->active].type != SC_FIELD_BOOL) {
        begin_edit(self);   // open the initial field's editor (skip bool toggle)
    }

    bool has_editor = form_has_multiline(self);
    ScPromptVTable vtable = {
        .render = form_render,
        .on_key = form_on_key,
        .paste = SC_PASTE_TEXT,
        .capture_escape = true,   // Esc handled per mode in on_key
        .edit_get = form_edit_get,
        .edit_set = form_edit_set,
        .wants_editor = form_wants_editor,
    };
    ScPromptShortcuts sk = {
        self->opts.shortcuts, self->opts.n_shortcuts, self->opts.out_shortcut_id
    };
    ScPromptEditor editor = {
        .enabled = has_editor,
        .cmd = self->editor,
        .chord = self->opts.editor_key,
    };
    ScInputStatus status = sc_prompt_run(
        &vtable, self, self->opts.shortcuts ? &sk : NULL,
        has_editor ? &editor : NULL
    );

    if (self->ed_active) {
        sc_le_free(&self->ed);
        self->ed_active = false;
    }
    free(self->list_checked);
    self->list_checked = NULL;
    self->editing = false;
    if (status == SC_INPUT_OK && !self->opts.hide_summary) {
        char line[160];
        snprintf(line, sizeof line, "\xe2\x9c\x94 %s saved",
                 self->title && self->title[0] ? self->title : "Form");
        sc_println(line, self->opts.summary_style);
    }
    return status;
}

ScRendered *sc_form_frame(ScForm *self) {
    if (!self) { return NULL; }
    return form_render(self);
}

ScRendered *sc_form_frame_edit(ScForm *self, int field) {
    if (!self || field < 0 || (size_t)field >= self->count) { return NULL; }
    form_finalize(self);
    self->active = field;
    self->editing = false;
    begin_edit(self);   // open the editor for a snapshot of the edit state
    return form_render(self);
}


/* ── Getters ─────────────────────────────────────────────────────────────── */

const char *sc_form_get_string(const ScForm *self, int field) {
    if (!self || field < 0 || (size_t)field >= self->count) { return NULL; }
    return self->fields[field].text;
}

double sc_form_get_number(const ScForm *self, int field) {
    if (!self || field < 0 || (size_t)field >= self->count) { return 0.0; }
    const Field *f = &self->fields[field];
    return f->type == SC_FIELD_NUMBER ? f->num : 0.0;
}

bool sc_form_get_bool(const ScForm *self, int field) {
    if (!self || field < 0 || (size_t)field >= self->count) { return false; }
    const Field *f = &self->fields[field];
    return f->type == SC_FIELD_BOOL ? f->bval : false;
}

size_t sc_form_get_choice(const ScForm *self, int field) {
    if (!self || field < 0 || (size_t)field >= self->count) { return 0; }
    const Field *f = &self->fields[field];
    return f->type == SC_FIELD_SELECT ? f->sel : 0;
}

bool sc_form_get_date(const ScForm *self, int field, struct tm *out) {
    if (!self || !out || field < 0 || (size_t)field >= self->count) {
        return false;
    }
    const Field *f = &self->fields[field];
    if (f->type != SC_FIELD_DATE) { return false; }
    if (!f->date_set) { return false; }   // empty optional date
    struct tm tmp = f->date;
    tmp.tm_hour = 0;
    tmp.tm_min = 0;
    tmp.tm_sec = 0;
    mktime(&tmp);
    *out = tmp;
    return true;
}

size_t sc_form_get_checked(const ScForm *self, int field, size_t *out,
                           size_t cap) {
    if (!self || field < 0 || (size_t)field >= self->count) { return 0; }
    const Field *f = &self->fields[field];
    if (f->type != SC_FIELD_MULTISELECT) { return 0; }
    size_t total = 0;
    for (size_t i = 0; i < f->n_choices; i++) {
        if (f->checked[i]) {
            if (out && total < cap) { out[total] = i; }
            total++;
        }
    }
    return total;
}
