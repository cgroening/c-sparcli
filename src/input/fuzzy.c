#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** One ranked candidate: its original add-order index and match score. */
typedef struct Match {
    size_t index;
    int score;
} Match;

/** Incremental fuzzy finder over a set of (multi-field) rows. */
struct ScFuzzy {
    char ***rows;          /**< rows[i][c] = field c of row i. */
    size_t *row_ncols;
    size_t count;
    size_t cap;
    ScFuzzyOpts opts;
    ScColor accent;
    int max_visible;

    /* Runtime state for the active run. */
    ScLineEditor query;
    Match *matches;        /**< Filtered + ranked; capacity `count`. */
    size_t match_n;
    size_t cursor;         /**< Index into `matches`. */
    size_t top;            /**< First visible match (scroll offset). */
};

static const char *const DEFAULT_HINT =
    "type to filter \xc2\xb7 \xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 "
    "enter select \xc2\xb7 esc cancel";


static void copy_opts_strings(ScFuzzy *self);
static void free_opts_strings(ScFuzzy *self);
static void refilter(ScFuzzy *self);
    static bool row_matches(const ScFuzzy *self, size_t i, const char *query,
                            int *best);
static ScRendered *fuzzy_render(void *state);
    static ScRendered *render_query_line(ScFuzzy *self);
    static ScRendered *render_list(ScFuzzy *self);
        static void append_highlighted(
            ScText *text, const char *label, const char *query,
            ScTextStyle base_style, ScColor accent);
    static ScRendered *render_table(ScFuzzy *self);
        static ScTableData *fuzzy_table_columns(ScFuzzy *self);
        static ScTableOpts resolve_fuzzy_table_opts(const ScFuzzy *self);
    static ScRendered *render_scroll_hint(ScFuzzy *self);
static void fuzzy_on_key(void *state, ScKey key, bool *done, bool *cancel);
static int match_cmp(const void *a, const void *b);
static void grow_rows(ScFuzzy *self);
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
    self->accent = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN;
    self->max_visible = opts.max_visible > 0 ? opts.max_visible : 10;
    return self;
}

void sc_fuzzy_add(ScFuzzy *self, const char *label) {
    const char *one[1] = { label };
    sc_fuzzy_add_row(self, one, 1);
}

void sc_fuzzy_add_row(ScFuzzy *self, const char *const *fields, size_t n) {
    if (!self || n == 0) {
        return;
    }
    grow_rows(self);
    if (self->count == self->cap) {
        return;   // grow failed (OOM): no slot, so don't write out of bounds
    }
    // calloc: checks the count * size multiplication for overflow
    char **row = calloc(n, sizeof *row);
    if (!row) {
        return;
    }
    for (size_t c = 0; c < n; c++) {
        row[c] = sc_dup_str(fields[c]);
    }
    self->rows[self->count] = row;
    self->row_ncols[self->count] = n;
    self->count++;
}

void sc_fuzzy_free(ScFuzzy *self) {
    if (!self) {
        return;
    }
    for (size_t i = 0; i < self->count; i++) {
        for (size_t c = 0; c < self->row_ncols[i]; c++) {
            free(self->rows[i][c]);
        }
        free(self->rows[i]);
    }
    free(self->rows);
    free(self->row_ncols);
    free(self->matches);
    free_opts_strings(self);
    free(self);
}

