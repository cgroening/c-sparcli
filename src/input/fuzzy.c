#include "input_internal.h"
#include "internal.h"   /* sc_terminal_width, sc_utf8_string_length */

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct { size_t idx; int score; } Match;

struct ScFuzzy {
    char       ***rows;       /* rows[i][c] = field c of row i */
    size_t       *row_ncols;
    size_t        count;
    size_t        cap;
    ScFuzzyOpts   opts;
    ScColor       accent;
    int           max_visible;

    /* runtime */
    ScLineEditor  query;
    Match        *matches;    /* filtered + ranked; size `count` */
    size_t        match_n;
    size_t        cursor;     /* index into matches */
    size_t        top;
};


static char *dup_str(const char *s) {
    size_t n = strlen(s ? s : "");
    char  *p = malloc(n + 1);
    if (p) { memcpy(p, s ? s : "", n + 1); }
    return p;
}

bool sc_fuzzy_match(const char *pattern, const char *str, int *score) {
    if (!pattern || !pattern[0]) { if (score) { *score = 0; } return true; }
    if (!str) { if (score) { *score = 0; } return false; }

    const char *p = pattern, *s = str;
    int total = 0, streak = 0;
    while (*p && *s) {
        if (tolower((unsigned char)*p) == tolower((unsigned char)*s)) {
            total += 1 + streak;
            if (s == str || s[-1] == ' ' || s[-1] == '_' || s[-1] == '-') {
                total += 2;  /* word-start bonus */
            }
            streak++;
            p++;
        } else {
            streak = 0;
        }
        s++;
    }
    bool ok = (*p == '\0');
    if (score) { *score = ok ? total : 0; }
    return ok;
}

ScFuzzy *sc_fuzzy_new(ScFuzzyOpts opts) {
    sc_theme_apply_fuzzy(&opts);
    ScFuzzy *f = calloc(1, sizeof *f);
    if (!f) { return NULL; }
    f->opts        = opts;
    f->accent      = opts.accent.index ? opts.accent : SC_ANSI_COLOR_CYAN;
    f->max_visible = opts.max_visible > 0 ? opts.max_visible : 10;
    return f;
}

static void grow_rows(ScFuzzy *f) {
    if (f->count != f->cap) { return; }
    size_t cap = f->cap ? f->cap * 2 : 8;
    char ***r  = realloc(f->rows, cap * sizeof *r);
    size_t *nc = realloc(f->row_ncols, cap * sizeof *nc);
    if (!r || !nc) { free(r); free(nc); return; }
    f->rows = r; f->row_ncols = nc; f->cap = cap;
}

void sc_fuzzy_add_row(ScFuzzy *f, const char *const *fields, size_t n) {
    if (!f || n == 0) { return; }
    grow_rows(f);
    char **row = malloc(n * sizeof *row);
    if (!row) { return; }
    for (size_t c = 0; c < n; c++) { row[c] = dup_str(fields[c]); }
    f->rows[f->count]      = row;
    f->row_ncols[f->count] = n;
    f->count++;
}

void sc_fuzzy_add(ScFuzzy *f, const char *label) {
    const char *one[1] = { label };
    sc_fuzzy_add_row(f, one, 1);
}

void sc_fuzzy_free(ScFuzzy *f) {
    if (!f) { return; }
    for (size_t i = 0; i < f->count; i++) {
        for (size_t c = 0; c < f->row_ncols[i]; c++) { free(f->rows[i][c]); }
        free(f->rows[i]);
    }
    free(f->rows);
    free(f->row_ncols);
    free(f->matches);
    free(f);
}

static int match_cmp(const void *a, const void *b) {
    const Match *x = a, *y = b;
    if (x->score != y->score) { return y->score - x->score; }  /* desc */
    return (x->idx > y->idx) - (x->idx < y->idx);              /* asc  */
}

