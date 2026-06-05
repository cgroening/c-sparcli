#include "sparcli.h"
#include "view/sparcli_markdown_render.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>


/* ── resolved render context ─────────────────────────────────────────────── */

typedef struct {
    int     width;
    ScColor heading;
    ScColor code;
    ScColor link;
    bool    quote_dim;   /* quote uses DIM attribute (default) */
    ScColor quote;       /* explicit quote color when set */
} Ctx;

static ScColor or_color(ScColor c, ScColor def) {
    return c.index != 0 ? c : def;
}


/* ── part accumulator (composed via sc_vstack) ───────────────────────────── */

typedef struct { ScRendered **items; size_t n, cap; } Parts;

static void parts_push(Parts *p, ScRendered *r) {
    if (!r) { return; }
    if (p->n == p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 16;
        ScRendered **g = realloc(p->items, nc * sizeof *g);
        if (!g) { sc_rendered_free(r); return; }
        p->items = g; p->cap = nc;
    }
    p->items[p->n++] = r;
}


/* ── small helpers ───────────────────────────────────────────────────────── */

/** Returns a pointer past leading spaces. */
static const char *skip_ws(const char *s) {
    while (*s == ' ' || *s == '\t') { s++; }
    return s;
}

/** Trims trailing whitespace in place; returns `s`. */
static char *rtrim(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\r')) {
        s[--n] = '\0';
    }
    return s;
}


/* ── inline emphasis parser ──────────────────────────────────────────────── */

/** Flushes the pending plain run as one span. */
static void flush_plain(ScText *t, char *buf, size_t *len, ScTextStyle base) {
    if (*len == 0) { return; }
    buf[*len] = '\0';
    sc_text_append(t, buf, base);
    *len = 0;
}

/**
 * Parses inline markdown emphasis in `s` into spans appended to `t`,
 * starting from the base style `base` (so block quotes can pass a dim base).
 * Handles `` `code` ``, `**bold**`/`__bold__`, `*italic*`/`_italic_` and
 * `[text](url)` links.
 */
static void append_inline(ScText *t, const char *s, ScTextStyle base,
                          const Ctx *c) {
    size_t cap = strlen(s) + 1;
    char *buf = malloc(cap);
    if (!buf) { return; }
    size_t len = 0;

    for (size_t i = 0; s[i];) {
        char ch = s[i];

        /* inline code */
        if (ch == '`') {
            const char *close = strchr(s + i + 1, '`');
            if (close) {
                flush_plain(t, buf, &len, base);
                size_t n = (size_t)(close - (s + i + 1));
                char *code = malloc(n + 1);
                if (code) {
                    memcpy(code, s + i + 1, n); code[n] = '\0';
                    ScTextStyle st = base;
                    st.fg = c->code;
                    sc_text_append(t, code, st);
                    free(code);
                }
                i = (size_t)(close - s) + 1;
                continue;
            }
        }

        /* emphasis */
        if (ch == '*' || ch == '_') {
            int run = (s[i + 1] == ch) ? 2 : 1;
            /* find a matching closing run */
            const char *p = s + i + run;
            const char *found = NULL;
            while (*p) {
                if (p[0] == ch && (run == 1 || p[1] == ch)) { found = p; break; }
                p++;
            }
            if (found && found > s + i + run) {
                flush_plain(t, buf, &len, base);
                size_t n = (size_t)(found - (s + i + run));
                char *inner = malloc(n + 1);
                if (inner) {
                    memcpy(inner, s + i + run, n); inner[n] = '\0';
                    ScTextStyle st = base;
                    st.attr |= (run == 2) ? SC_TEXT_ATTR_BOLD
                                          : SC_TEXT_ATTR_ITALIC;
                    sc_text_append(t, inner, st);
                    free(inner);
                }
                i = (size_t)(found - s) + (size_t)run;
                continue;
            }
        }

        /* link [text](url) */
        if (ch == '[') {
            const char *rb = strchr(s + i + 1, ']');
            if (rb && rb[1] == '(') {
                const char *rp = strchr(rb + 2, ')');
                if (rp) {
                    flush_plain(t, buf, &len, base);
                    size_t tn = (size_t)(rb - (s + i + 1));
                    size_t un = (size_t)(rp - (rb + 2));
                    char *txt = malloc(tn + 1);
                    char *url = malloc(un + 1);
                    if (txt && url) {
                        memcpy(txt, s + i + 1, tn); txt[tn] = '\0';
                        memcpy(url, rb + 2, un); url[un] = '\0';
                        ScTextStyle st = base;
                        st.attr |= SC_TEXT_ATTR_UNDER;
                        st.fg = c->link;
                        sc_text_append_link(t, txt, url, st);
                    }
                    free(txt); free(url);
                    i = (size_t)(rp - s) + 1;
                    continue;
                }
            }
        }

        buf[len++] = ch;
        i++;
    }
    flush_plain(t, buf, &len, base);
    free(buf);
}