ScInputStatus sc_fuzzy_run(ScFuzzy *self, size_t *out_index) {
    if (!self || !out_index || self->count == 0) {
        return SC_INPUT_ERROR;
    }

    sc_le_init(&self->query, NULL);
    if (!self->query.buf) {
        return SC_INPUT_ERROR;
    }
    self->matches = calloc(self->count, sizeof *self->matches);
    if (!self->matches) {
        sc_le_free(&self->query);
        return SC_INPUT_ERROR;
    }
    self->cursor = 0;
    self->top = 0;
    refilter(self);

    ScPromptVTable vtable = {
        .render = fuzzy_render,
        .on_key = fuzzy_on_key,
        .paste = SC_PASTE_TEXT,
    };
    ScPromptShortcuts sk = {
        self->opts.shortcuts, self->opts.n_shortcuts, self->opts.out_shortcut_id
    };
    ScInputStatus status =
        sc_prompt_run(&vtable, self, self->opts.shortcuts ? &sk : NULL, NULL);

    if (status == SC_INPUT_OK && self->match_n > 0) {
        // Enter requires a match; a RETURN shortcut can fire with an empty
        // result set, so guard the dereference and report index 0 in that case.
        size_t row_index = self->matches[self->cursor].index;
        *out_index = row_index;
        if (!self->opts.hide_summary) {
            const char *prompt =
                self->opts.prompt ? self->opts.prompt : "Search";
            size_t size = strlen(prompt) + strlen(self->rows[row_index][0]) + 4;
            char *line = malloc(size);
            if (line) {
                snprintf(line, size, "? %s %s", prompt,
                         self->rows[row_index][0]);
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

size_t sc_fuzzy_cursor_index(const ScFuzzy *self) {
    if (!self || !self->matches || self->match_n == 0) {
        return 0;
    }
    return self->matches[self->cursor].index;
}

void sc_fuzzy_remove(ScFuzzy *self, size_t index) {
    if (!self || index >= self->count) {
        return;
    }
    for (size_t c = 0; c < self->row_ncols[index]; c++) {
        free(self->rows[index][c]);
    }
    free(self->rows[index]);
    size_t tail = self->count - index - 1;
    memmove(&self->rows[index], &self->rows[index + 1],
            tail * sizeof *self->rows);
    memmove(&self->row_ncols[index], &self->row_ncols[index + 1],
            tail * sizeof *self->row_ncols);
    self->count--;
    // Rebuild the filtered/ranked view against the shrunk set and re-clamp the
    // cursor. `matches` is only allocated during a run (when callbacks fire).
    if (self->matches) {
        refilter(self);
    }
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
    for (size_t c = 0; c < self->row_ncols[i]; c++) {
        if (mask != 0 && !(mask & ((uint64_t)1 << c))) {
            continue;
        }
        int score = 0;
        if (sc_fuzzy_match(query, self->rows[i][c], &score)) {
            matched = true;
            if (score > best_score) {
                best_score = score;
            }
        }
    }
    *best = best_score;
    return matched;
}

/** Recomputes the filtered/ranked match list from the current query. */
static void refilter(ScFuzzy *self) {
    const char *query = self->query.buf;
    self->match_n = 0;
    for (size_t i = 0; i < self->count; i++) {
        int score = 0;
        if (row_matches(self, i, query, &score)) {
            self->matches[self->match_n].index = i;
            self->matches[self->match_n].score = score;
            self->match_n++;
        }
    }
    qsort(self->matches, self->match_n, sizeof *self->matches, match_cmp);
    if (self->cursor >= self->match_n) {
        self->cursor = self->match_n ? self->match_n - 1 : 0;
    }
    size_t visible = (size_t)self->max_visible;
    if (self->cursor < self->top) {
        self->top = self->cursor;
    } else if (self->cursor >= self->top + visible) {
        self->top = self->cursor - visible + 1;
    }
    if (self->match_n <= visible) {
        self->top = 0;
    }
}

static ScRendered *fuzzy_render(void *state) {
    ScFuzzy *self = state;
    ScRendered *query = render_query_line(self);
    ScRendered *body =
        self->opts.table ? render_table(self) : render_list(self);
    if (!query || !body) {
        sc_rendered_free(query);
        sc_rendered_free(body);
        return NULL;
    }
    ScRendered *scroll = render_scroll_hint(self);

    const ScRendered *parts[3];
    size_t count = 0;
    parts[count++] = query;
    parts[count++] = body;
    if (scroll) {
        parts[count++] = scroll;
    }
    ScRendered *stacked = sc_vstack(parts, count, 0);
    sc_rendered_free(query);
    sc_rendered_free(body);
    sc_rendered_free(scroll);
    if (!stacked) {
        return NULL;
    }
    return sc_compose_hint(stacked,
                           self->opts.hint ? self->opts.hint : DEFAULT_HINT,
                           self->opts.hint_layout, self->opts.hint_pos,
                           self->opts.hint_style);
}

/** Builds the query/prompt line (query field scrolls when long). */
static ScRendered *render_query_line(ScFuzzy *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    const char *prompt = self->opts.prompt ? self->opts.prompt : "Search";
    ScTextStyle prompt_style = sc_style_set(self->opts.prompt_style)
        ? self->opts.prompt_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    sc_prompt_append(text, prompt, prompt_style,
                     self->opts.prompt_markup, self->opts.prompt_text);
    sc_text_append(text, " ", (ScTextStyle){ 0 });

    char counter[48];
    snprintf(
        counter, sizeof counter, "  (%zu/%zu)", self->match_n, self->count
    );

    // Query field = line width − prompt − space − counter.
    int prompt_w = (int)sc_utf8_string_length(prompt, strlen(prompt));
    int counter_w = (int)sc_utf8_string_length(counter, strlen(counter));
    int field = sc_terminal_width() - prompt_w - 1 - counter_w;
    if (field < 1) {
        field = 1;
    }
    sc_le_render_into(&self->query, text, (ScTextStyle){ 0 },
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

/** List view: one row per match, matched characters emphasized. */
static ScRendered *render_list(ScFuzzy *self) {
    ScText *text = sc_text_new();
    if (!text) {
        return NULL;
    }
    size_t visible = (size_t)self->max_visible;
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }

    ScTextStyle selected_style = sc_style_set(self->opts.selected_style)
        ? self->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, self->accent, SC_ANSI_COLOR_NONE };
    const char *cursor_marker = self->opts.cursor_marker
        ? self->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *marker = self->opts.marker ? self->opts.marker : "  ";

    for (size_t i = self->top; i < end; i++) {
        bool on_cursor = (i == self->cursor);
        ScTextStyle row = on_cursor ? selected_style : (ScTextStyle){ 0 };
        sc_text_append(text, on_cursor ? cursor_marker : marker, row);
        append_highlighted(text, self->rows[self->matches[i].index][0],
                           self->query.buf, row, self->accent);
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
static void append_highlighted(ScText *text, const char *label,
                               const char *query, ScTextStyle base_style,
                               ScColor accent) {
    const char *pattern = (query && query[0]) ? query : NULL;
    const char *s = label;
    while (*s) {
        size_t len = cp_len((unsigned char)*s);
        char glyph[8];
        memcpy(glyph, s, len);
        glyph[len] = '\0';

        bool hit = pattern && *pattern
            && tolower((unsigned char)*pattern) == tolower((unsigned char)*s);
        ScTextStyle style = base_style;
        if (hit) {
            style.attr |= SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER;
            if (base_style.fg.index == 0) {
                style.fg = accent;
            }
            pattern++;
        }
        sc_text_append(text, glyph, style);
        s += len;
    }
}

/** Table view: visible matches as a sparcli table, cursor row via row-bg. */
static ScRendered *render_table(ScFuzzy *self) {
    ScTableData *table = fuzzy_table_columns(self);
    if (!table) {
        return NULL;
    }
    size_t cols = self->opts.n_cols ? self->opts.n_cols : 1;

    size_t visible = (size_t)self->max_visible;
    size_t end = self->top + visible;
    if (end > self->match_n) {
        end = self->match_n;
    }

    // Cells with matched characters are built as ScText (highlighted) and
    // borrowed by the table, so they must outlive the capture below.
    const char *query = self->query.buf;
    uint64_t mask = self->opts.search_columns;   /* 0 = all columns */
    ScTextStyle sel = self->opts.selected_style; /* cursor-row text style */
    bool sel_set = sc_style_set(sel);
    // calloc(0, ...) is implementation-defined; with no visible rows the
    // loop below doesn't run, so skip the allocation entirely.
    size_t shown = (end > self->top) ? end - self->top : 0;
    ScText **texts = shown > 0 ? calloc(shown * cols, sizeof *texts) : NULL;
    size_t n_texts = 0;

    for (size_t i = self->top; i < end; i++) {
        size_t row_index = self->matches[i].index;
        bool on_cursor = (i == self->cursor);
        ScCell *cells = calloc(cols, sizeof *cells);
        if (!cells) {
            break;
        }
        for (size_t c = 0; c < cols; c++) {
            const char *value = (c < self->row_ncols[row_index])
                ? self->rows[row_index][c] : "";
            bool searched = (mask == 0) || (mask & ((uint64_t)1 << c));
            bool hl = texts && searched && query[0]
                    && sc_fuzzy_match(query, value, NULL);
            if (texts && (hl || (on_cursor && sel_set))) {
                // Highlight matched characters and/or apply the cursor-row text
                // style. On the cursor row the accent fg is dropped (accent is
                // the row background); selected_style provides the text colour.
                ScText *t = sc_text_new();
                if (t) {
                    ScTextStyle base = on_cursor ? sel : (ScTextStyle){ 0 };
                    if (hl) {
                        append_highlighted(t, value, query, base,
                            on_cursor ? SC_ANSI_COLOR_NONE : self->accent);
                    } else {
                        sc_text_append(t, value, base);
                    }
                    texts[n_texts++] = t;
                    cells[c] = sc_cell_t(t);
                } else {
                    cells[c] = sc_cell(value);
                }
            } else {
                cells[c] = sc_cell(value);
            }
        }
        if (on_cursor) {
            sc_table_add_row_bg(table, cells, cols, self->accent);
        } else {
            sc_table_add_row(table, cells, cols);
        }
        free(cells);
    }

    ScRendered *rendered =
        sc_capture_table(table, resolve_fuzzy_table_opts(self));
    sc_table_free(table);
    for (size_t t = 0; t < n_texts; t++) {
        sc_text_free(texts[t]);
    }
    free(texts);
    return rendered;
}

/** Builds the table with one (word-wrap-off) column per configured field. */
static ScTableData *fuzzy_table_columns(ScFuzzy *self) {
    ScTableData *table = sc_table_new();
    if (!table) {
        return NULL;
    }
    size_t cols = self->opts.n_cols ? self->opts.n_cols : 1;
    for (size_t c = 0; c < cols; c++) {
        const char *header = (self->opts.headers && c < self->opts.n_cols)
            ? self->opts.headers[c] : "";
        sc_table_add_column(table, header, (ScColOpts){ .word_wrap = false });
    }
    return table;
}

/**
 * Resolves the caller's `table_opts`, filling the fuzzy defaults (single
 * border, bold header row) only where a field was left zero-init.
 */
static ScTableOpts resolve_fuzzy_table_opts(const ScFuzzy *self) {
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
    return opts;
}

/** Builds a dim "↑ first-last/total ↓" line, or NULL when nothing is hidden. */
static ScRendered *render_scroll_hint(ScFuzzy *self) {
    size_t visible = (size_t)self->max_visible;
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

static void fuzzy_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ScFuzzy *self = state;
    switch (key.type) {
        case SC_KEY_UP:
        case SC_KEY_BACKTAB:
            if (self->cursor > 0) {
                self->cursor--;
            }
            break;
        case SC_KEY_DOWN:
        case SC_KEY_TAB:
            if (self->cursor + 1 < self->match_n) {
                self->cursor++;
            }
            break;
        case SC_KEY_ENTER:
            if (self->match_n > 0) {
                *done = true;
            }
            return;
        default:
            if (sc_le_handle(&self->query, key)) {
                self->cursor = 0;
                refilter(self);
            }
            return;
    }
    size_t visible = (size_t)self->max_visible;
    if (self->cursor < self->top) {
        self->top = self->cursor;
    } else if (self->cursor >= self->top + visible) {
        self->top = self->cursor - visible + 1;
    }
}

/** qsort comparator: score descending, then add-order ascending. */
static int match_cmp(const void *a, const void *b) {
    const Match *left = a;
    const Match *right = b;
    if (left->score != right->score) {
        return right->score - left->score;
    }
    return (left->index > right->index) - (left->index < right->index);
}

/** Grows the row arrays when full. */
static void grow_rows(ScFuzzy *self) {
    if (self->count != self->cap) {
        return;
    }
    size_t cap = self->cap ? self->cap * 2 : 8;
    // Commit each realloc to `self` immediately on success. A naive
    // "realloc both, free on either failure" leaves `self->rows` dangling
    // when the first grew (and was thus already freed/moved) but the second
    // failed. `self->cap` is bumped only after both succeed, so a partial
    // grow just leaves harmless spare capacity and is retried next time.
    char ***rows = realloc(self->rows, cap * sizeof *rows);
    if (!rows) { return; }
    self->rows = rows;
    size_t *ncols = realloc(self->row_ncols, cap * sizeof *ncols);
    if (!ncols) { return; }
    self->row_ncols = ncols;
    self->cap = cap;
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