/** Recomputes the filtered/ranked match list from the current query. */
static void refilter(ScFuzzy *f) {
    const char *q = f->query.buf;
    f->match_n = 0;
    for (size_t i = 0; i < f->count; i++) {
        int score = 0;
        if (sc_fuzzy_match(q, f->rows[i][0], &score)) {
            f->matches[f->match_n].idx   = i;
            f->matches[f->match_n].score = score;
            f->match_n++;
        }
    }
    qsort(f->matches, f->match_n, sizeof *f->matches, match_cmp);
    if (f->cursor >= f->match_n) { f->cursor = f->match_n ? f->match_n - 1 : 0; }
    size_t visible = (size_t)f->max_visible;
    if (f->cursor < f->top)                { f->top = f->cursor; }
    else if (f->cursor >= f->top + visible) { f->top = f->cursor - visible + 1; }
    if (f->match_n <= visible) { f->top = 0; }
}

/** Builds the query/prompt line as a captured rendering. */
static ScRendered *render_query_line(ScFuzzy *f) {
    ScText *t = sc_text_new();
    if (!t) { return NULL; }
    const char *prompt = f->opts.prompt ? f->opts.prompt : "Search";
    ScTextStyle ps = sc_style_set(f->opts.prompt_style)
        ? f->opts.prompt_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, f->accent, SC_ANSI_COLOR_NONE };
    sc_text_append(t, prompt, ps);
    sc_text_append(t, " ", (ScTextStyle){ 0 });

    char counter[48];
    snprintf(counter, sizeof counter, "  (%zu/%zu)", f->match_n, f->count);

    /* Query field = line width − prompt − space − counter, so a long query
     * scrolls horizontally instead of overflowing the line. */
    int prompt_w  = (int)sc_utf8_string_length(prompt, strlen(prompt));
    int counter_w = (int)sc_utf8_string_length(counter, strlen(counter));
    int field = sc_terminal_width() - prompt_w - 1 - counter_w;
    if (field < 1) { field = 1; }
    sc_le_render_into(&f->query, t, (ScTextStyle){ 0 }, f->opts.cursor_style,
                      NULL, NULL, (ScTextStyle){ 0 }, field);

    ScTextStyle cst = sc_style_set(f->opts.counter_style)
        ? f->opts.counter_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_text_append(t, counter, cst);

    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

/** Byte length of the UTF-8 codepoint led by `c`. */
static size_t cp_len(unsigned char c) {
    if ((c & 0x80) == 0x00) { return 1; }
    if ((c & 0xE0) == 0xC0) { return 2; }
    if ((c & 0xF0) == 0xE0) { return 3; }
    return 4;
}

/**
 * Appends `label` codepoint-by-codepoint, emphasizing the characters that the
 * (greedy, case-insensitive) query subsequence matches — the same matching
 * order as `sc_fuzzy_match`.
 */
static void append_highlighted(
    ScText *t, const char *label, const char *query,
    ScTextStyle base, ScColor accent
) {
    const char *p = (query && query[0]) ? query : NULL;
    const char *s = label;
    while (*s) {
        size_t n = cp_len((unsigned char)*s);
        char g[8];
        memcpy(g, s, n);
        g[n] = '\0';

        bool hit = p && *p
            && tolower((unsigned char)*p) == tolower((unsigned char)*s);
        ScTextStyle st = base;
        if (hit) {
            st.attr |= SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER;
            if (base.fg.index == 0) { st.fg = accent; }
            p++;
        }
        sc_text_append(t, g, st);
        s += n;
    }
}

