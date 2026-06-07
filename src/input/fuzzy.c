#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */
#include "core/text_internal.h"  /* sc_text_append_raw (trusted clone path) */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>   /* strcasecmp (ORDER_COLUMN) */


/**
 * One display entry of the filtered view: the add-order row `index`, its match
 * score, and whether the entry is a non-selectable section header. The display
 * list interleaves section headers with their matching rows; the cursor only
 * ever lands on a selectable (non-section, non-disabled) entry.
 */
typedef struct Match {
    size_t index;
    int score;
    bool is_section;
} Match;

/**
 * One stored row. `fields[c]` is always present (plain text used for matching
 * and as the display fallback). `styles`/`texts` are only allocated when the
 * styled / rich add API is used (NULL otherwise, so the common case carries no
 * overhead). A row may instead be a non-selectable section header or a disabled
 * (greyed) item.
 */
typedef struct Row {
    char       **fields;   /**< fields[c] = plain text of column c. */
    ScTextStyle *styles;   /**< per-cell base style, or NULL. */
    ScText     **texts;    /**< per-cell rich ScText, or NULL (entries may be NULL). */
    size_t       ncols;
    uint64_t     id;       /**< stable caller id (0 = unset). */
    bool         is_section; /**< non-selectable section header. */
    bool         disabled;   /**< greyed, non-selectable. */
    bool         checked;    /**< checked (multi-select). */
} Row;

/** Incremental fuzzy finder over a set of (multi-field) rows. */
struct ScFuzzy {
    Row   *rows;           /**< rows[i] = row i. */
    size_t count;
    size_t cap;
    bool   has_sections;   /**< any section header added. */
    size_t selectable_total; /**< rows that are neither section nor disabled. */
    ScFuzzyOpts opts;
    ScColor accent;
    int max_visible;

    /* Runtime state for the active run. */
    ScLineEditor query;
    Match *matches;        /**< Filtered display list; capacity `count`. */
    size_t match_n;        /**< Display entries (headers + matches). */
    size_t selectable_n;   /**< Selectable matches in the current filter. */
    size_t cursor;         /**< Index into `matches` (always selectable). */
    size_t top;            /**< First visible entry (scroll offset). */
    size_t pending_cursor; /**< add-order seed cursor (SIZE_MAX = none). */
    bool   multi;          /**< active run is multi-select. */
    bool   insert_mode;    /**< modal: currently in insert mode (else normal). */
};

static const char *const DEFAULT_HINT =
    "type to filter \xc2\xb7 \xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 "
    "enter select \xc2\xb7 esc cancel";

/* Modal default hints (per mode). \xc2\xb7 = "·", \xe2\x86\x91/\xe2\x86\x93 = ↑/↓ */
static const char *const HINT_NORMAL =
    "i insert \xc2\xb7 j/k move \xc2\xb7 enter select \xc2\xb7 esc cancel";
static const char *const HINT_INSERT =
    "type to filter \xc2\xb7 \xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 "
    "enter select \xc2\xb7 esc normal";


static void copy_opts_strings(ScFuzzy *self);
static void free_opts_strings(ScFuzzy *self);
static void refilter(ScFuzzy *self);
    static bool row_matches(const ScFuzzy *self, size_t i, const char *query,
                            int *best);
static ScRendered *fuzzy_render(void *state);
    static ScRendered *render_query_line(ScFuzzy *self);
    static int render_list_width(ScFuzzy *self);
    static ScRendered *render_list(ScFuzzy *self, int interior_w, bool fill);
        static void append_highlighted(
            ScText *text, const char *label, const char *query,
            ScTextStyle base_style, ScColor accent);
            static void append_run(ScText *text, const char *start,
                                   const char *end, ScTextStyle style);
    static ScRendered *render_table(ScFuzzy *self, int total_width);
        static ScTableData *fuzzy_table_columns(ScFuzzy *self, bool stretch);
        static bool fuzzy_checkbox_column(const ScFuzzy *self);
        static ScTableOpts resolve_fuzzy_table_opts(const ScFuzzy *self,
                                                    int total_width);
    static ScRendered *render_scroll_hint(ScFuzzy *self);
static void fuzzy_on_key(void *state, ScKey key, bool *done, bool *cancel);
static int match_cmp(const void *a, const void *b);
static bool entry_selectable(const ScFuzzy *self, size_t k);
static size_t first_selectable(const ScFuzzy *self);
static size_t last_selectable(const ScFuzzy *self);
static size_t prev_selectable(const ScFuzzy *self, size_t from);
static size_t next_selectable(const ScFuzzy *self, size_t from);
static void section_jump(ScFuzzy *self, int dir);
static void scroll_to_cursor(ScFuzzy *self);
static void grow_rows(ScFuzzy *self);
static void free_row(Row *row);
static ScText *clone_text(const ScText *src);
static char *flatten_text(const ScText *src);
static size_t cp_len(unsigned char lead);


bool sc_fuzzy_match(const char *pattern, const char *str, int *score) {
    if (!pattern || !pattern[0]) {
        if (score) {
            *score = 0;
        }
        return true;
    }
    if (!str) {
        if (score) {
            *score = 0;
        }
        return false;
    }

    const char *p = pattern;
    const char *s = str;
    int total = 0;
    int streak = 0;
    while (*p && *s) {
        if (tolower((unsigned char)*p) == tolower((unsigned char)*s)) {
            total += 1 + streak;
            if (s == str || s[-1] == ' ' || s[-1] == '_' || s[-1] == '-') {
                total += 2;   // word-start bonus
            }
            streak++;
            p++;
        } else {
            streak = 0;
        }
        s++;
    }
    bool ok = (*p == '\0');
    if (score) {
        *score = ok ? total : 0;
    }
    return ok;
}

ScFuzzy *sc_fuzzy_new(ScFuzzyOpts opts) {
    sc_theme_apply_fuzzy(&opts);
    ScFuzzy *self = calloc(1, sizeof *self);
    if (!self) {
        return NULL;
    }
    self->opts = opts;
    copy_opts_strings(self);
    /* Default accent resolves the runtime palette "accent" (overridable via
       sc_palette_set), falling back to ANSI cyan; explicit opts/theme win. */
    ScColor pal_accent;
    self->accent = opts.accent.index ? opts.accent
                 : (sc_color_by_name("accent", &pal_accent) ? pal_accent
                                                            : SC_ANSI_COLOR_CYAN);
    self->max_visible = opts.max_visible > 0 ? opts.max_visible : 10;
    self->pending_cursor = SIZE_MAX;
    return self;
}

/* Screen rows the finder spends *around* the result body: the query line, the
   scroll-indicator reserve, the box frame, the widget's own hint footer, and the
   engine's labeled-shortcut footer. The result body (list rows or the table)
   fills whatever remains. */
static int fuzzy_chrome_rows(const ScFuzzy *self) {
    ScBoxStyle b = self->opts.box;
    bool border = b.enabled || b.border.type != SC_BORDER_NONE;
    bool framed = border || b.bg.index != 0 || sc_edges_any(b.padding)
                  || b.width > 0 || b.width_mode != SC_WIDTH_DEFAULT;
    int rows = 1;                              /* query/prompt line */
    rows += 1;                                 /* scroll-indicator reserve */
    if (framed) { rows += (border ? 2 : 0) + b.padding.top + b.padding.bottom; }
    if (self->opts.hint_layout != SC_HINT_HIDDEN
        && self->opts.hint_pos != SC_HINT_POS_LEFT
        && self->opts.hint_pos != SC_HINT_POS_RIGHT) {
        rows += 1;                             /* own hint footer (inline) */
    }
    /* The engine stacks the labeled-shortcut footer beneath the frame. */
    rows += sc_shortcut_hint_rows(self->opts.shortcuts, self->opts.n_shortcuts);
    return rows;
}

/* Per-entry (`per`) and constant (`base`) screen rows the *table* view spends on
   its own chrome - outer border, header row + separator, and the inner
   separators between rows - so `table_height(V) = base + per*V`. Assumes
   single-line rows (the finder truncates columns; it does not word-wrap). */
static void fuzzy_table_overhead(const ScFuzzy *self, int *per, int *base) {
    ScTableOpts t = resolve_fuzzy_table_opts(self, 0);
    int vpad = t.cell_pad.top + t.cell_pad.bottom;
    bool border = t.border.type != SC_BORDER_NONE;
    bool inner = border && !t.border.no_inner_h;   /* separator between rows */
    *per = 1 + vpad + (inner ? 1 : 0);
    int b = 0;
    if (border && !t.border.no_outer) { b += 2; }       /* top + bottom border */
    if (t.header.row) { b += 1 + vpad + (border ? 1 : 0); } /* header + sep */
    if (inner) { b -= 1; }   /* V rows have only V-1 separators between them */
    *base = b;
}

/* Number of result *entries* the finder shows before it scrolls. The budget is
   view-aware: the list view spends one screen row per entry, the table view
   spends `fuzzy_table_overhead` (borders/header/separators) on top of one row
   per entry. `fullscreen` fills below the pinned header; `max_height` caps to
   that many rows; otherwise the `max_visible` viewport applies. Every mode is
   clamped so the rendered frame never overflows the terminal. */
