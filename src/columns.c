#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

/* zero-init ScColor {0,0,0,0} treated as "not set" for bg fields */
static int color_active(ScColor c) {
    return c.index != -2 && !(c.index == 0 && !c.r && !c.g && !c.b);
}

/* Output a line, re-applying bg after every \033[0m reset so the column
   background is not cancelled by resets in the captured content.
   Caller must apply bg before calling and reset after. */
static void fputs_with_bg(const char *line, ScColor bg) {
    const char *p = line;
    while (*p) {
        if (p[0] == '\033' && p[1] == '[' && p[2] == '0' && p[3] == 'm') {
            fputs("\033[0m", stdout);
            p += 4;
            if (*p) { sc_apply_colors(SC_ANSI_COLOR_NONE, bg); }
        } else {
            fputc(*p++, stdout);
        }
    }
}

/* ── Separator characters per border style ──────────────────────────────── */

static const char *sep_chars[] = {
    [SC_BORDER_NONE]    = NULL,
    [SC_BORDER_ASCII]   = "|",
    [SC_BORDER_SINGLE]  = "│",
    [SC_BORDER_DOUBLE]  = "║",
    [SC_BORDER_ROUNDED] = "│",
    [SC_BORDER_THICK]   = "┃",
};

/* ── ANSI-aware visible width ───────────────────────────────────────────── */

static int ansi_vis_w(const char *s) {
    int w = 0;
    while (*s) {
        if ((unsigned char)*s == 0x1b && s[1] == '[') {
            s += 2;
            while (*s && *s != 'm') { s++; }
            if (*s) { s++; }
        } else if (((unsigned char)*s & 0xC0) != 0x80) {
            w++; s++;
        } else {
            s++;
        }
    }
    return w;
}

/* ── ScRendered ─────────────────────────────────────────────────────────── */

void sc_rendered_free(ScRendered *r) {
    if (!r) { return; }
    for (size_t i = 0; i < r->line_count; i++) { free(r->lines[i]); }
    free(r->lines);
    free(r->column_widths);
    free(r);
}

static ScRendered *buf_to_rendered(char *buf, size_t sz) {
    ScRendered *r = malloc(sizeof(ScRendered));
    r->lines      = NULL;
    r->column_widths = NULL;
    r->line_count      = 0;
    r->max_column_width  = 0;

    size_t cap = 16;
    r->lines      = malloc(cap * sizeof(char *));
    r->column_widths = malloc(cap * sizeof(int));

    char *p = buf;
    char *end = buf + sz;
    while (p <= end) {
        char *nl = memchr(p, '\n', (size_t)(end - p));
        size_t len = nl ? (size_t)(nl - p) : (size_t)(end - p);

        /* skip trailing empty line after final newline */
        if (!nl && len == 0) { break; }

        if (r->line_count == cap) {
            cap *= 2;
            r->lines      = realloc(r->lines,      cap * sizeof(char *));
            r->column_widths = realloc(r->column_widths, cap * sizeof(int));
        }
        char *line = strndup(p, len);
        int   vw   = ansi_vis_w(line);
        r->lines[r->line_count]      = line;
        r->column_widths[r->line_count] = vw;
        if (vw > r->max_column_width) { r->max_column_width = vw; }
        r->line_count++;
        p = nl ? nl + 1 : end + 1;
    }
    return r;
}

/* ── Capture-and-replay ─────────────────────────────────────────────────── */

typedef struct { void (*fn)(void *); void *ctx; } CapCtx;

static ScRendered *render_captured(void (*fn)(void *), void *ctx) {
    fflush(stdout);

    FILE *tmp = tmpfile();
    if (!tmp) { return NULL; }

    int saved = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);

    fn(ctx);

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    fseek(tmp, 0, SEEK_END);
    long len = ftell(tmp);
    rewind(tmp);
    char *buf = malloc((size_t)len + 1);
    fread(buf, 1, (size_t)len, tmp);
    buf[len] = '\0';
    fclose(tmp);

    ScRendered *r = buf_to_rendered(buf, (size_t)len);
    free(buf);
    return r;
}

/* ── Capture wrappers ───────────────────────────────────────────────────── */

typedef struct { const ScTableData *table_data; ScTableOpts opts; } CtxTable;
static void _render_table(void *p) {
    CtxTable *c = p;
    sc_table_print(c->table_data, c->opts);
}

typedef struct { const char *content; ScPanelOpts opts; } CtxPanelStr;
static void _render_panel_str(void *p) {
    CtxPanelStr *c = p;
    sc_panel_str(c->content, c->opts);
}