/* ── block detectors ─────────────────────────────────────────────────────── */

static bool is_blank(const char *s) { return *skip_ws(s) == '\0'; }

static bool is_fence(const char *s) {
    const char *p = skip_ws(s);
    return strncmp(p, "```", 3) == 0 || strncmp(p, "~~~", 3) == 0;
}

static bool is_hr(const char *s) {
    const char *p = skip_ws(s);
    char ch = *p;
    if (ch != '-' && ch != '*' && ch != '_') { return false; }
    int count = 0;
    for (; *p; p++) {
        if (*p == ch) { count++; }
        else if (*p != ' ') { return false; }
    }
    return count >= 3;
}

static bool is_quote(const char *s) { return *skip_ws(s) == '>'; }

static bool is_bullet(const char *s, const char **rest) {
    const char *p = skip_ws(s);
    if ((*p == '-' || *p == '*' || *p == '+') && p[1] == ' ') {
        *rest = skip_ws(p + 1);
        return true;
    }
    return false;
}

static bool is_ordered(const char *s, const char **rest) {
    const char *p = skip_ws(s);
    const char *d = p;
    while (isdigit((unsigned char)*p)) { p++; }
    if (p > d && (*p == '.' || *p == ')') && p[1] == ' ') {
        *rest = skip_ws(p + 1);
        return true;
    }
    return false;
}

/** A table separator row: cells of `:?-+:?` separated by `|`. */
static bool is_table_sep(const char *s) {
    const char *p = skip_ws(s);
    if (!strchr(p, '|') && !strchr(p, '-')) { return false; }
    bool saw_dash = false, saw_pipe = false;
    for (; *p; p++) {
        if (*p == '-') { saw_dash = true; }
        else if (*p == '|') { saw_pipe = true; }
        else if (*p != ':' && *p != ' ' && *p != '\t') { return false; }
    }
    return saw_dash && saw_pipe;
}


/* ── line array ──────────────────────────────────────────────────────────── */

typedef struct { char *buf; char **items; size_t count; } Lines;

static bool split_lines(const char *s, Lines *out) {
    out->buf = NULL; out->items = NULL; out->count = 0;
    char *buf = strdup(s ? s : "");
    if (!buf) { return false; }
    size_t cap = 16, count = 0;
    char **items = malloc(cap * sizeof *items);
    if (!items) { free(buf); return false; }
    char *line = buf;
    for (char *p = buf;; p++) {
        if (*p == '\n' || *p == '\0') {
            bool end = (*p == '\0');
            *p = '\0';
            if (count == cap) {
                cap *= 2;
                char **g = realloc(items, cap * sizeof *items);
                if (!g) { free(items); free(buf); return false; }
                items = g;
            }
            items[count++] = line;
            line = p + 1;
            if (end) { break; }
        }
    }
    out->buf = buf; out->items = items; out->count = count;
    return true;
}

static void free_lines(Lines *l) { free(l->buf); free(l->items); }


/* ── block renderers ─────────────────────────────────────────────────────── */