static size_t effective_visible(const ScFuzzy *self) {
    int chrome = fuzzy_chrome_rows(self);
    bool fs = self->opts.fullscreen;
    int cap;
    if (fs) {
        int header_h = self->opts.header
            ? (int)self->opts.header->line_count : 0;
        cap = sc_terminal_height() - header_h;
        if (self->opts.max_height > 0 && self->opts.max_height < cap) {
            cap = self->opts.max_height;
        }
    } else if (self->opts.max_height > 0) {
        cap = self->opts.max_height;
    } else {
        cap = sc_terminal_height();
    }
    int avail = cap - chrome;                  /* screen rows for the body */
    if (avail < 1) { avail = 1; }

    size_t entries;
    if (self->opts.table) {
        int per, base;
        fuzzy_table_overhead(self, &per, &base);
        if (per < 1) { per = 1; }
        int v = (avail - base) / per;
        entries = v < 1 ? 1 : (size_t)v;
    } else {
        entries = (size_t)avail;               /* one row per entry */
    }
    /* Default mode (no fullscreen / max_height): the max_visible viewport. */
    if (!fs && self->opts.max_height == 0) {
        size_t vis = self->max_visible > 0 ? (size_t)self->max_visible : 10;
        if (entries > vis) { entries = vis; }
    }
    return entries < 1 ? 1 : entries;
}

void sc_fuzzy_add(ScFuzzy *self, const char *label) {
    const char *one[1] = { label };
    sc_fuzzy_add_row(self, one, 1);
}

/**
 * Appends a row, filling the common fields. `styles`/`texts` (optional, may be
 * NULL) are borrowed for the call: their entries are copied/duplicated here.
 * Returns the new row's add-order index, or `SIZE_MAX` on failure.
 */
static size_t add_row_full(ScFuzzy *self, const char *const *fields,
                           const ScTextStyle *styles, ScText *const *texts,
                           size_t n) {
    grow_rows(self);
    if (self->count == self->cap) {
        return SIZE_MAX;   // grow failed (OOM): no slot
    }
    // calloc: checks the count * size multiplication for overflow
    char **f = calloc(n, sizeof *f);
    if (!f) {
        return SIZE_MAX;
    }
    for (size_t c = 0; c < n; c++) {
        f[c] = sc_dup_str(fields[c]);
    }
    Row *row = &self->rows[self->count];
    *row = (Row){ .fields = f, .ncols = n };
    if (styles) {
        row->styles = calloc(n, sizeof *row->styles);
        if (row->styles) {
            memcpy(row->styles, styles, n * sizeof *row->styles);
        }
    }
    if (texts) {
        row->texts = calloc(n, sizeof *row->texts);
        if (row->texts) {
            for (size_t c = 0; c < n; c++) {
                row->texts[c] = texts[c] ? clone_text(texts[c]) : NULL;
            }
        }
    }
    self->count++;
    self->selectable_total++;
    return self->count - 1;
}

void sc_fuzzy_add_row(ScFuzzy *self, const char *const *fields, size_t n) {
    if (!self || n == 0) {
        return;
    }
    add_row_full(self, fields, NULL, NULL, n);
}

/** Marks the row at `idx` as a (non-selectable) section header. */
static void mark_section(ScFuzzy *self, size_t idx) {
    if (idx == SIZE_MAX) {
        return;
    }
    self->rows[idx].is_section = true;
    self->has_sections = true;
    self->selectable_total--;   // a section header is not selectable
}

void sc_fuzzy_add_section(ScFuzzy *self, const char *title) {
    if (!self) {
        return;
    }
    const char *one[1] = { title };
    mark_section(self, add_row_full(self, one, NULL, NULL, 1));
}

void sc_fuzzy_add_section_styled(ScFuzzy *self, const char *title,
                                 ScTextStyle style) {
    if (!self) {
        return;
    }
    const char *one[1] = { title };
    mark_section(self, add_row_full(self, one, &style, NULL, 1));
}

void sc_fuzzy_add_section_text(ScFuzzy *self, const ScText *title,
                               ScTextStyle fill) {
    if (!self || !title) {
        return;
    }
    /* Flatten for the display/width fallback (sections aren't matched); store
       the rich title in texts[0] and the bar/count style in styles[0]. */
    char *flat = flatten_text(title);
    const char *one[1] = { flat ? flat : "" };
    ScText *const rich[1] = { (ScText *)title };
    mark_section(self, add_row_full(self, one, &fill, rich, 1));
    free(flat);
}

void sc_fuzzy_add_styled(ScFuzzy *self, const char *label, ScTextStyle style) {
    if (!self) {
        return;
    }
    const char *one[1] = { label };
    add_row_full(self, one, &style, NULL, 1);
}

void sc_fuzzy_add_row_styled(ScFuzzy *self, const char *const *fields,
                             const ScTextStyle *styles, size_t n) {
    if (!self || n == 0) {
        return;
    }
    add_row_full(self, fields, styles, NULL, n);
}

void sc_fuzzy_add_row_rich(ScFuzzy *self, ScText *const *cells, size_t n) {
    if (!self || n == 0) {
        return;
    }
    const char **fields = calloc(n, sizeof *fields);
    char **flat = calloc(n, sizeof *flat);
    if (!fields || !flat) {
        free(fields);
        free(flat);
        return;
    }
    for (size_t c = 0; c < n; c++) {
        flat[c] = cells[c] ? flatten_text(cells[c]) : NULL;
        fields[c] = flat[c] ? flat[c] : "";
    }
    add_row_full(self, fields, NULL, cells, n);
    for (size_t c = 0; c < n; c++) {
        free(flat[c]);
    }
    free(flat);
    free(fields);
}

void sc_fuzzy_set_disabled(ScFuzzy *self, size_t index, bool on) {
    if (!self || index >= self->count) {
        return;
    }
    Row *r = &self->rows[index];
    if (r->is_section || r->disabled == on) {
        return;
    }
    r->disabled = on;
    if (on) {
        r->checked = false;   // a disabled row leaves the selection
        self->selectable_total--;
    } else {
        self->selectable_total++;
    }
    if (self->matches) {
        refilter(self);
    }
}

void sc_fuzzy_set_id(ScFuzzy *self, size_t index, uint64_t id) {
    if (self && index < self->count) {
        self->rows[index].id = id;
    }
}

uint64_t sc_fuzzy_id_at(const ScFuzzy *self, size_t index) {
    return (self && index < self->count) ? self->rows[index].id : 0;
}

uint64_t sc_fuzzy_cursor_id(const ScFuzzy *self) {
    if (!self || !self->matches || self->selectable_n == 0) {
        return 0;
    }
    return self->rows[self->matches[self->cursor].index].id;
}

void sc_fuzzy_set_checked(ScFuzzy *self, size_t index, bool on) {
    if (!self || index >= self->count) {
        return;
    }
    Row *r = &self->rows[index];
    if (!r->is_section && !r->disabled) {
        r->checked = on;
    }
}

bool sc_fuzzy_is_checked(const ScFuzzy *self, size_t index) {
    return self && index < self->count && self->rows[index].checked;
}

void sc_fuzzy_check_all(ScFuzzy *self, bool on) {
    if (!self) {
        return;
    }
    for (size_t i = 0; i < self->count; i++) {
        Row *r = &self->rows[i];
        if (!r->is_section && !r->disabled) {
            r->checked = on;
        }
    }
}

size_t sc_fuzzy_checked_count(const ScFuzzy *self) {
    if (!self) {
        return 0;
    }
    size_t n = 0;
    for (size_t i = 0; i < self->count; i++) {
        if (self->rows[i].checked) {
            n++;
        }
    }
    return n;
}

void sc_fuzzy_set_cursor(ScFuzzy *self, size_t index) {
    if (!self || index >= self->count) {
        return;
    }
    self->pending_cursor = index;
    if (!self->matches) {
        return;   // applied after the first refilter in run
    }
    for (size_t k = 0; k < self->match_n; k++) {
        if (!self->matches[k].is_section && self->matches[k].index == index) {
            self->cursor = k;
            size_t visible = effective_visible(self);
            if (self->cursor < self->top) {
                self->top = self->cursor;
            } else if (self->cursor >= self->top + visible) {
                self->top = self->cursor - visible + 1;
            }
            return;
        }
    }
}

const char *sc_fuzzy_label(const ScFuzzy *self, size_t index) {
    if (!self || index >= self->count || self->rows[index].ncols == 0) {
        return NULL;
    }
    return self->rows[index].fields[0];
}

void sc_fuzzy_set_label(ScFuzzy *self, size_t index, const char *label) {
    if (!self || index >= self->count || self->rows[index].ncols == 0) {
        return;
    }
    char *dup = sc_dup_str(label);
    if (!dup) {
        return;
    }
    free(self->rows[index].fields[0]);
    self->rows[index].fields[0] = dup;
    if (self->matches) {
        refilter(self);
    }
}

