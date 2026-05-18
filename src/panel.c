#include "sparcli.h"
#include "internal.h"
#include <string.h>
#include <stdlib.h>
#include <string.h>

/* ── Border character sets ──────────────────────────────────────────────── */

static const struct {
    const char *tl, *tr, *bl, *br, *h, *v;
} border_table[] = {
    [SC_BORDER_NONE]    = { " ", " ", " ", " ", " ", " " },
    [SC_BORDER_ASCII]   = { "+", "+", "+", "+", "-", "|" },
    [SC_BORDER_SINGLE]  = { "┌", "┐", "└", "┘", "─", "│" },
    [SC_BORDER_DOUBLE]  = { "╔", "╗", "╚", "╝", "═", "║" },
    [SC_BORDER_ROUNDED] = { "╭", "╮", "╰", "╯", "─", "│" },
    [SC_BORDER_THICK]   = { "┏", "┓", "┗", "┛", "━", "┃" },
};

/* ── Internal rendering helpers ─────────────────────────────────────────── */

static void print_colored(const char *s, ScColor color) {
    sc_apply_colors(color, SC_COLOR_NONE);
    fputs(s, stdout);
    fputs(SC_RESET, stdout);
}

static void print_repeat(const char *s, int n, ScColor color) {
    if (n <= 0) return;
    sc_apply_colors(color, SC_COLOR_NONE);
    for (int i = 0; i < n; i++) fputs(s, stdout);
    fputs(SC_RESET, stdout);
}

static void render_hline(int inner_w, ScBorderStyle border, ScColor color,
                          const char *lcorner, const char *rcorner,
                          const char *title, ScOptions title_opts, ScAlign align,
                          int title_pad) {
    const char *h = border_table[border].h;
    if (title_pad < 0) title_pad = 0;
    print_colored(lcorner, color);

    if (title && *title) {
        int tlen   = (int)strlen(title);
        int dashes = inner_w - tlen - 2 * title_pad;
        if (dashes < 0) dashes = 0;

        int ld, rd;
        if (align == SC_ALIGN_LEFT) {
            ld = 1; rd = dashes - 1;
        } else if (align == SC_ALIGN_RIGHT) {
            ld = dashes - 1; rd = 1;
        } else { /* CENTER */
            ld = dashes / 2; rd = dashes - ld;
        }
        if (ld < 0) ld = 0;
        if (rd < 0) rd = 0;

        print_repeat(h, ld, color);
        for (int i = 0; i < title_pad; i++) print_colored(" ", color);
        sc_print(title, title_opts);
        for (int i = 0; i < title_pad; i++) print_colored(" ", color);
        print_repeat(h, rd, color);
    } else {
        print_repeat(h, inner_w, color);
    }

    print_colored(rcorner, color);
    fputc('\n', stdout);
}

static void render_empty_line(int inner_w, ScBorderStyle border, ScColor color) {
    print_colored(border_table[border].v, color);
    for (int i = 0; i < inner_w; i++) fputc(' ', stdout);
    print_colored(border_table[border].v, color);
    fputc('\n', stdout);
}

/* ── Line extraction from ScText ────────────────────────────────────────── */

typedef struct { const char *text; ScOptions opts; } PSpan;
typedef struct { PSpan *spans; size_t count; size_t vis_w; } PLine;