/** Renders a fenced code block; advances `*i` past the closing fence. */
static void render_code(Parts *parts, const Lines *L, size_t *i,
                        const Ctx *c) {
    size_t start = ++(*i);
    while (*i < L->count && !is_fence(L->items[*i])) { (*i)++; }

    /* Join the code lines verbatim. */
    size_t total = 1;
    for (size_t k = start; k < *i; k++) { total += strlen(L->items[k]) + 1; }
    char *code = malloc(total);
    if (code) {
        code[0] = '\0';
        for (size_t k = start; k < *i; k++) {
            strcat(code, L->items[k]);
            if (k + 1 < *i) { strcat(code, "\n"); }
        }
        parts_push(parts, sc_capture_panel_str(code, (ScPanelOpts){
            .border = { .type = SC_BORDER_ROUNDED, .color = c->code },
            .padding = { 0, 1, 0, 1 },
            .width = c->width,
        }));
        free(code);
    }
    if (*i < L->count) { (*i)++; }   /* skip closing fence */
}

/** Renders a block quote (consecutive `>` lines). */
static void render_quote(Parts *parts, const Lines *L, size_t *i,
                         const Ctx *c) {
    ScText *t = sc_text_new();
    ScTextStyle base = { 0 };
    if (c->quote_dim) { base.attr = SC_TEXT_ATTR_DIM; }
    else { base.fg = c->quote; }

    bool firstline = true;
    while (*i < L->count && is_quote(L->items[*i])) {
        const char *p = skip_ws(L->items[*i]);
        p++;                       /* drop '>' */
        p = skip_ws(p);
        if (!firstline) { sc_text_append(t, "\n", (ScTextStyle){ 0 }); }
        sc_text_append(t, "\xe2\x96\x8f ", base);   /* ▏ bar */
        append_inline(t, p, base, c);
        firstline = false;
        (*i)++;
    }
    parts_push(parts, sc_capture_text(t));
    sc_text_free(t);
}

/** Renders a bullet/ordered list (consecutive items). */
static void render_list(Parts *parts, const Lines *L, size_t *i,
                        bool ordered, const Ctx *c) {
    ScList *list = sc_list_new((ScListOpts){
        .marker = ordered ? SC_LIST_NUMBER : SC_LIST_BULLET,
        .width = c->width,
    });

    /* Build + keep item ScTexts alive until capture (they are borrowed). */
    ScText *texts[256];
    size_t tn = 0;
    const char *rest = NULL;
    while (*i < L->count && tn < 256) {
        bool b = is_bullet(L->items[*i], &rest);
        bool o = !b && is_ordered(L->items[*i], &rest);
        if (!b && !o) { break; }
        if (ordered != o) { break; }   /* list type changed */
        ScText *t = sc_text_new();
        append_inline(t, rest, (ScTextStyle){ 0 }, c);
        sc_list_add_text(list, t);
        texts[tn++] = t;
        (*i)++;
    }

    parts_push(parts, sc_capture_list(list));
    sc_list_free(list);
    for (size_t k = 0; k < tn; k++) { sc_text_free(texts[k]); }
}

/** Splits a `| a | b |` row into trimmed cell strings (heap array). */
static char **split_row(const char *line, size_t *out_n) {
    const char *p = skip_ws(line);
    if (*p == '|') { p++; }
    char *copy = strdup(p);
    if (!copy) { *out_n = 0; return NULL; }
    rtrim(copy);
    /* drop a trailing '|' */
    size_t cl = strlen(copy);
    if (cl > 0 && copy[cl - 1] == '|') { copy[cl - 1] = '\0'; }

    size_t cap = 8, n = 0;
    char **cells = malloc(cap * sizeof *cells);
    char *seg = copy;
    for (char *q = copy;; q++) {
        if (*q == '|' || *q == '\0') {
            bool end = (*q == '\0');
            *q = '\0';
            char *cell = strdup(rtrim((char *)skip_ws(seg)));
            if (n == cap) {
                cap *= 2;
                char **g = realloc(cells, cap * sizeof *cells);
                if (!g) {
                    free(cell);
                    for (size_t k = 0; k < n; k++) { free(cells[k]); }
                    free(cells); free(copy);
                    *out_n = 0;
                    return NULL;
                }
                cells = g;
            }
            cells[n++] = cell ? cell : strdup("");
            seg = q + 1;
            if (end) { break; }
        }
    }
    free(copy);
    *out_n = n;
    return cells;
}