void sc_fuzzy_set_row(ScFuzzy *self, size_t index, const char *const *fields,
                      size_t n) {
    if (!self || index >= self->count || n == 0) {
        return;
    }
    char **f = calloc(n, sizeof *f);
    if (!f) {
        return;
    }
    for (size_t c = 0; c < n; c++) {
        f[c] = sc_dup_str(fields[c]);
    }
    Row *r = &self->rows[index];
    free_row(r);
    *r = (Row){ .fields = f, .ncols = n, .id = r->id,
                .is_section = r->is_section, .disabled = r->disabled,
                .checked = r->checked };
    if (self->matches) {
        refilter(self);
    }
}

void sc_fuzzy_set_row_style(ScFuzzy *self, size_t index, size_t col,
                            ScTextStyle style) {
    if (!self || index >= self->count || col >= self->rows[index].ncols) {
        return;
    }
    Row *r = &self->rows[index];
    if (!r->styles) {
        r->styles = calloc(r->ncols, sizeof *r->styles);
        if (!r->styles) {
            return;
        }
    }
    r->styles[col] = style;
}

void sc_fuzzy_free(ScFuzzy *self) {
    if (!self) {
        return;
    }
    for (size_t i = 0; i < self->count; i++) {
        free_row(&self->rows[i]);
    }
    free(self->rows);
    free(self->matches);
    free_opts_strings(self);
    free(self);
}

/** Chord that enters insert mode (zero-init = `i`). */
static ScKeyChord resolve_insert_key(const ScFuzzy *self) {
    ScKeyChord k = self->opts.insert_key;
    if (k.key == SC_KEY_NONE && k.codepoint == 0) {
        return (ScKeyChord){ .key = SC_KEY_CHAR, .codepoint = 'i' };
    }
    return k;
}

/** Chord that leaves insert mode / cancels in normal mode (zero-init = Esc). */
static ScKeyChord resolve_normal_key(const ScFuzzy *self) {
    ScKeyChord k = self->opts.normal_key;
    if (k.key == SC_KEY_NONE && k.codepoint == 0) {
        return (ScKeyChord){ .key = SC_KEY_ESC };
    }
    return k;
}

/** Engine predicate: suppress bare-letter shortcuts while in insert mode. */
static bool fuzzy_suppress_chars(void *state) {
    const ScFuzzy *self = state;
    return self->opts.modal && self->insert_mode;
}

/** Empties the query field in place (no realloc, cannot fail). */
static void clear_query(ScFuzzy *self) {
    self->query.len = 0;
    self->query.cursor = 0;
    if (self->query.buf) {
        self->query.buf[0] = '\0';
    }
}

SC_DEFINE_HINT_INDENT(fuzzy_hint_indent, ScFuzzy)

/** Shared setup + prompt loop for single/multi runs. Leaves `matches`/`cursor`
    intact for the caller to read results; the caller frees the query editor. */
static ScInputStatus fuzzy_begin(ScFuzzy *self, bool multi) {
    sc_le_init(&self->query, self->opts.initial_query);
    if (!self->query.buf) {
        return SC_INPUT_ERROR;
    }
    free(self->matches);   /* discard a previous run's list (re-run loops) */
    self->matches = calloc(self->count, sizeof *self->matches);
    if (!self->matches) {
        sc_le_free(&self->query);
        return SC_INPUT_ERROR;
    }
    self->multi = multi;
    self->cursor = 0;
    self->top = 0;
    /* Non-modal finders always "type" (insert-like); modal ones start in the
     * configured mode (normal by default). */
    self->insert_mode = self->opts.modal ? self->opts.start_in_insert : true;
    refilter(self);
    if (self->pending_cursor != SIZE_MAX) {
        sc_fuzzy_set_cursor(self, self->pending_cursor);
    }

    ScPromptVTable vtable = {
        .render = fuzzy_render,
        .on_key = fuzzy_on_key,
        .paste = SC_PASTE_TEXT,
        .capture_escape = self->opts.modal,
        .suppress_char_shortcuts =
            self->opts.modal ? fuzzy_suppress_chars : NULL,
        .hint_indent = fuzzy_hint_indent,
    };
    ScPromptShortcuts sk = {
        self->opts.shortcuts, self->opts.n_shortcuts, self->opts.out_shortcut_id
    };
    return sc_prompt_run(&vtable, self, self->opts.shortcuts ? &sk : NULL, NULL);
}

ScInputStatus sc_fuzzy_run(ScFuzzy *self, size_t *out_index) {
    if (!self || !out_index || self->count == 0) {
        return SC_INPUT_ERROR;
    }
    ScInputStatus status = fuzzy_begin(self, false);

    if (status == SC_INPUT_OK && self->selectable_n > 0) {
        // Enter requires a selectable match; a RETURN shortcut can fire with an
        // empty result set, so guard the dereference and report 0 in that case.
        size_t row_index = self->matches[self->cursor].index;
        *out_index = row_index;
        if (!self->opts.hide_summary) {
            const char *prompt =
                self->opts.prompt ? self->opts.prompt : "Search";
            const char *label = self->rows[row_index].fields[0];
            size_t size = strlen(prompt) + strlen(label) + 4;
            char *line = malloc(size);
            if (line) {
                snprintf(line, size, "? %s %s", prompt, label);
                sc_println(line, self->opts.summary_style);
                free(line);
            }
        }
    } else if (status == SC_INPUT_OK) {
        *out_index = 0;   // shortcut fired with no matches
    }
    sc_le_free(&self->query);
    return status;
}

ScInputStatus sc_fuzzy_run_multi(ScFuzzy *self, size_t *indices,
                                 size_t *count_io) {
    if (!self || !indices || !count_io || self->count == 0) {
        return SC_INPUT_ERROR;
    }
    size_t cap = *count_io;
    ScInputStatus status = fuzzy_begin(self, true);

    if (status == SC_INPUT_OK) {
        size_t written = 0;
        for (size_t i = 0; i < self->count && written < cap; i++) {
            if (self->rows[i].checked) {
                indices[written++] = i;
            }
        }
        *count_io = written;
        if (!self->opts.hide_summary) {
            char line[64];
            snprintf(line, sizeof line, "? %zu selected", written);
            sc_println(line, self->opts.summary_style);
        }
    } else {
        *count_io = 0;
    }
    sc_le_free(&self->query);
    return status;
}

size_t sc_fuzzy_cursor_index(const ScFuzzy *self) {
    if (!self || !self->matches || self->selectable_n == 0) {
        return 0;
    }
    return self->matches[self->cursor].index;
}

bool sc_fuzzy_has_selection(const ScFuzzy *self) {
    return self && self->matches && self->selectable_n > 0;
}

void sc_fuzzy_remove(ScFuzzy *self, size_t index) {
    if (!self || index >= self->count) {
        return;
    }
    bool was_selectable =
        !self->rows[index].is_section && !self->rows[index].disabled;
    free_row(&self->rows[index]);
    size_t tail = self->count - index - 1;
    memmove(&self->rows[index], &self->rows[index + 1],
            tail * sizeof *self->rows);
    self->count--;
    if (was_selectable && self->selectable_total > 0) {
        self->selectable_total--;
    }
    // Rebuild the filtered/ranked view against the shrunk set and re-clamp the
    // cursor. `matches` is only allocated during a run (when callbacks fire).
    if (self->matches) {
        refilter(self);
    }
}

void sc_fuzzy_refresh(ScFuzzy *self) {
    // `matches` is allocated only during a run; refilter re-applies the query
    // and re-clamps the cursor (a no-op outside a run).
    if (self && self->matches) {
        refilter(self);
    }
}

size_t sc_fuzzy_scroll_top(const ScFuzzy *self) {
    return self ? self->top : 0;
}

ScRendered *sc_fuzzy_frame(ScFuzzy *self, const char *query) {
    if (!self || self->count == 0) {
        return NULL;
    }
    sc_le_init(&self->query, query);
    self->matches = calloc(self->count, sizeof *self->matches);
    if (!self->query.buf || !self->matches) {
        sc_le_free(&self->query);
        free(self->matches);
        self->matches = NULL;
        return NULL;
    }
    self->cursor = 0;
    self->top = 0;
    self->insert_mode = self->opts.modal ? self->opts.start_in_insert : true;
    refilter(self);
    ScRendered *rendered = fuzzy_render(self);
    sc_le_free(&self->query);
    free(self->matches);
    self->matches = NULL;
    return rendered;
}

/**
 * Replaces the borrowed string fields of `self->opts` with heap copies, so
 * the finder honors the "opts are copied internally" contract: the caller's
 * strings only need to live until `sc_fuzzy_new` returns. A failed copy
 * (OOM) leaves the field NULL, which falls back to the built-in default.
 * `shortcuts` and `prompt_text` stay borrowed (documented per-field).
 */