typedef struct { const ScText *content; ScPanelOpts opts; } CtxPanelText;
static void _render_panel_text(void *p) {
    CtxPanelText *c = p;
    sc_panel_text(c->content, c->opts);
}

typedef struct { const ScText *t; } CtxText;
static void _render_text(void *p) {
    sc_print_text(((CtxText *)p)->t);
}

typedef struct { const char *s; } CtxStr;
static void _render_str(void *p) {
    fputs(((CtxStr *)p)->s, stdout);
    /* ensure trailing newline */
    const char *s = ((CtxStr *)p)->s;
    if (s[0] && s[strlen(s) - 1] != '\n') { fputc('\n', stdout); }
}

typedef struct { const ScColumns *cl; } CtxCols;
static void _render_columns(void *p) {
    sc_columns_print(((CtxCols *)p)->cl);
}

typedef struct { const ScTree *t; } CtxTree;
static void _render_tree(void *p) {
    sc_tree_print(((CtxTree *)p)->t);
}

typedef struct { const ScList *l; } CtxList;
static void _render_list(void *p) {
    sc_list_print(((CtxList *)p)->l);
}

typedef struct { const ScKV *kv; } CtxKV;
static void _render_kv(void *p) {
    sc_kv_print(((CtxKV *)p)->kv);
}

typedef struct { const char *title; ScRuleOpts opts; } CtxRuleStr;
static void _render_rule_str(void *p) {
    CtxRuleStr *c = p; sc_rule_str(c->title, c->opts);
}

typedef struct { const ScText *title; ScRuleOpts opts; } CtxRuleText;
static void _render_rule_text(void *p) {
    CtxRuleText *c = p; sc_rule_text(c->title, c->opts);
}

/* ── Internal entry helpers ─────────────────────────────────────────────── */

typedef struct {
    ScRendered  *rendered;
    ScColItem    item;
    bool         stretch;     /* expand to full column height at print time */
    ScPanelOpts  panel_opts;  /* used when stretch to generate filler lines */
} ScColEntry;

struct ScColumns {
    ScColEntry    *entries;
    size_t         count, cap;
    ScColumnsOpts  opts;
};

static void columns_push(ScColumns *cl, ScRendered *r, ScColItem item) {
    if (cl->count == cl->cap) {
        cl->cap = cl->cap ? cl->cap * 2 : 4;
        cl->entries = realloc(cl->entries, cl->cap * sizeof(ScColEntry));
    }
    cl->entries[cl->count++] = (ScColEntry){ r, item, 0, {0} };
}

static void columns_push_panel(ScColumns *cl, ScRendered *r,
                                ScColItem item, ScPanelOpts opts) {
    if (cl->count == cl->cap) {
        cl->cap = cl->cap ? cl->cap * 2 : 4;
        cl->entries = realloc(cl->entries, cl->cap * sizeof(ScColEntry));
    }
    cl->entries[cl->count++] = (ScColEntry){ r, item, item.stretch, opts };
}

/* ── Public API ─────────────────────────────────────────────────────────── */

ScColumns *sc_columns_new(ScColumnsOpts opts) {
    ScColumns *cl = malloc(sizeof(ScColumns));
    cl->entries = NULL;
    cl->count   = 0;
    cl->cap     = 0;
    cl->opts    = opts;
    return cl;
}

void sc_columns_add_table(ScColumns *cl, const ScTableData *table_data, ScTableOpts opts, ScColItem item) {
    CtxTable ctx = { table_data, opts };
    columns_push(cl, render_captured(_render_table, &ctx), item);
}

void sc_columns_add_panel_str(ScColumns *cl, const char *content,
                               ScPanelOpts opts, ScColItem item) {
    CtxPanelStr ctx = { content, opts };
    columns_push_panel(cl, render_captured(_render_panel_str, &ctx), item, opts);
}

void sc_columns_add_panel_text(ScColumns *cl, const ScText *content,
                                ScPanelOpts opts, ScColItem item) {
    CtxPanelText ctx = { content, opts };
    columns_push_panel(cl, render_captured(_render_panel_text, &ctx), item, opts);
}

void sc_columns_add_text(ScColumns *cl, const ScText *t, ScColItem item) {
    CtxText ctx = { t };
    columns_push(cl, render_captured(_render_text, &ctx), item);
}

void sc_columns_add_str(ScColumns *cl, const char *s, ScColItem item) {
    CtxStr ctx = { s };
    columns_push(cl, render_captured(_render_str, &ctx), item);
}

void sc_columns_add_columns(ScColumns *cl, const ScColumns *nested, ScColItem item) {
    CtxCols ctx = { nested };
    columns_push(cl, render_captured(_render_columns, &ctx), item);
}