/** Builds a dim "↑ first–last/total ↓" line, or NULL when nothing is hidden. */
static ScRendered *render_scroll_hint(ScFuzzy *f) {
    size_t visible = (size_t)f->max_visible;
    size_t end = f->top + visible;
    if (end > f->match_n) { end = f->match_n; }
    if (f->top == 0 && end >= f->match_n) { return NULL; }

    char buf[80];
    snprintf(buf, sizeof buf, "  %s %zu\xe2\x80\x93%zu/%zu %s",
             f->top > 0       ? "\xe2\x86\x91" : " ",
             f->top + 1, end, f->match_n,
             end < f->match_n ? "\xe2\x86\x93" : " ");
    ScText *t = sc_text_new();
    if (!t) { return NULL; }
    sc_text_append(t, buf,
        (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static ScRendered *render_list(ScFuzzy *f) {
    ScText *t = sc_text_new();
    if (!t) { return NULL; }
    size_t visible = (size_t)f->max_visible;
    size_t end = f->top + visible;
    if (end > f->match_n) { end = f->match_n; }

    ScTextStyle sel = sc_style_set(f->opts.selected_style)
        ? f->opts.selected_style
        : (ScTextStyle){ SC_TEXT_ATTR_BOLD, f->accent, SC_ANSI_COLOR_NONE };
    const char *cur_mk = f->opts.cursor_marker ? f->opts.cursor_marker : "\xe2\x80\xa3 ";
    const char *mk     = f->opts.marker        ? f->opts.marker        : "  ";

    for (size_t k = f->top; k < end; k++) {
        bool cur = (k == f->cursor);
        ScTextStyle row = cur ? sel : (ScTextStyle){ 0 };
        sc_text_append(t, cur ? cur_mk : mk, row);
        append_highlighted(t, f->rows[f->matches[k].idx][0], f->query.buf,
                           row, f->accent);
        if (k + 1 < end) { sc_text_append(t, "\n", (ScTextStyle){ 0 }); }
    }
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static ScRendered *render_table(ScFuzzy *f) {
    ScTableData *tbl = sc_table_new();
    if (!tbl) { return NULL; }
    size_t cols = f->opts.n_cols ? f->opts.n_cols : 1;
    for (size_t c = 0; c < cols; c++) {
        const char *h = (f->opts.headers && c < f->opts.n_cols)
                      ? f->opts.headers[c] : "";
        sc_table_add_column(tbl, h, (ScColOpts){ .word_wrap = false });
    }

    size_t visible = (size_t)f->max_visible;
    size_t end = f->top + visible;
    if (end > f->match_n) { end = f->match_n; }

    for (size_t k = f->top; k < end; k++) {
        size_t ri = f->matches[k].idx;
        ScCell *cells = calloc(cols, sizeof *cells);
        if (!cells) { break; }
        for (size_t c = 0; c < cols; c++) {
            const char *v = (c < f->row_ncols[ri]) ? f->rows[ri][c] : "";
            cells[c] = sc_cell(v);
        }
        if (k == f->cursor) {
            sc_table_add_row_bg(tbl, cells, cols, f->accent);
        } else {
            sc_table_add_row(tbl, cells, cols);
        }
        free(cells);
    }

    /* Start from the caller's table_opts; fill the original defaults only for
     * the fields left zero-init, so callers can override border/header etc. */
    ScTableOpts opts = f->opts.table_opts;
    if (opts.border.type == SC_BORDER_NONE) { opts.border.type = SC_BORDER_SINGLE; }
    if (!opts.header.row) {
        opts.header.row = true;
        if (!sc_style_set(opts.header.style)) {
            opts.header.style = (ScTextStyle){ SC_TEXT_ATTR_BOLD,
                SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
        }
    }
    ScRendered *r = sc_capture_table(tbl, opts);
    sc_table_free(tbl);
    return r;
}

/** Builds the key-hint footer line, or NULL when suppressed. */
static ScRendered *render_hint_footer(ScFuzzy *f) {
    if (f->opts.hide_hint) { return NULL; }
    const char *hint = f->opts.hint ? f->opts.hint
        : "type to filter \xc2\xb7 \xe2\x86\x91/\xe2\x86\x93 move \xc2\xb7 enter select \xc2\xb7 esc cancel";
    ScText *t = sc_text_new();
    if (!t) { return NULL; }
    ScTextStyle st = sc_style_set(f->opts.hint_style)
        ? f->opts.hint_style
        : (ScTextStyle){ SC_TEXT_ATTR_DIM, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    sc_text_append(t, hint, st);
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}

static ScRendered *fuzzy_render(void *state) {
    ScFuzzy *f = state;
    ScRendered *query = render_query_line(f);
    ScRendered *body  = (f->opts.table) ? render_table(f) : render_list(f);
    if (!query || !body) { sc_rendered_free(query); sc_rendered_free(body); return NULL; }
    ScRendered *scroll = render_scroll_hint(f);
    ScRendered *foot   = render_hint_footer(f);

    const ScRendered *parts[4];
    size_t n = 0;
    parts[n++] = query;
    parts[n++] = body;
    if (scroll) { parts[n++] = scroll; }
    if (foot)   { parts[n++] = foot; }
    ScRendered *stacked = sc_vstack(parts, n, 0);
    sc_rendered_free(query);
    sc_rendered_free(body);
    sc_rendered_free(scroll);
    sc_rendered_free(foot);
    return stacked;
}

static void fuzzy_on_key(void *state, ScKey key, bool *done, bool *cancel) {
    (void)cancel;
    ScFuzzy *f = state;
    switch (key.type) {
        case SC_KEY_UP:
        case SC_KEY_BACKTAB:
            if (f->cursor > 0) { f->cursor--; }
            break;
        case SC_KEY_DOWN:
        case SC_KEY_TAB:
            if (f->cursor + 1 < f->match_n) { f->cursor++; }
            break;
        case SC_KEY_ENTER:
            if (f->match_n > 0) { *done = true; }
            return;
        default:
            if (sc_le_handle(&f->query, key)) { f->cursor = 0; refilter(f); }
            return;
    }
    size_t visible = (size_t)f->max_visible;
    if (f->cursor < f->top)                { f->top = f->cursor; }
    else if (f->cursor >= f->top + visible) { f->top = f->cursor - visible + 1; }
}

ScInputStatus sc_fuzzy_run(ScFuzzy *f, size_t *out_index) {
    if (!f || !out_index || f->count == 0) { return SC_INPUT_ERROR; }

    sc_le_init(&f->query, NULL);
    if (!f->query.buf) { return SC_INPUT_ERROR; }
    f->matches = malloc(f->count * sizeof *f->matches);
    if (!f->matches) { sc_le_free(&f->query); return SC_INPUT_ERROR; }
    f->cursor = 0; f->top = 0;
    refilter(f);

    ScPromptVTable vt = { .render = fuzzy_render, .on_key = fuzzy_on_key };
    ScInputStatus status = sc_prompt_run(&vt, f);

    if (status == SC_INPUT_OK) {
        size_t ri = f->matches[f->cursor].idx;
        *out_index = ri;
        if (!f->opts.hide_summary) {
            const char *prompt = f->opts.prompt ? f->opts.prompt : "Search";
            size_t n = strlen(prompt) + strlen(f->rows[ri][0]) + 4;
            char  *line = malloc(n);
            if (line) {
                snprintf(line, n, "? %s %s", prompt, f->rows[ri][0]);
                sc_println(line, f->opts.summary_style);
                free(line);
            }
        }
    }
    sc_le_free(&f->query);
    return status;
}

ScRendered *sc_fuzzy_frame(ScFuzzy *f, const char *query) {
    if (!f || f->count == 0) { return NULL; }
    sc_le_init(&f->query, query);
    f->matches = malloc(f->count * sizeof *f->matches);
    if (!f->query.buf || !f->matches) {
        sc_le_free(&f->query);
        free(f->matches); f->matches = NULL;
        return NULL;
    }
    f->cursor = 0; f->top = 0;
    refilter(f);
    ScRendered *r = fuzzy_render(f);
    sc_le_free(&f->query);
    free(f->matches); f->matches = NULL;
    return r;
}