static void copy_opts_strings(ScFuzzy *self) {
    ScFuzzyOpts *opts = &self->opts;
    opts->prompt = sc_dup_opt_str(opts->prompt);
    opts->cursor_marker = sc_dup_opt_str(opts->cursor_marker);
    opts->marker = sc_dup_opt_str(opts->marker);
    opts->hint = sc_dup_opt_str(opts->hint);
    opts->checkbox_on = sc_dup_opt_str(opts->checkbox_on);
    opts->checkbox_off = sc_dup_opt_str(opts->checkbox_off);
    opts->empty_text = sc_dup_opt_str(opts->empty_text);
    opts->initial_query = sc_dup_opt_str(opts->initial_query);
    opts->normal_label = sc_dup_opt_str(opts->normal_label);
    opts->insert_label = sc_dup_opt_str(opts->insert_label);

    if (opts->headers && opts->n_cols > 0) {
        char **headers = calloc(opts->n_cols, sizeof *headers);
        if (headers) {
            for (size_t c = 0; c < opts->n_cols; c++) {
                headers[c] = sc_dup_str(opts->headers[c]);
            }
        }
        opts->headers = (const char *const *)headers;
    }
}

/** Releases the opts strings duplicated by `copy_opts_strings`. */
static void free_opts_strings(ScFuzzy *self) {
    ScFuzzyOpts *opts = &self->opts;
    free((char *)opts->prompt);
    free((char *)opts->cursor_marker);
    free((char *)opts->marker);
    free((char *)opts->hint);
    free((char *)opts->checkbox_on);
    free((char *)opts->checkbox_off);
    free((char *)opts->empty_text);
    free((char *)opts->initial_query);
    free((char *)opts->normal_label);
    free((char *)opts->insert_label);
    if (opts->headers) {
        for (size_t c = 0; c < opts->n_cols; c++) {
            free((char *)opts->headers[c]);
        }
        free((void *)opts->headers);
    }
}

/**
 * Returns true when `query` matches any searched column of row `i`, writing the
 * best column score to `*best`. The searched columns are `opts.search_columns`
 * (a bitmask; 0 = all). List items have a single column, so they are
 * unaffected.
 */
static bool row_matches(const ScFuzzy *self, size_t i, const char *query,
                        int *best) {
    uint64_t mask = self->opts.search_columns;   /* 0 = all columns */
    int best_score = 0;
    bool matched = false;
    for (size_t c = 0; c < self->rows[i].ncols; c++) {
        if (mask != 0 && !(mask & ((uint64_t)1 << c))) {
            continue;
        }
        int score = 0;
        if (sc_fuzzy_match(query, self->rows[i].fields[c], &score)) {
            matched = true;
            if (score > best_score) {
                best_score = score;
            }
        }
    }
    *best = best_score;
    return matched;
}

/** True when display entry `k` is selectable (not a header, not disabled). */
static bool entry_selectable(const ScFuzzy *self, size_t k) {
    return !self->matches[k].is_section
        && !self->rows[self->matches[k].index].disabled;
}

static size_t first_selectable(const ScFuzzy *self) {
    for (size_t i = 0; i < self->match_n; i++) {
        if (entry_selectable(self, i)) { return i; }
    }
    return SIZE_MAX;
}

static size_t last_selectable(const ScFuzzy *self) {
    for (size_t i = self->match_n; i-- > 0; ) {
        if (entry_selectable(self, i)) { return i; }
    }
    return SIZE_MAX;
}

static size_t prev_selectable(const ScFuzzy *self, size_t from) {
    for (size_t i = from; i-- > 0; ) {
        if (entry_selectable(self, i)) { return i; }
    }
    return SIZE_MAX;
}

static size_t next_selectable(const ScFuzzy *self, size_t from) {
    for (size_t i = from + 1; i < self->match_n; i++) {
        if (entry_selectable(self, i)) { return i; }
    }
    return SIZE_MAX;
}

/**
 * Moves the cursor to the first selectable row of the next (`dir > 0`) or
 * previous (`dir < 0`) section, cycling around the ends. No-op without sections
 * or matches. Each shown group starts with a header followed by ≥1 row, so the
 * cursor always lands on a selectable entry. Bound to Tab / Shift-Tab.
 */
static void section_jump(ScFuzzy *self, int dir) {
    if (self->match_n == 0) {
        return;
    }
    size_t total = 0;   /* number of section headers in the display list */
    size_t cur = 0;     /* 0-based ordinal of the cursor's section */
    for (size_t i = 0; i < self->match_n; i++) {
        if (self->matches[i].is_section) {
            if (i <= self->cursor) { cur = total; }
            total++;
        }
    }
    if (total == 0) {
        return;
    }
    size_t step = (dir > 0) ? 1 : total - 1;   /* -1 mod total, unsigned */
    size_t target = (cur + step) % total;
    size_t ord = 0;
    for (size_t i = 0; i < self->match_n; i++) {
        if (!self->matches[i].is_section) { continue; }
        if (ord == target) {
            size_t sel = next_selectable(self, i);
            if (sel != SIZE_MAX) { self->cursor = sel; }
            break;
        }
        ord++;
    }
    scroll_to_cursor(self);
}

/* Active sort context for the qsort comparator (single prompt session). */
static const ScFuzzy *g_sort_ctx;

/** Sorts the match entries in `[start, end)` by the configured order. */
static void sort_range(ScFuzzy *self, size_t start, size_t end) {
    if (end > start) {
        g_sort_ctx = self;
        qsort(self->matches + start, end - start, sizeof *self->matches,
              match_cmp);
        g_sort_ctx = NULL;
    }
}

/** Moves the cursor to the nearest selectable entry (forward, then back). */
static void clamp_cursor_selectable(ScFuzzy *self) {
    if (self->match_n == 0) {
        self->cursor = 0;
        return;
    }
    if (self->cursor >= self->match_n) {
        self->cursor = self->match_n - 1;
    }
    if (entry_selectable(self, self->cursor)) {
        return;
    }
    for (size_t i = self->cursor; i < self->match_n; i++) {
        if (entry_selectable(self, i)) { self->cursor = i; return; }
    }
    size_t p = prev_selectable(self, self->cursor);
    self->cursor = (p != SIZE_MAX) ? p : 0;
}

/**
 * Recomputes the filtered display list from the current query. Without sections
 * the matches are collected flat and ordered globally. With sections each group
 * is emitted as `[header][its matches]`, the header only when the group has a
 * match (empty groups stay hidden), and ordering applies within each group.
 * Disabled rows still appear (greyed) when they match but are not selectable.
 */
static void refilter(ScFuzzy *self) {
    const char *query = self->query.buf;
    // Rows can grow mid-run (sc_fuzzy_add + sc_fuzzy_refresh). The display list
    // never holds more than one entry per row, so a `count`-sized array fits.
    if (self->count > 0) {
        void *grown =
            realloc(self->matches, self->count * sizeof *self->matches);
        if (grown) { self->matches = grown; }
    }
    self->match_n = 0;
    self->selectable_n = 0;

    if (!self->has_sections) {
        for (size_t i = 0; i < self->count; i++) {
            int score = 0;
            if (row_matches(self, i, query, &score)) {
                self->matches[self->match_n].index = i;
                self->matches[self->match_n].score = score;
                self->matches[self->match_n].is_section = false;
                self->match_n++;
                if (!self->rows[i].disabled) { self->selectable_n++; }
            }
        }
        sort_range(self, 0, self->match_n);
    } else {
        size_t pending_header = SIZE_MAX;  /* header awaiting first match */
        bool header_open = false;
        size_t group_start = 0;            /* first match entry of current group */
        for (size_t i = 0; i < self->count; i++) {
            if (self->rows[i].is_section) {
                sort_range(self, group_start, self->match_n);
                pending_header = i;
                header_open = false;
                group_start = self->match_n;
                continue;
            }
            int score = 0;
            if (!row_matches(self, i, query, &score)) {
                continue;
            }
            if (pending_header != SIZE_MAX && !header_open) {
                self->matches[self->match_n].index = pending_header;
                self->matches[self->match_n].score = 0;
                self->matches[self->match_n].is_section = true;
                self->match_n++;
                header_open = true;
                group_start = self->match_n;
            }
            self->matches[self->match_n].index = i;
            self->matches[self->match_n].score = score;
            self->matches[self->match_n].is_section = false;
            self->match_n++;
            if (!self->rows[i].disabled) { self->selectable_n++; }
        }
        sort_range(self, group_start, self->match_n);
    }

    clamp_cursor_selectable(self);

    scroll_to_cursor(self);   /* anchors a leading section header into view */
    if (self->match_n <= effective_visible(self)) {
        self->top = 0;
    }
}

/**
 * Appends a 1-column vertical scrollbar (a leading gap + track/thumb glyph) to
 * the right of every line of `body`, padding each line to the body width first.
 * The thumb spans the body height `h`, sized/positioned from the scroll window
 * (`window` of `total` items, offset `top`). Consumes `body`, returns a new
 * `ScRendered` (or the original on failure). Body lines are library-generated
 * (already past the trust boundary), so they are appended raw to keep their ANSI.
 */