/** Renders a GitHub-style pipe table (header + sep + rows). */
static void render_table(Parts *parts, const Lines *L, size_t *i,
                         const Ctx *c) {
    size_t hn = 0;
    char **headers = split_row(L->items[*i], &hn);
    (*i) += 2;   /* skip header + separator */

    ScTableData *tbl = sc_table_new();
    for (size_t k = 0; k < hn; k++) {
        sc_table_add_column(tbl, headers[k], (ScColOpts){ .word_wrap = true });
    }

    /* Collect data rows; cell strings are borrowed by the table until
       capture, so keep them in an arena. */
    char **arena = NULL; size_t an = 0, acap = 0;
    while (*i < L->count && !is_blank(L->items[*i])
           && strchr(L->items[*i], '|')) {
        size_t cn = 0;
        char **cells = split_row(L->items[*i], &cn);
        if (!cells) { (*i)++; continue; }
        ScCell row[64];
        size_t rc = cn < hn ? cn : hn;
        if (rc > 64) { rc = 64; }

        if (an + rc > acap) {
            size_t nc = acap ? acap : 32;
            while (an + rc > nc) { nc *= 2; }
            char **g = realloc(arena, nc * sizeof *arena);
            if (!g) {
                for (size_t k = 0; k < cn; k++) { free(cells[k]); }
                free(cells);
                break;
            }
            arena = g; acap = nc;
        }
        for (size_t k = 0; k < rc; k++) {
            arena[an] = cells[k];
            row[k] = sc_cell(arena[an]);
            an++;
        }
        for (size_t k = rc; k < cn; k++) { free(cells[k]); }
        free(cells);
        sc_table_add_row(tbl, row, rc);
        (*i)++;
    }

    parts_push(parts, sc_capture_table(tbl, (ScTableOpts){
        .border = { .type = SC_BORDER_SINGLE },
        .header = { .row = true, .style = { .attr = SC_TEXT_ATTR_BOLD } },
        .total_width = c->width,
    }));
    sc_table_free(tbl);
    for (size_t k = 0; k < hn; k++) { free(headers[k]); }
    free(headers);
    for (size_t k = 0; k < an; k++) { free(arena[k]); }
    free(arena);
}

/** Renders a paragraph (consecutive plain lines joined by spaces). */
static void render_paragraph(Parts *parts, const Lines *L, size_t *i,
                             const Ctx *c) {
    ScText *t = sc_text_new();
    bool firstline = true;
    while (*i < L->count && !is_blank(L->items[*i]) && !is_fence(L->items[*i])
           && !is_hr(L->items[*i]) && !is_quote(L->items[*i])) {
        const char *rest = NULL;
        if (is_bullet(L->items[*i], &rest) || is_ordered(L->items[*i], &rest)) {
            break;
        }
        if (!firstline) { sc_text_append(t, " ", (ScTextStyle){ 0 }); }
        append_inline(t, skip_ws(L->items[*i]), (ScTextStyle){ 0 }, c);
        firstline = false;
        (*i)++;
    }
    parts_push(parts, sc_capture_text(t));
    sc_text_free(t);
}

