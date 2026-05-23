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

/* zero-init ScColor {0,0,0,0} treated as "not set" for bg/border_bg fields */
static int color_active(ScColor c) {
    return c.index != -2 && !(c.index == 0 && !c.r && !c.g && !c.b);
}

static ScColor norm_bg(ScColor c) {
    return color_active(c) ? c : SC_ANSI_COLOR_NONE;
}

static void print_colored(const char *s, ScColor fg, ScColor bg) {
    sc_apply_colors(fg, norm_bg(bg));
    fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

static void print_repeat(const char *s, int n, ScColor fg, ScColor bg) {
    if (n <= 0) return;
    sc_apply_colors(fg, norm_bg(bg));
    for (int i = 0; i < n; i++) fputs(s, stdout);
    fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
}

static void render_hline(int inner_w, ScBorderType border,
                          ScColor border_fg, ScColor border_bg,
                          const char *lcorner, const char *rcorner,
                          const char *title, ScTextStyle title_opts, ScHAlign align,
                          int title_pad) {
    const char *h = border_table[border].h;
    if (title_pad < 0) title_pad = 0;
    print_colored(lcorner, border_fg, border_bg);

    if (title && *title) {
        int tlen   = (int)sc_utf8_string_length(title, strlen(title));
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

        print_repeat(h, ld, border_fg, border_bg);
        for (int i = 0; i < title_pad; i++) print_colored(" ", SC_ANSI_COLOR_NONE, title_opts.bg);
        sc_print(title, title_opts);
        for (int i = 0; i < title_pad; i++) print_colored(" ", SC_ANSI_COLOR_NONE, title_opts.bg);
        print_repeat(h, rd, border_fg, border_bg);
    } else {
        print_repeat(h, inner_w, border_fg, border_bg);
    }

    print_colored(rcorner, border_fg, border_bg);
    fputc('\n', stdout);
}

static void render_empty_line(int inner_w, ScBorderType border,
                               ScColor border_fg, ScColor border_bg, ScColor content_bg) {
    print_colored(border_table[border].v, border_fg, border_bg);
    if (color_active(content_bg)) {
        sc_apply_colors(SC_ANSI_COLOR_NONE, content_bg);
        for (int i = 0; i < inner_w; i++) fputc(' ', stdout);
        fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    } else {
        for (int i = 0; i < inner_w; i++) fputc(' ', stdout);
    }
    print_colored(border_table[border].v, border_fg, border_bg);
    fputc('\n', stdout);
}

/* ── Line extraction from ScText ────────────────────────────────────────── */

typedef struct { const char *text; ScTextStyle opts; } PSpan;
typedef struct { PSpan *spans; size_t count; size_t vis_w; } PLine;

static PLine *make_plines(const ScText *t, size_t *out_n) {
    size_t lines_cap = 8, nlines = 0;
    PLine *lines = malloc(lines_cap * sizeof(PLine));

    size_t buf_cap = 8, buf_n = 0, buf_w = 0;
    PSpan *buf = malloc(buf_cap * sizeof(PSpan));

    for (size_t si = 0; si < t->count; si++) {
        const char *s     = t->spans[si].raw_str;
        ScTextStyle   opts  = t->spans[si].style;
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
                    buf_w += sc_utf8_string_length(start, seglen);
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
            buf_w += sc_utf8_string_length(start, seglen);
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

static void render_content_line(PLine *line, int inner_w, int pad_l, int pad_r,
                                 ScBorderType border, ScColor border_fg, ScColor border_bg,
                                 ScHAlign align, ScColor content_bg) {
    int spare = inner_w - pad_l - pad_r - (int)line->vis_w;
    if (spare < 0) spare = 0;
    int lp = 0, rp = spare;
    if (align == SC_ALIGN_CENTER) { lp = spare / 2; rp = spare - lp; }
    else if (align == SC_ALIGN_RIGHT) { lp = spare; rp = 0; }

    print_colored(border_table[border].v, border_fg, border_bg);
    if (color_active(content_bg)) sc_apply_colors(SC_ANSI_COLOR_NONE, content_bg);
    for (int i = 0; i < pad_l; i++) fputc(' ', stdout);
    for (int i = 0; i < lp; i++) fputc(' ', stdout);
    for (size_t i = 0; i < line->count; i++) {
        sc_print(line->spans[i].text, line->spans[i].opts);
        if (color_active(content_bg)) sc_apply_colors(SC_ANSI_COLOR_NONE, content_bg);
    }
    for (int i = 0; i < rp; i++) fputc(' ', stdout);
    for (int i = 0; i < pad_r; i++) fputc(' ', stdout);
    if (color_active(content_bg)) fputs(SC_ANSI_ESCAPE_CODE_RESET, stdout);
    print_colored(border_table[border].v, border_fg, border_bg);
    fputc('\n', stdout);
}

void sc_panel_text(const ScText *content, ScPanelOpts opts) {
    size_t nlines;
    PLine *lines = make_plines(content, &nlines);

    size_t max_cw = 0;
    for (size_t i = 0; i < nlines; i++)
        if (lines[i].vis_w > max_cw) max_cw = lines[i].vis_w;

    int title_pad = opts.title_pad;
    int title_len = opts.title ? (int)sc_utf8_string_length(opts.title, strlen(opts.title)) : 0;
    int min4title = opts.title ? title_len + 2 * title_pad + 2 : 0;
    int pad_l = opts.padding.left  > 0 ? opts.padding.left  : 0;
    int pad_r = opts.padding.right > 0 ? opts.padding.right : 0;
    int pad_t = opts.padding.top   > 0 ? opts.padding.top   : 0;
    int pad_b = opts.padding.bottom> 0 ? opts.padding.bottom: 0;
    int ml    = opts.margin.left   > 0 ? opts.margin.left   : 0;
    int mr    = opts.margin.right  > 0 ? opts.margin.right  : 0;

    int inner_w;
    if (opts.full_width) {
        inner_w = sc_terminal_width() - 2 - ml - mr;
        if (inner_w < 2) inner_w = 2;
    } else if (opts.width > 0) {
        inner_w = opts.width - 2;
    } else {
        int from_content = (int)max_cw + pad_l + pad_r;
        inner_w = from_content > min4title ? from_content : min4title;
        if (inner_w < 2) inner_w = 2;
    }

    const char *tl = border_table[opts.border].tl;
    const char *tr = border_table[opts.border].tr;
    const char *bl = border_table[opts.border].bl;
    const char *br = border_table[opts.border].br;

#define PMARG() do { for (int _i = 0; _i < ml; _i++) fputc(' ', stdout); } while(0)

    for (int i = 0; i < opts.margin.top; i++) fputc('\n', stdout);

    /* top border */
    PMARG();
    render_hline(inner_w, opts.border, opts.border_color, opts.border_bg, tl, tr,
                 opts.title_pos == SC_TITLE_TOP ? opts.title : NULL,
                 opts.title_opts, opts.title_align, title_pad);

    for (int i = 0; i < pad_t; i++) {
        PMARG();
        render_empty_line(inner_w, opts.border, opts.border_color, opts.border_bg, opts.bg);
    }

    for (size_t i = 0; i < nlines; i++) {
        PMARG();
        render_content_line(&lines[i], inner_w, pad_l, pad_r,
                            opts.border, opts.border_color, opts.border_bg,
                            opts.content_align, opts.bg);
    }

    for (int i = 0; i < pad_b; i++) {
        PMARG();
        render_empty_line(inner_w, opts.border, opts.border_color, opts.border_bg, opts.bg);
    }

    /* bottom border */
    PMARG();
    render_hline(inner_w, opts.border, opts.border_color, opts.border_bg, bl, br,
                 opts.title_pos == SC_TITLE_BOTTOM ? opts.title : NULL,
                 opts.title_opts, opts.title_align, title_pad);

    for (int i = 0; i < opts.margin.bottom; i++) fputc('\n', stdout);

#undef PMARG

    free_plines(lines, nlines);
}

void sc_panel_str(const char *content, ScPanelOpts opts) {
    ScTextStyle plain = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    ScText *t = sc_text_new();
    sc_text_append(t, content, plain);
    sc_panel_text(t, opts);
    sc_text_free(t);
}