static ScRendered *attach_scrollbar(ScRendered *body, int width, size_t top,
                                    size_t total, size_t window) {
    if (!body || body->line_count == 0 || total == 0) {
        return body;
    }
    size_t h = body->line_count;
    if (width < body->max_column_width) { width = body->max_column_width; }
    size_t thumb = (h * window + total - 1) / total;   /* ceil */
    if (thumb < 1) { thumb = 1; }
    if (thumb > h) { thumb = h; }
    size_t span = (total > window) ? total - window : 0;
    size_t pos = 0;
    if (span > 0 && h > thumb) {
        pos = (top * (h - thumb) + span / 2) / span;   /* rounded */
        if (pos > h - thumb) { pos = h - thumb; }
    }

    ScText *t = sc_text_new();
    if (!t) { return body; }
    ScTextStyle none = { 0 };
    ScTextStyle dim = { SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    for (size_t i = 0; i < h; i++) {
        const char *line = body->lines[i] ? body->lines[i] : "";
        sc_text_append_raw(t, line, none);
        int vis = body->column_widths ? body->column_widths[i] : width;
        for (int p = vis; p < width; p++) { sc_text_append_raw(t, " ", none); }
        sc_text_append_raw(t, " ", none);   /* gap before the bar */
        bool on_thumb = (i >= pos && i < pos + thumb);
        sc_text_append(t, on_thumb ? "\xe2\x96\x88" : "\xe2\x96\x91", dim); /* █ ░ */
        if (i + 1 < h) { sc_text_append_raw(t, "\n", none); }
    }
    ScRendered *out = sc_capture_text(t);
    sc_text_free(t);
    if (!out) { return body; }
    sc_rendered_free(body);
    return out;
}

static ScRendered *fuzzy_render(void *state) {
    ScFuzzy *self = state;
    ScRendered *query = render_query_line(self);
    ScRendered *scroll = render_scroll_hint(self);
    if (!query) {
        sc_rendered_free(scroll);
        return NULL;
    }

    /* Resolve the widget width across the query line, the results and the
     * scroll indicator, then map it through the box's width mode. */
    int query_w = query->max_column_width;
    int scroll_w = scroll ? scroll->max_column_width : 0;
    ScRendered *body = NULL;
    int body_w;
    if (self->opts.table) {
        body = render_table(self, 0);   /* natural width first */
        body_w = body ? body->max_column_width : 0;
    } else {
        body_w = render_list_width(self);   /* measured before rendering */
    }

    /* Right-edge scrollbar (on by default while the list scrolls): reserve two
       columns (gap + bar) inside the interior so the box still fits. */
    size_t sb_window = effective_visible(self);
    bool sb = self->match_n > sb_window && !self->opts.no_scrollbar;
    int sb_cols = sb ? 2 : 0;

    int content_w = query_w;
    if (scroll_w > content_w) { content_w = scroll_w; }
    if (body_w > content_w) { content_w = body_w; }
    content_w += sb_cols;
    ScBoxLayout layout = sc_box_layout(self->opts.box, content_w,
                                       sc_terminal_width(), SC_WIDTH_CONTENT);
    bool fill = self->opts.box.bg_extent != SC_BG_EXTENT_TEXT;

    int body_target = layout.interior_w - sb_cols;
    if (body_target < 1) { body_target = 1; }
    if (!self->opts.table) {
        body = render_list(self, body_target, fill);
    } else if (self->opts.stretch_columns && body
               && body_target > body->max_column_width) {
        /* Box is wider than the natural table: re-render filling the interior,
           the surplus going to the stretch columns. */
        ScRendered *wide = render_table(self, body_target);
        if (wide) {
            sc_rendered_free(body);
            body = wide;
        }
    }
    if (!body) {
        sc_rendered_free(query);
        sc_rendered_free(scroll);
        return NULL;
    }
    if (sb) {
        /* Pad to the box interior so the bar sits flush at the right edge (just
           left of the frame) even when the body is narrower than the box. */
        body = attach_scrollbar(body, body_target, self->top, self->match_n,
                                sb_window);
    }

    /* Empty-state line when nothing matches the current query. */
    ScRendered *empty = NULL;
    if (self->match_n == 0 && self->opts.empty_text) {
        ScText *et = sc_text_new();
        if (et) {
            ScTextStyle es = sc_style_set(self->opts.empty_style)
                ? self->opts.empty_style
                : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                                 SC_ANSI_COLOR_NONE };
            sc_text_append(et, self->opts.empty_text, es);
            empty = sc_capture_text(et);
            sc_text_free(et);
        }
    }

    const ScRendered *parts[4];
    size_t count = 0;
    parts[count++] = query;
    parts[count++] = body;
    if (empty) {
        parts[count++] = empty;
    }
    if (scroll) {
        parts[count++] = scroll;
    }
    ScRendered *stacked = sc_vstack(parts, count, 0);
    sc_rendered_free(query);
    sc_rendered_free(body);
    sc_rendered_free(empty);
    sc_rendered_free(scroll);
    if (!stacked) {
        return NULL;
    }
    if (layout.active) {
        ScRendered *framed = sc_capture_panel_rendered(stacked, layout.panel);
        if (framed) {
            sc_rendered_free(stacked);
            stacked = framed;
        }
    }
    const char *default_hint = DEFAULT_HINT;
    if (self->opts.modal) {
        default_hint = self->insert_mode ? HINT_INSERT : HINT_NORMAL;
    }
    ScRendered *frame = sc_compose_hint(stacked,
                           self->opts.hint ? self->opts.hint : default_hint,
                           self->opts.hint_layout, self->opts.hint_pos,
                           self->opts.hint_style,
                           sc_box_content_left(self->opts.box));
    if (self->opts.fullscreen) {
        /* Pin the header above and align the block; recomputed each frame so a
           growing list re-aligns and fills the screen. Reserve the engine's
           shortcut footer so it is not pushed off the bottom. */
        int footer = sc_shortcut_hint_rows(self->opts.shortcuts,
                                           self->opts.n_shortcuts);
        frame = sc_fullscreen_compose(frame, self->opts.header,
                                      self->opts.valign, footer);
    }
    return frame;
}

/** Resolved badge / field style for the active modal mode. */
static ScTextStyle mode_style_of(const ScFuzzy *self) {
    if (self->insert_mode) {
        return sc_style_set(self->opts.mode_insert_style)
            ? self->opts.mode_insert_style
            : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                             SC_ANSI_COLOR_GREEN };
    }
    return sc_style_set(self->opts.mode_normal_style)
        ? self->opts.mode_normal_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                         SC_ANSI_COLOR_BLUE };
}

/** Badge label for the active modal mode. */
static const char *mode_label_of(const ScFuzzy *self) {
    if (self->insert_mode) {
        return self->opts.insert_label ? self->opts.insert_label : "INSERT";
    }
    return self->opts.normal_label ? self->opts.normal_label : "NORMAL";
}

/** Builds the query/prompt line (query field scrolls when long). */
static ScRendered *render_query_line(ScFuzzy *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }

    /* Modal mode badge (e.g. " NORMAL ") on a colored bar, plus a space. */
    int badge_w = 0;
    if (self->opts.modal && !self->opts.hide_mode_badge) {
        char badge[40];
        int n = snprintf(badge, sizeof badge, " %s ", mode_label_of(self));
        sc_text_append(text, badge, mode_style_of(self));
        sc_text_append(text, " ", (ScTextStyle){ 0 });
        badge_w = (int)sc_utf8_string_length(badge, (size_t)n) + 1;
    }

    const char *prompt = self->opts.prompt ? self->opts.prompt : "Search";
    ScTextStyle prompt_style = sc_style_set(self->opts.prompt_style)
        ? self->opts.prompt_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    sc_prompt_append(text, prompt, prompt_style,
                     self->opts.prompt_markup, self->opts.prompt_text);
    sc_text_append(text, " ", (ScTextStyle){ 0 });

    char counter[64];
    if (self->opts.multi) {
        snprintf(counter, sizeof counter, "  (%zu/%zu \xc2\xb7 %zu \xe2\x9c\x93)",
                 self->selectable_n, self->selectable_total,
                 sc_fuzzy_checked_count(self));
    } else {
        snprintf(counter, sizeof counter, "  (%zu/%zu)",
                 self->selectable_n, self->selectable_total);
    }

    // Query field = line width − prompt − space − counter.
    int prompt_w = (int)sc_utf8_string_length(prompt, strlen(prompt));
    int counter_w = (int)sc_utf8_string_length(counter, strlen(counter));
    int field = sc_terminal_width() - badge_w - prompt_w - 1 - counter_w;
    if (field < 1) {
        field = 1;
    }
    /* Tint the query field with the mode's signature color (modal only). */
    ScTextStyle field_style = { 0 };
    if (self->opts.modal) {
        ScTextStyle ms = mode_style_of(self);
        field_style.fg = ms.bg.index != 0 ? ms.bg : ms.fg;
    }
    sc_le_render_into(&self->query, text, field_style,
                      self->opts.cursor_style, NULL, NULL, (ScTextStyle){ 0 },
                      field);

    ScTextStyle counter_style = sc_style_set(self->opts.counter_style)
        ? self->opts.counter_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };
    sc_text_append(text, counter, counter_style);

    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/** Checkbox glyph for the given checked state (multi-select). */
static const char *cb_glyph(const ScFuzzy *self, bool on) {
    if (on) {
        return self->opts.checkbox_on ? self->opts.checkbox_on : "[x] ";
    }
    return self->opts.checkbox_off ? self->opts.checkbox_off : "[ ] ";
}