void sc_columns_add_tree(ScColumns *cl, const ScTree *tree, ScColItem item) {
    CtxTree ctx = { tree };
    columns_push(cl, render_captured(_render_tree, &ctx), item);
}

void sc_columns_add_list(ScColumns *cl, const ScList *list, ScColItem item) {
    CtxList ctx = { list };
    columns_push(cl, render_captured(_render_list, &ctx), item);
}

/* ── Capture API (public) ────────────────────────────────────────────────── */

ScRendered *sc_capture_str(const char *s) {
    CtxStr ctx = { s }; return render_captured(_render_str, &ctx);
}
ScRendered *sc_capture_text(const ScText *t) {
    CtxText ctx = { t }; return render_captured(_render_text, &ctx);
}
ScRendered *sc_capture_table(const ScTableData *table_data, ScTableOpts opts) {
    CtxTable ctx = { table_data, opts }; return render_captured(_render_table, &ctx);
}
ScRendered *sc_capture_list(const ScList *l) {
    CtxList ctx = { l }; return render_captured(_render_list, &ctx);
}
ScRendered *sc_capture_tree(const ScTree *t) {
    CtxTree ctx = { t }; return render_captured(_render_tree, &ctx);
}
ScRendered *sc_capture_kv(const ScKV *kv) {
    CtxKV ctx = { kv }; return render_captured(_render_kv, &ctx);
}
ScRendered *sc_capture_columns(const ScColumns *cl) {
    CtxCols ctx = { cl }; return render_captured(_render_columns, &ctx);
}
ScRendered *sc_capture_panel_str(const char *content, ScPanelOpts opts) {
    CtxPanelStr ctx = { content, opts }; return render_captured(_render_panel_str, &ctx);
}
ScRendered *sc_capture_panel_text(const ScText *content, ScPanelOpts opts) {
    CtxPanelText ctx = { content, opts }; return render_captured(_render_panel_text, &ctx);
}
ScRendered *sc_capture_rule_str(const char *title, ScRuleOpts opts) {
    CtxRuleStr ctx = { title, opts }; return render_captured(_render_rule_str, &ctx);
}
ScRendered *sc_capture_rule_text(const ScText *title, ScRuleOpts opts) {
    CtxRuleText ctx = { title, opts }; return render_captured(_render_rule_text, &ctx);
}

/* ── sc_columns_add_rendered ─────────────────────────────────────────────── */

void sc_columns_add_rendered(ScColumns *cl, const ScRendered *r, ScColItem item) {
    if (!r) { return; }
    ScRendered *copy    = malloc(sizeof(ScRendered));
    copy->line_count         = r->line_count;
    copy->max_column_width     = r->max_column_width;
    copy->lines         = malloc(r->line_count * sizeof(char *));
    copy->column_widths    = malloc(r->line_count * sizeof(int));
    for (size_t i = 0; i < r->line_count; i++) {
        copy->lines[i]      = strdup(r->lines[i]);
        copy->column_widths[i] = r->column_widths[i];
    }
    columns_push(cl, copy, item);
}

void sc_columns_free(ScColumns *cl) {
    if (!cl) { return; }
    for (size_t i = 0; i < cl->count; i++) {
        sc_rendered_free(cl->entries[i].rendered);
    }
    free(cl->entries);
    free(cl);
}

/* ── Printing ────────────────────────────────────────────────────────────── */

/* Generate one empty content line matching the panel's border and bg colors.
   inner_w = panel's inner width (max_vis_w - 2). Returns a heap-allocated string. */
static char *make_empty_content_line(const ScPanelOpts *opts, int inner_w) {
    const char *v = (opts->border.type != SC_BORDER_NONE && sep_chars[opts->border.type])
                    ? sep_chars[opts->border.type] : " ";
    fflush(stdout);
    FILE *tmp = tmpfile();
    int   saved = dup(STDOUT_FILENO);
    dup2(fileno(tmp), STDOUT_FILENO);

    sc_apply_colors(opts->border.color, opts->border.bg);
    fputs(v, stdout);
    fputs("\033[0m", stdout);
    if (color_active(opts->bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, opts->bg);
        for (int i = 0; i < inner_w; i++) { fputc(' ', stdout); }
        fputs("\033[0m", stdout);
    } else {
        for (int i = 0; i < inner_w; i++) { fputc(' ', stdout); }
    }
    sc_apply_colors(opts->border.color, opts->border.bg);
    fputs(v, stdout);
    fputs("\033[0m", stdout);

    fflush(stdout);
    dup2(saved, STDOUT_FILENO);
    close(saved);

    fseek(tmp, 0, SEEK_END);
    long len = ftell(tmp);
    rewind(tmp);
    char *line = malloc((size_t)len + 1);
    fread(line, 1, (size_t)len, tmp);
    line[len] = '\0';
    fclose(tmp);
    return line;
}