/** Renders the body text of one section into block parts. */
static void render_body(Parts *parts, const char *body, const Ctx *c) {
    if (!body || !body[0]) { return; }
    Lines L;
    if (!split_lines(body, &L)) { return; }

    size_t i = 0;
    while (i < L.count) {
        const char *line = L.items[i];
        if (is_blank(line)) { i++; continue; }

        if (is_fence(line)) { render_code(parts, &L, &i, c); }
        else if (is_hr(line)) {
            parts_push(parts, sc_capture_rule_str(NULL, (ScRuleOpts){
                .type = SC_BORDER_SINGLE, .width = c->width }));
            i++;
        }
        else if (is_quote(line)) { render_quote(parts, &L, &i, c); }
        else if (i + 1 < L.count && strchr(line, '|')
                 && is_table_sep(L.items[i + 1])) {
            render_table(parts, &L, &i, c);
        }
        else {
            const char *rest = NULL;
            if (is_bullet(line, &rest)) {
                render_list(parts, &L, &i, false, c);
            } else if (is_ordered(line, &rest)) {
                render_list(parts, &L, &i, true, c);
            } else {
                render_paragraph(parts, &L, &i, c);
            }
        }
    }
    free_lines(&L);
}

/** Renders a heading (text + optional underline rule) as one combined part. */
static void render_heading(Parts *parts, const ScMdSection *section,
                           const Ctx *c) {
    int level = sc_md_section_level(section);
    const char *title = sc_md_section_title(section);
    if (level <= 0 || !title || !title[0]) { return; }

    ScText *t = sc_text_new();
    ScTextStyle st = { SC_TEXT_ATTR_BOLD, c->heading, SC_ANSI_COLOR_NONE };
    append_inline(t, title, st, c);
    ScRendered *text = sc_capture_text(t);
    sc_text_free(t);

    if (level <= 2) {   /* underline top-level headings with a rule */
        ScRendered *rule = sc_capture_rule_str(NULL, (ScRuleOpts){
            .type = level == 1 ? SC_BORDER_DOUBLE : SC_BORDER_SINGLE,
            .color = c->heading, .width = c->width });
        ScRendered *combo[] = { text, rule };
        parts_push(parts, sc_vstack((const ScRendered *const *)combo, 2, 0));
        sc_rendered_free(text);
        sc_rendered_free(rule);
    } else {
        parts_push(parts, text);
    }
}

/** Walks a section subtree depth-first. */
static void render_section(Parts *parts, ScMdSection *section, const Ctx *c) {
    render_heading(parts, section, c);
    render_body(parts, sc_md_section_body(section), c);
    size_t n = sc_md_section_child_count(section);
    for (size_t k = 0; k < n; k++) {
        render_section(parts, sc_md_section_child(section, k), c);
    }
}


/* ── public API ──────────────────────────────────────────────────────────── */

ScRendered *sc_capture_markdown(const ScMarkdown *md,
                                ScMarkdownRenderOpts opts) {
    Ctx c = {
        .width   = opts.width,
        .heading = or_color(opts.heading_color, SC_ANSI_COLOR_CYAN),
        .code    = or_color(opts.code_color, SC_ANSI_COLOR_MAGENTA),
        .link    = or_color(opts.link_color, SC_ANSI_COLOR_BLUE),
        .quote_dim = opts.quote_color.index == 0,
        .quote   = opts.quote_color,
    };

    Parts parts = { 0 };
    /* sc_markdown_root needs a non-const handle; the tree is not mutated. */
    ScMdSection *root = sc_markdown_root((ScMarkdown *)md);
    if (root) { render_section(&parts, root, &c); }

    ScRendered *out = parts.n
        ? sc_vstack((const ScRendered *const *)parts.items, parts.n, 1)
        : sc_capture_str("");
    for (size_t k = 0; k < parts.n; k++) { sc_rendered_free(parts.items[k]); }
    free(parts.items);
    return out;
}

void sc_markdown_render(const ScMarkdown *md, ScMarkdownRenderOpts opts) {
    ScRendered *r = sc_capture_markdown(md, opts);
    sc_pad_print(r, (ScPadOpts){ 0 });
    sc_rendered_free(r);
}

void sc_markdown_render_str(const char *src, ScMarkdownRenderOpts opts) {
    ScMarkdown *md = sc_markdown_parse(src ? src : "",
                                       src ? strlen(src) : 0, NULL);
    if (!md) { return; }
    sc_markdown_render(md, opts);
    sc_markdown_free(md);
}