/** Resolved section-header style (zero-init = dim + bold). */
static ScTextStyle section_style_of(const ScFuzzy *self) {
    return sc_style_set(self->opts.section_style)
        ? self->opts.section_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM | SC_TEXT_ATTR_BOLD,
                         SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
}

/** Overlays the set fields of `over` onto `base` (per-cell color + cursor). */
static ScTextStyle merge_style(ScTextStyle base, ScTextStyle over) {
    if (over.attr != SC_TEXT_ATTR_NONE) { base.attr = over.attr; }
    if (over.fg.index != 0) { base.fg = over.fg; }
    if (over.bg.index != 0) { base.bg = over.bg; }
    return base;
}

/* Like merge_style but unions the attributes, so the cursor-row highlight
   (e.g. bold) adds to each cell's own attributes (e.g. dim/strike) rather than
   replacing them. Used only for the table cursor row. */
static ScTextStyle merge_cursor_style(ScTextStyle base, ScTextStyle over) {
    base.attr = (ScTextAttribute)(base.attr | over.attr);
    if (over.fg.index != 0) { base.fg = over.fg; }
    if (over.bg.index != 0) { base.bg = over.bg; }
    return base;
}

/** Effective style for a section header: the global default with the row's own
    `styles[0]` (from add_section_styled/_text) merged over it (set fields win). */
static ScTextStyle section_style_for(const ScFuzzy *self, const Row *r) {
    ScTextStyle base = section_style_of(self);
    return r->styles ? merge_style(base, r->styles[0]) : base;
}

/** Resolved disabled-row style (zero-init = dim). */
static ScTextStyle disabled_style_of(const ScFuzzy *self) {
    return sc_style_set(self->opts.disabled_style)
        ? self->opts.disabled_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE,
                         SC_ANSI_COLOR_NONE };
}

/** Number of matching data entries directly under the header at `entry`. */
static size_t group_match_count(const ScFuzzy *self, size_t entry) {
    size_t k = 0;
    for (size_t i = entry + 1;
         i < self->match_n && !self->matches[i].is_section; i++) {
        k++;
    }
    return k;
}

/** Appends another ScText's spans into `dst` via the trusted raw path. */
static void append_rich(ScText *dst, const ScText *src) {
    size_t n = sc_text_span_count(src);
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(src, i);
        if (span.raw_str) {
            sc_text_append_raw(dst, span.raw_str, span.style);
        }
    }
}

/** Visible width of the section header at display entry `entry`. */
static int section_entry_width(const ScFuzzy *self, size_t entry) {
    const char *title = self->rows[self->matches[entry].index].fields[0];
    int w = (int)sc_utf8_string_length(title, strlen(title));
    if (self->opts.section_counts) {
        char cnt[32];
        int n = snprintf(cnt, sizeof cnt, " (%zu)",
                         group_match_count(self, entry));
        w += (int)sc_utf8_string_length(cnt, (size_t)n);
    }
    return w;
}

/** Widest visible list row, for the box width resolution. */
static int render_list_width(ScFuzzy *self) {
    size_t visible = effective_visible(self);
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }
    const char *cursor_marker = self->opts.cursor_marker
        ? self->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *marker = self->opts.marker ? self->opts.marker : "  ";
    bool multi = self->opts.multi;
    int max = 0;
    for (size_t i = self->top; i < end; i++) {
        int w;
        if (self->matches[i].is_section) {
            w = section_entry_width(self, i);
        } else {
            size_t row_index = self->matches[i].index;
            const char *mk = (i == self->cursor) ? cursor_marker : marker;
            const char *label = self->rows[row_index].fields[0];
            w = (int)sc_utf8_string_length(mk, strlen(mk))
              + (int)sc_utf8_string_length(label, strlen(label));
            if (multi) {
                const char *cb = cb_glyph(self, self->rows[row_index].checked);
                w += (int)sc_utf8_string_length(cb, strlen(cb));
            }
        }
        if (w > max) { max = w; }
    }
    return max;
}

/**
 * List view: one row per match, matched characters emphasized. The cursor row
 * is extended into a full-width highlight bar (to `interior_w`) when `fill` is
 * set and the cursor style carries a background.
 */
static ScRendered *render_list(ScFuzzy *self, int interior_w, bool fill) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    size_t visible = effective_visible(self);
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }

    ScTextStyle selected_style = sc_style_set(self->opts.selected_style)
        ? self->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    ScTextStyle disabled_style = disabled_style_of(self);
    const char *cursor_marker = self->opts.cursor_marker
        ? self->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *marker = self->opts.marker ? self->opts.marker : "  ";
    bool multi = self->opts.multi;

    for (size_t i = self->top; i < end; i++) {
        size_t row_index = self->matches[i].index;
        Row *r = &self->rows[row_index];

        if (self->matches[i].is_section) {
            ScTextStyle ss = section_style_for(self, r);
            const char *title = r->fields[0];
            if (r->texts && r->texts[0]) {
                append_rich(text, r->texts[0]);   /* rich multi-span title */
            } else {
                sc_text_append(text, title, ss);
            }
            int used = (int)sc_utf8_string_length(title, strlen(title));
            if (self->opts.section_counts) {
                char cnt[32];
                int n = snprintf(cnt, sizeof cnt, " (%zu)",
                                 group_match_count(self, i));
                sc_text_append(text, cnt, ss);
                used += (int)sc_utf8_string_length(cnt, (size_t)n);
            }
            if (fill && ss.bg.index != 0) {
                sc_pad_line_to(text, used, interior_w, ss);
            }
            if (i + 1 < end) {
                sc_text_append(text, "\n", (ScTextStyle){ 0 });
            }
            continue;
        }

        bool on_cursor = (i == self->cursor);
        ScTextStyle base = r->disabled ? disabled_style
                         : on_cursor   ? selected_style
                         : (r->styles ? r->styles[0] : (ScTextStyle){ 0 });
        const char *mk = on_cursor ? cursor_marker : marker;
        const char *label = r->fields[0];
        int used = 0;
        sc_text_append(text, mk, base);
        used += (int)sc_utf8_string_length(mk, strlen(mk));
        if (multi) {
            const char *cb = cb_glyph(self, r->checked);
            sc_text_append(text, cb, base);
            used += (int)sc_utf8_string_length(cb, strlen(cb));
        }
        if (r->texts && r->texts[0]) {
            append_rich(text, r->texts[0]);   /* rich cell: no match overlay */
        } else {
            append_highlighted(text, label, self->query.buf, base,
                               self->accent);
        }
        used += (int)sc_utf8_string_length(label, strlen(label));
        if (on_cursor && fill && selected_style.bg.index != 0) {
            sc_pad_line_to(text, used, interior_w, selected_style);
        }
        if (i + 1 < end) {
            sc_text_append(text, "\n", (ScTextStyle){ 0 });
        }
    }
    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/**
 * Appends `label` codepoint-by-codepoint, emphasizing the characters the
 * (greedy, case-insensitive) query subsequence matches - same order as
 * `sc_fuzzy_match`.
 */
/** Appends the byte range `[start, end)` as one styled span. */
static void append_run(ScText *text, const char *start, const char *end,
                       ScTextStyle style) {
    size_t n = (size_t)(end - start);
    char *buf = malloc(n + 1);
    if (!buf) {
        return;
    }
    memcpy(buf, start, n);
    buf[n] = '\0';
    sc_text_append(text, buf, style);
    free(buf);
}

static void append_highlighted(ScText *text, const char *label,
                               const char *query, ScTextStyle base_style,
                               ScColor accent) {
    ScTextStyle hit_style = base_style;
    hit_style.attr |= SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER;
    if (base_style.fg.index == 0) {
        hit_style.fg = accent;
    }

    /* Coalesce consecutive characters with the same match state into one span
     * (fewer escape sequences, esp. once a background fills behind every span). */
    const char *pattern = (query && query[0]) ? query : NULL;
    const char *s = label;
    const char *run = s;
    bool run_hit = false;
    bool have = false;
    while (*s) {
        size_t len = cp_len((unsigned char)*s);
        bool hit = pattern && *pattern
            && tolower((unsigned char)*pattern) == tolower((unsigned char)*s);
        if (hit) {
            pattern++;
        }
        if (!have) {
            run = s;
            run_hit = hit;
            have = true;
        } else if (hit != run_hit) {
            append_run(text, run, s, run_hit ? hit_style : base_style);
            run = s;
            run_hit = hit;
        }
        s += len;
    }
    if (have) {
        append_run(text, run, s, run_hit ? hit_style : base_style);
    }
}

/** Table view: visible entries as a sparcli table. Section headers span all
    columns; the cursor row gets a background; rows carry per-cell styles, rich
    cells, disabled greying and (multi) a checkbox column or glyph. */