/* Build an expanded copy of r with `extra` empty filler lines inserted before
   the last line (bottom border). Caller owns the returned ScRendered. */
static ScRendered *stretch_rendered(const ScRendered *orig, int extra,
                                    const ScPanelOpts *opts) {
    int inner_w = orig->max_column_width - 2;
    if (inner_w < 0) { inner_w = 0; }
    char *empty = make_empty_content_line(opts, inner_w);
    int   evw   = ansi_vis_w(empty);

    size_t new_count = orig->line_count + (size_t)extra;
    ScRendered *exp  = malloc(sizeof(ScRendered));
    exp->lines      = malloc(new_count * sizeof(char *));
    exp->column_widths = malloc(new_count * sizeof(int));
    exp->line_count      = new_count;
    exp->max_column_width  = orig->max_column_width;

    for (size_t i = 0; i < orig->line_count - 1; i++) {
        exp->lines[i]      = strdup(orig->lines[i]);
        exp->column_widths[i] = orig->column_widths[i];
    }
    for (int i = 0; i < extra; i++) {
        exp->lines[orig->line_count - 1 + (size_t)i]      = strdup(empty);
        exp->column_widths[orig->line_count - 1 + (size_t)i] = evw;
    }
    exp->lines[new_count - 1]      = strdup(orig->lines[orig->line_count - 1]);
    exp->column_widths[new_count - 1] = orig->column_widths[orig->line_count - 1];

    free(empty);
    return exp;
}