static PLine *make_plines(const ScText *t, size_t *out_n) {
    size_t lines_cap = 8, nlines = 0;
    PLine *lines = malloc(lines_cap * sizeof(PLine));

    size_t buf_cap = 8, buf_n = 0, buf_w = 0;
    PSpan *buf = malloc(buf_cap * sizeof(PSpan));

    for (size_t si = 0; si < t->count; si++) {
        const char *s     = t->spans[si].text;
        ScOptions   opts  = t->spans[si].opts;
        const char *start = s;

        while (*s) {
            if (*s == '\n') {
                size_t seglen = (size_t)(s - start);
                if (seglen > 0) {
                    if (buf_n == buf_cap) {
                        buf_cap *= 2;
                        buf = realloc(buf, buf_cap * sizeof(PSpan));
                    }
                    buf[buf_n++] = (PSpan){ strndup(start, seglen), opts };
                    buf_w += sc_utf8_vis_w(start, seglen);
                }
                /* flush line */
                if (nlines == lines_cap) {
                    lines_cap *= 2;
                    lines = realloc(lines, lines_cap * sizeof(PLine));
                }
                PSpan *ls = malloc((buf_n + 1) * sizeof(PSpan));
                memcpy(ls, buf, buf_n * sizeof(PSpan));
                lines[nlines++] = (PLine){ ls, buf_n, buf_w };
                buf_n = 0;
                buf_w = 0;
                start = s + 1;
            }
            s++;
        }
        size_t seglen = (size_t)(s - start);
        if (seglen > 0) {
            if (buf_n == buf_cap) {
                buf_cap *= 2;
                buf = realloc(buf, buf_cap * sizeof(PSpan));
            }
            buf[buf_n++] = (PSpan){ strndup(start, seglen), opts };
            buf_w += sc_utf8_vis_w(start, seglen);
        }
    }

    /* flush last line */
    if (nlines == lines_cap) {
        lines_cap *= 2;
        lines = realloc(lines, lines_cap * sizeof(PLine));
    }
    PSpan *ls = malloc((buf_n + 1) * sizeof(PSpan));
    memcpy(ls, buf, buf_n * sizeof(PSpan));
    lines[nlines++] = (PLine){ ls, buf_n, buf_w };

    free(buf);
    *out_n = nlines;
    return lines;
}

static void free_plines(PLine *lines, size_t n) {
    for (size_t i = 0; i < n; i++) {
        for (size_t j = 0; j < lines[i].count; j++)
            free((char *)lines[i].spans[j].text);
        free(lines[i].spans);
    }
    free(lines);
}

/* ── Panel rendering ────────────────────────────────────────────────────── */

static void render_content_line(PLine *line, int inner_w, int pad_x,
                                 ScBorderStyle border, ScColor bc) {
    int rpad = inner_w - 2 * pad_x - (int)line->vis_w;
    if (rpad < 0) rpad = 0;

    print_colored(border_table[border].v, bc);
    for (int i = 0; i < pad_x; i++) fputc(' ', stdout);
    for (size_t i = 0; i < line->count; i++)
        sc_print(line->spans[i].text, line->spans[i].opts);
    for (int i = 0; i < rpad; i++) fputc(' ', stdout);
    for (int i = 0; i < pad_x; i++) fputc(' ', stdout);
    print_colored(border_table[border].v, bc);
    fputc('\n', stdout);
}

void sc_panel_text(const ScText *content, ScPanelOpts opts) {
    size_t nlines;
    PLine *lines = make_plines(content, &nlines);

    size_t max_cw = 0;
    for (size_t i = 0; i < nlines; i++)
        if (lines[i].vis_w > max_cw) max_cw = lines[i].vis_w;

    int title_pad   = opts.title_pad;
    int title_len   = opts.title ? (int)strlen(opts.title) : 0;
    int min4title   = opts.title ? title_len + 2 * title_pad + 2 : 0;
    int inner_w;
    if (opts.width > 0) {
        inner_w = opts.width - 2;
    } else {
        int from_content = (int)max_cw + 2 * opts.pad_x;
        inner_w = from_content > min4title ? from_content : min4title;
        if (inner_w < 2) inner_w = 2;
    }

    const char *tl = border_table[opts.border].tl;
    const char *tr = border_table[opts.border].tr;
    const char *bl = border_table[opts.border].bl;
    const char *br = border_table[opts.border].br;

    /* top border */
    render_hline(inner_w, opts.border, opts.border_color, tl, tr,
                 opts.title_pos == SC_TITLE_TOP ? opts.title : NULL,
                 opts.title_opts, opts.title_align, title_pad);

    for (int i = 0; i < opts.pad_y; i++)
        render_empty_line(inner_w, opts.border, opts.border_color);

    for (size_t i = 0; i < nlines; i++)
        render_content_line(&lines[i], inner_w, opts.pad_x,
                            opts.border, opts.border_color);

    for (int i = 0; i < opts.pad_y; i++)
        render_empty_line(inner_w, opts.border, opts.border_color);

    /* bottom border */
    render_hline(inner_w, opts.border, opts.border_color, bl, br,
                 opts.title_pos == SC_TITLE_BOTTOM ? opts.title : NULL,
                 opts.title_opts, opts.title_align, title_pad);

    free_plines(lines, nlines);
}

void sc_panel_str(const char *content, ScPanelOpts opts) {
    ScOptions plain = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
    ScText *t = sc_text_new();
    sc_text_append(t, content, plain);
    sc_panel_text(t, opts);
    sc_text_free(t);
}