static ScRendered *render_table(ScFuzzy *self, int total_width) {
    ScTableData *table = fuzzy_table_columns(self, total_width > 0);
    if (!table) {
        return NULL;
    }
    size_t cols = self->opts.n_cols ? self->opts.n_cols : 1;
    bool multi = self->opts.multi;
    bool cb = fuzzy_checkbox_column(self);
    size_t off = cb ? 1 : 0;          /* leading checkbox column offset */
    size_t tcols = cols + off;        /* total table columns */

    size_t visible = effective_visible(self);
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }

    // Cells with matched characters / styling are built as ScText (borrowed by
    // the table), so they must outlive the capture below.
    const char *query = self->query.buf;
    uint64_t mask = self->opts.search_columns;   /* 0 = all columns */
    ScTextStyle sel = self->opts.selected_style; /* cursor-row text style */
    bool sel_set = sc_style_set(sel);
    ScTextStyle disabled_style = disabled_style_of(self);
    /* Cursor-row background: selected_style.bg overrides the accent default. */
    ScColor cursor_bg = sel.bg.index != 0 ? sel.bg : self->accent;
    // calloc(0, ...) is implementation-defined; with no visible rows the
    // loop below doesn't run, so skip the allocation entirely.
    size_t shown = (end > self->top) ? end - self->top : 0;
    ScText **texts = shown > 0 ? calloc(shown * tcols, sizeof *texts) : NULL;
    size_t n_texts = 0;

    for (size_t i = self->top; i < end; i++) {
        size_t row_index = self->matches[i].index;
        Row *r = &self->rows[row_index];
        ScCell *cells = calloc(tcols, sizeof *cells);
        if (!cells) {
            break;
        }

        if (self->matches[i].is_section) {
            /* One row spanning every column with the (styled) header text. */
            ScTextStyle ss = section_style_for(self, r);
            ScText *t = texts ? sc_text_new() : NULL;
            if (t) {
                if (r->texts && r->texts[0]) {
                    append_rich(t, r->texts[0]);   /* rich multi-span title */
                } else {
                    sc_text_append(t, r->fields[0], ss);
                }
                if (self->opts.section_counts) {
                    char cnt[32];
                    snprintf(cnt, sizeof cnt, " (%zu)",
                             group_match_count(self, i));
                    sc_text_append(t, cnt, ss);
                }
                texts[n_texts++] = t;
                cells[0] = sc_cell_tcs(t, (int)tcols);
            } else {
                cells[0] = sc_cell_cs(r->fields[0], (int)tcols);
            }
            for (size_t c = 1; c < tcols; c++) {
                cells[c] = sc_cell_skip();
            }
            if (ss.bg.index != 0) {
                sc_table_add_row_bg(table, cells, tcols, ss.bg);
            } else {
                sc_table_add_row(table, cells, tcols);
            }
            free(cells);
            continue;
        }

        bool on_cursor = (i == self->cursor);
        if (cb) {
            cells[0] = sc_cell(cb_glyph(self, r->checked));
        }
        for (size_t dc = 0; dc < cols; dc++) {
            size_t pos = dc + off;
            const char *value = (dc < r->ncols) ? r->fields[dc] : "";
            ScText *rich = (r->texts && dc < r->ncols) ? r->texts[dc] : NULL;
            bool searched = (mask == 0) || (mask & ((uint64_t)1 << dc));
            bool hl = !rich && searched && query[0]
                    && sc_fuzzy_match(query, value, NULL);
            ScTextStyle cell = r->styles ? r->styles[dc] : (ScTextStyle){ 0 };
            ScTextStyle base = r->disabled ? disabled_style
                             : on_cursor   ? merge_cursor_style(cell, sel)
                             : cell;
            bool styled = sc_style_set(base);
            bool prefix_cb = (multi && !cb && dc == 0);
            bool need_text = texts
                && (rich || hl || styled || prefix_cb
                    || (on_cursor && sel_set));
            if (need_text) {
                ScText *t = sc_text_new();
                if (t) {
                    if (prefix_cb) {
                        sc_text_append(t, cb_glyph(self, r->checked), base);
                    }
                    if (rich) {
                        append_rich(t, rich);
                    } else if (hl) {
                        append_highlighted(t, value, query, base,
                            on_cursor ? SC_ANSI_COLOR_NONE : self->accent);
                    } else {
                        sc_text_append(t, value, base);
                    }
                    texts[n_texts++] = t;
                    cells[pos] = sc_cell_t(t);
                } else {
                    cells[pos] = sc_cell(value);
                }
            } else {
                cells[pos] = sc_cell(value);
            }
        }
        if (on_cursor) {
            sc_table_add_row_bg(table, cells, tcols, cursor_bg);
        } else {
            sc_table_add_row(table, cells, tcols);
        }
        free(cells);
    }

    ScRendered *rendered =
        sc_capture_table(table, resolve_fuzzy_table_opts(self, total_width));
    sc_table_free(table);
    for (size_t t = 0; t < n_texts; t++) {
        sc_text_free(texts[t]);
    }
    free(texts);
    return rendered;
}

/** True when the table view renders a dedicated leading checkbox column. */
static bool fuzzy_checkbox_column(const ScFuzzy *self) {
    return self->opts.multi && self->opts.checkbox_column;
}

/** Builds the table with one (word-wrap-off) column per configured field, plus
    an optional leading checkbox column for multi-select. When `stretch` is set,
    columns NOT in `opts.stretch_columns` are marked `no_stretch` so the
    `total_width` surplus reaches only the selected column(s). */
static ScTableData *fuzzy_table_columns(ScFuzzy *self, bool stretch) {
    ScTableData *table = sc_table_new();
    if (!table) {
        return NULL;
    }
    uint64_t mask = self->opts.stretch_columns;
    if (fuzzy_checkbox_column(self)) {
        const char *off = cb_glyph(self, false);
        int w = (int)sc_utf8_string_length(off, strlen(off));
        sc_table_add_column(table, "",
            (ScColOpts){ .word_wrap = false, .fixed_width = w });
    }
    size_t cols = self->opts.n_cols ? self->opts.n_cols : 1;
    for (size_t c = 0; c < cols; c++) {
        const char *header = (self->opts.headers && c < self->opts.n_cols)
            ? self->opts.headers[c] : "";
        bool stretch_col = stretch && (mask & ((uint64_t)1 << c));
        sc_table_add_column(table, header,
            (ScColOpts){ .word_wrap = false, .no_stretch = !stretch_col });
    }
    return table;
}

/**
 * Resolves the caller's `table_opts`, filling the fuzzy defaults (single
 * border, bold header row) only where a field was left zero-init.
 */