void sc_columns_print(const ScColumns *cl) {
    if (!cl || !cl->count) { return; }

    const char *sep = (cl->opts.sep.type > SC_BORDER_NONE)
                      ? sep_chars[cl->opts.sep.type] : NULL;
    int gap = cl->opts.gap > 0 ? cl->opts.gap : (sep ? 2 : 3);

    /* ── stretch: build working array with expanded copies for stretch panels ── */
    size_t max_h_all = 0;
    for (size_t i = 0; i < cl->count; i++) {
        if (cl->entries[i].rendered->line_count > max_h_all) {
            max_h_all = cl->entries[i].rendered->line_count;
        }
    }

    ScRendered **working = malloc(cl->count * sizeof(ScRendered *));
    for (size_t i = 0; i < cl->count; i++) {
        working[i] = cl->entries[i].rendered;
    }

    for (size_t ci = 0; ci < cl->count; ci++) {
        const ScColEntry *e = &cl->entries[ci];
        if (!e->stretch || e->rendered->line_count >= max_h_all) { continue; }
        int extra = (int)max_h_all - (int)e->rendered->line_count;
        working[ci] = stretch_rendered(e->rendered, extra, &e->panel_opts);
    }

    /* ── column widths ── */
    int *cw = malloc(cl->count * sizeof(int));
    for (size_t i = 0; i < cl->count; i++) {
        ScColItem *item = &cl->entries[i].item;
        int w = working[i]->max_column_width;
        if (item->fixed_w > 0) {
            w = item->fixed_w;
        } else {
            if (item->min_w > 0 && w < item->min_w) { w = item->min_w; }
            if (item->max_w > 0 && w > item->max_w) { w = item->max_w; }
        }
        cw[i] = w;
    }

    /* ── total_width distribution ── */
    if (cl->opts.total_width > 0) {
        int used = 0;
        for (size_t i = 0; i < cl->count; i++) { used += cw[i]; }
        /* add separator and gap widths */
        for (size_t i = 0; i + 1 < cl->count; i++) {
            used += sep ? (gap * 2 + 1) : gap;
        }
        int delta = cl->opts.total_width - used;
        if (delta != 0) {
            int nflex = 0;
            for (size_t i = 0; i < cl->count; i++) {
                if (cl->entries[i].item.fixed_w == 0) { nflex++; }
            }
            if (nflex > 0) {
                int per  = delta / nflex;
                int rem  = delta - per * nflex;
                int sign = rem >= 0 ? 1 : -1;
                rem = rem < 0 ? -rem : rem;
                for (size_t i = 0; i < cl->count; i++) {
                    if (cl->entries[i].item.fixed_w > 0) { continue; }
                    int adj = per + (rem > 0 ? sign : 0);
                    if (rem > 0) { rem--; }
                    int nw = cw[i] + adj;
                    if (nw < 0) { nw = 0; }
                    cw[i] = nw;
                }
            }
        }
    }

    /* ── vertical alignment offsets ── */
    size_t total_h = 0;
    for (size_t i = 0; i < cl->count; i++) {
        if (working[i]->line_count > total_h) {
            total_h = working[i]->line_count;
        }
    }

    int *top_off = malloc(cl->count * sizeof(int));
    for (size_t i = 0; i < cl->count; i++) {
        ScColItem *item = &cl->entries[i].item;
        ScVAlign   va   = item->valign_set ? item->valign : cl->opts.valign;
        int h     = (int)working[i]->line_count;
        int extra = (int)total_h - h;
        int off   = 0;
        if (va == SC_VALIGN_MIDDLE) { off = extra / 2; }
        else if (va == SC_VALIGN_BOTTOM) { off = extra; }
        top_off[i] = off;
    }

    int cml = cl->opts.margin.left > 0 ? cl->opts.margin.left : 0;

    /* ── render lines ── */
    for (int i = 0; i < cl->opts.margin.top; i++) { fputc('\n', stdout); }
    for (size_t li = 0; li < total_h; li++) {
        for (int i = 0; i < cml; i++) { fputc(' ', stdout); }
        for (size_t ci = 0; ci < cl->count; ci++) {
            ScColEntry *e   = &cl->entries[ci];
            ScRendered *r   = working[ci];
            int         col = cw[ci];
            int         ri  = (int)li - top_off[ci];

            if (ri >= 0 && ri < (int)r->line_count) {
                const char *line = r->lines[ri];
                int vw    = r->column_widths[ri];
                int spare = col - vw;
                if (spare < 0) { spare = 0; }
                int lp = 0, rp = spare;
                if (e->item.align == SC_ALIGN_CENTER) { lp = spare / 2; rp = spare - lp; }
                else if (e->item.align == SC_ALIGN_RIGHT) { lp = spare; rp = 0; }
                int has_bg = color_active(e->item.bg);
                if (has_bg) {
                    sc_apply_colors(SC_ANSI_COLOR_NONE, e->item.bg);
                    for (int k = 0; k < lp; k++) { fputc(' ', stdout); }
                    fputs_with_bg(line, e->item.bg);
                    for (int k = 0; k < rp; k++) { fputc(' ', stdout); }
                    fputs("\033[0m", stdout);
                } else {
                    for (int k = 0; k < lp; k++) { fputc(' ', stdout); }
                    fputs(line, stdout);
                    for (int k = 0; k < rp; k++) { fputc(' ', stdout); }
                }
            } else {
                if (color_active(e->item.bg)) {
                    sc_apply_colors(SC_ANSI_COLOR_NONE, e->item.bg);
                    for (int k = 0; k < col; k++) { fputc(' ', stdout); }
                    fputs("\033[0m", stdout);
                } else {
                    for (int k = 0; k < col; k++) { fputc(' ', stdout); }
                }
            }

            if (ci + 1 < cl->count) {
                if (sep) {
                    int has_sep_bg = color_active(cl->opts.sep.bg);
                    if (has_sep_bg) { sc_apply_colors(SC_ANSI_COLOR_NONE, cl->opts.sep.bg); }
                    for (int k = 0; k < gap; k++) { fputc(' ', stdout); }
                    if (color_active(cl->opts.sep.color)) {
                        sc_apply_colors(cl->opts.sep.color, cl->opts.sep.bg);
                        fputs(sep, stdout);
                        if (has_sep_bg) { sc_apply_colors(SC_ANSI_COLOR_NONE, cl->opts.sep.bg); }
                        else { fputs("\033[0m", stdout); }
                    } else {
                        fputs(sep, stdout);
                    }
                    for (int k = 0; k < gap; k++) { fputc(' ', stdout); }
                    if (has_sep_bg) { fputs("\033[0m", stdout); }
                } else {
                    int has_sep_bg = color_active(cl->opts.sep.bg);
                    if (has_sep_bg) { sc_apply_colors(SC_ANSI_COLOR_NONE, cl->opts.sep.bg); }
                    for (int k = 0; k < gap; k++) { fputc(' ', stdout); }
                    if (has_sep_bg) { fputs("\033[0m", stdout); }
                }
            }
        }
        fputc('\n', stdout);
    }
    for (int i = 0; i < cl->opts.margin.bottom; i++) { fputc('\n', stdout); }

    for (size_t i = 0; i < cl->count; i++) {
        if (working[i] != cl->entries[i].rendered) {
            sc_rendered_free(working[i]);
        }
    }
    free(working);
    free(cw);
    free(top_off);
}