static ScTableOpts resolve_fuzzy_table_opts(const ScFuzzy *self,
                                            int total_width) {
    ScTableOpts opts = self->opts.table_opts;
    if (opts.border.type == SC_BORDER_NONE) {
        opts.border.type = SC_BORDER_SINGLE;
    }
    if (!opts.header.row) {
        opts.header.row = true;
        if (!sc_style_set(opts.header.style)) {
            opts.header.style = (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        }
    }
    if (total_width > 0) { opts.total_width = total_width; }
    return opts;
}

/** Builds a dim "↑ first-last/total ↓" line, or NULL when nothing is hidden. */
static ScRendered *render_scroll_hint(ScFuzzy *self) {
    size_t visible = effective_visible(self);
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }
    if (self->top == 0 && end >= self->match_n) {
        return NULL;
    }

    char buf[80];
    snprintf(buf, sizeof buf, "  %s %zu-%zu/%zu %s",
             self->top > 0       ? "\xe2\x86\x91" : " ",
             self->top + 1, end, self->match_n,
             end < self->match_n ? "\xe2\x86\x93" : " ");
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    sc_text_append(text, buf, (ScTextStyle){ SC_TEXT_ATTR_DIM,
                   SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    ScRendered *rendered = sc_capture_text(text);
    sc_text_free(text);
    return rendered;
}

/** Toggles the checked state of all selectable display entries in `[lo, hi)`:
    checks them all unless they are already all checked (then unchecks). */
static void toggle_range(ScFuzzy *self, size_t lo, size_t hi) {
    bool all = true;
    bool any = false;
    for (size_t i = lo; i < hi; i++) {
        if (entry_selectable(self, i)) {
            any = true;
            if (!self->rows[self->matches[i].index].checked) {
                all = false;
                break;
            }
        }
    }
    if (!any) {
        return;
    }
    bool target = !all;
    for (size_t i = lo; i < hi; i++) {
        if (entry_selectable(self, i)) {
            self->rows[self->matches[i].index].checked = target;
        }
    }
}

/** Toggles the cursor's section group (the contiguous run of non-header display
    entries around the cursor). Without sections this spans the whole list. */
static void toggle_section_displayed(ScFuzzy *self) {
    if (self->match_n == 0) {
        return;
    }
    size_t lo = self->cursor;
    while (lo > 0 && !self->matches[lo - 1].is_section) {
        lo--;
    }
    size_t hi = self->cursor + 1;
    while (hi < self->match_n && !self->matches[hi].is_section) {
        hi++;
    }
    toggle_range(self, lo, hi);
}

/** Moves the cursor to the next selectable entry (cycling unless `no_cycle`). */
static void cursor_down(ScFuzzy *self) {
    size_t n = next_selectable(self, self->cursor);
    if (n != SIZE_MAX) {
        self->cursor = n;
    } else if (!self->opts.no_cycle) {
        size_t f = first_selectable(self);
        if (f != SIZE_MAX) { self->cursor = f; }
    }
}

/** Moves the cursor to the previous selectable entry (cycling unless
    `no_cycle`). */
static void cursor_up(ScFuzzy *self) {
    size_t p = prev_selectable(self, self->cursor);
    if (p != SIZE_MAX) {
        self->cursor = p;
    } else if (!self->opts.no_cycle) {
        size_t l = last_selectable(self);
        if (l != SIZE_MAX) { self->cursor = l; }
    }
}

/** Keeps the cursor row within the visible viewport. When the cursor sits just
    below its section header, the viewport anchors to the header (it can't land on
    one - headers are non-selectable) so reaching a group's first row reveals the
    group title and the scrollbar/▲ indicator sit at the very top. */
static void scroll_to_cursor(ScFuzzy *self) {
    size_t visible = effective_visible(self);
    size_t anchor = self->cursor;
    while (anchor > 0 && self->matches[anchor - 1].is_section) {
        anchor--;
    }
    if (anchor < self->top) {
        self->top = anchor;
    } else if (self->cursor >= self->top + visible) {
        self->top = self->cursor - visible + 1;
    }
}

/**
 * Handles modal mode switching (normal/insert) for non-pasted keys. Returns
 * true when the key was consumed (the caller should stop). May set `*cancel`
 * (Esc in normal mode).
 */
static bool fuzzy_mode_switch(ScFuzzy *self, ScKey key, bool *cancel) {
    if (!self->opts.modal || (key.mods & SC_MOD_PASTED)) {
        return false;
    }
    if (self->insert_mode) {
        if (sc_key_chord_matches(key, resolve_normal_key(self))) {
            self->insert_mode = false;   // Esc: insert → normal
            return true;
        }
        return false;
    }
    // Normal mode.
    if (sc_key_chord_matches(key, resolve_insert_key(self))) {
        self->insert_mode = true;        // i: normal → insert
        return true;
    }
    if (self->opts.clear_key.key != 0
        && sc_key_chord_matches(key, self->opts.clear_key)) {
        clear_query(self);
        self->cursor = 0;
        refilter(self);
        return true;
    }
    if (sc_key_chord_matches(key, resolve_normal_key(self))) {
        *cancel = true;                  // Esc in normal mode cancels
        return true;
    }
    return false;
}

static void fuzzy_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    ScFuzzy *self = state;
    bool pasted = key.mods & SC_MOD_PASTED;

    if (fuzzy_mode_switch(self, key, cancel)) {
        return;
    }

    // Ctrl-N / Ctrl-P move the cursor in both modes (also for the non-modal
    // finder - matches the documented behavior).
    if (!pasted && key.type == SC_KEY_CHAR && (key.mods & SC_MOD_CTRL)
        && (key.codepoint == 'n' || key.codepoint == 'p')) {
        if (key.codepoint == 'n') { cursor_down(self); }
        else                      { cursor_up(self); }
        scroll_to_cursor(self);
        return;
    }

    if (self->multi && !pasted) {
        if (self->opts.toggle_all_key.key != 0
            && sc_key_chord_matches(key, self->opts.toggle_all_key)) {
            toggle_range(self, 0, self->match_n);
            return;
        }
        if (self->opts.toggle_section_key.key != 0
            && sc_key_chord_matches(key, self->opts.toggle_section_key)) {
            toggle_section_displayed(self);
            return;
        }
        // Space toggles the cursor row, except while typing in insert mode.
        if (key.type == SC_KEY_CHAR && key.codepoint == ' '
            && !(self->opts.modal && self->insert_mode)) {
            if (self->selectable_n > 0) {
                Row *r = &self->rows[self->matches[self->cursor].index];
                r->checked = !r->checked;
            }
            return;
        }
    }

    // Normal-mode (command) keys: bare j/k/g/G navigate; other bare characters
    // are ignored (they do not type into the query).
    bool command_mode = self->opts.modal && !self->insert_mode && !pasted;
    if (command_mode && key.type == SC_KEY_CHAR && key.mods == 0) {
        switch (key.codepoint) {
            case 'j': cursor_down(self); break;
            case 'k': cursor_up(self);   break;
            case 'g': {
                size_t f = first_selectable(self);
                if (f != SIZE_MAX) { self->cursor = f; }
                break;
            }
            case 'G': {
                size_t l = last_selectable(self);
                if (l != SIZE_MAX) { self->cursor = l; }
                break;
            }
            default:
                return;   // ignore other characters in normal mode
        }
        scroll_to_cursor(self);
        return;
    }

    switch (key.type) {
        case SC_KEY_UP:
            cursor_up(self);
            break;
        case SC_KEY_DOWN:
            cursor_down(self);
            break;
        case SC_KEY_TAB:
            // With sections, Tab jumps to the next section (cycling); without
            // sections it falls back to moving the cursor down one row.
            if (self->has_sections && self->match_n > 0) {
                section_jump(self, +1);
                return;
            }
            cursor_down(self);
            break;
        case SC_KEY_BACKTAB:
            if (self->has_sections && self->match_n > 0) {
                section_jump(self, -1);
                return;
            }
            cursor_up(self);
            break;
        case SC_KEY_ENTER:
            if (self->selectable_n > 0) {
                *done = true;
            }
            return;
        default:
            if (command_mode) {
                return;   // normal mode: never edit the query
            }
            if (sc_le_handle(&self->query, key)) {
                self->cursor = 0;
                refilter(self);
            }
            return;
    }
    scroll_to_cursor(self);
}

/**
 * qsort comparator honoring `opts.order` (via `g_sort_ctx`):
 *  - SCORE     : score desc, then add-order asc (default).
 *  - INSERTION : add-order (asc, or desc with `order_desc`).
 *  - COLUMN    : `fields[order_column]` case-insensitive, then add-order.
 */
static int match_cmp(const void *a, const void *b) {
    const Match *left = a;
    const Match *right = b;
    const ScFuzzy *self = g_sort_ctx;
    int add_order = (left->index > right->index)
                  - (left->index < right->index);
    ScFuzzyOrder order = self ? self->opts.order : SC_FUZZY_ORDER_SCORE;

    if (order == SC_FUZZY_ORDER_COLUMN && self) {
        size_t col = self->opts.order_column;
        const char *ls = (col < self->rows[left->index].ncols)
            ? self->rows[left->index].fields[col] : "";
        const char *rs = (col < self->rows[right->index].ncols)
            ? self->rows[right->index].fields[col] : "";
        int cmp = strcasecmp(ls, rs);
        if (cmp != 0) {
            return self->opts.order_desc ? -cmp : cmp;
        }
        return add_order;
    }
    if (order == SC_FUZZY_ORDER_INSERTION) {
        return (self && self->opts.order_desc) ? -add_order : add_order;
    }
    if (left->score != right->score) {
        return right->score - left->score;
    }
    return add_order;
}

/** Grows the row array when full. */
static void grow_rows(ScFuzzy *self) {
    if (self->count != self->cap) {
        return;
    }
    size_t cap = self->cap ? self->cap * 2 : 8;
    Row *rows = realloc(self->rows, cap * sizeof *rows);
    if (!rows) { return; }
    self->rows = rows;
    self->cap = cap;
}

/**
 * Deep-copies an `ScText` span-by-span. The source spans hold already-sanitized
 * UTF-8 (the rich cell came from the caller through `sc_text_append`), so the
 * copy goes through the trusted raw path to preserve the bytes exactly.
 */
static ScText *clone_text(const ScText *src) {
    ScText *dst = sc_text_new();
    if (!dst) {
        return NULL;
    }
    size_t n = sc_text_span_count(src);
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(src, i);
        if (span.raw_str) {
            sc_text_append_raw(dst, span.raw_str, span.style);
        }
    }
    return dst;
}

/**
 * Flattens an `ScText` to a plain heap string (concatenated span text), used as
 * the match/display key for a rich cell. Caller frees.
 */
static char *flatten_text(const ScText *src) {
    size_t total = 0;
    size_t n = sc_text_span_count(src);
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(src, i);
        if (span.raw_str) {
            total += strlen(span.raw_str);
        }
    }
    char *out = malloc(total + 1);
    if (!out) {
        return NULL;
    }
    size_t pos = 0;
    for (size_t i = 0; i < n; i++) {
        ScSpan span = sc_text_span(src, i);
        if (span.raw_str) {
            size_t len = strlen(span.raw_str);
            memcpy(out + pos, span.raw_str, len);
            pos += len;
        }
    }
    out[pos] = '\0';
    return out;
}

/** Frees the owned buffers of a single row (fields, styles, rich texts). */
static void free_row(Row *row) {
    for (size_t c = 0; c < row->ncols; c++) {
        free(row->fields[c]);
        if (row->texts && row->texts[c]) {
            sc_text_free(row->texts[c]);
        }
    }
    free(row->fields);
    free(row->styles);
    free(row->texts);
}

/** Byte length of the UTF-8 codepoint led by `lead`. */
static size_t cp_len(unsigned char lead) {
    if ((lead & 0x80) == 0x00) {
        return 1;
    }
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    return 4;
}
