#include "sparcli.h"
#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal structures ─────────────────────────────────────────────────── */

struct ScListItem {
    int           is_text;
    char         *str;       /* owned */
    const ScText *text;      /* not owned */
    ScTextStyle     opts;
    ScList       *sublist;   /* owned, may be NULL */
};

struct ScList {
    ScListItem  **items;
    size_t        count, cap;
    ScListOpts    opts;
};

/* ── Roman numeral conversion ────────────────────────────────────────────── */

static void to_roman(int n, char *buf, int upper) {
    static const struct { int v; const char *u; const char *l; } t[] = {
        {1000,"M","m"},{900,"CM","cm"},{500,"D","d"},{400,"CD","cd"},
        {100,"C","c"},{ 90,"XC","xc"},{ 50,"L","l"},{ 40,"XL","xl"},
        { 10,"X","x"},{  9,"IX","ix"},{  5,"V","v"},{  4,"IV","iv"},
        {  1,"I","i"},{  0,NULL,NULL}
    };
    buf[0] = '\0';
    if (n <= 0) { strcpy(buf, "0"); return; }
    for (int i = 0; t[i].v; i++) {
        while (n >= t[i].v) { strcat(buf, upper ? t[i].u : t[i].l); n -= t[i].v; }
    }
}

/* ── Marker helpers ──────────────────────────────────────────────────────── */

static void format_marker_val(const ScList *l, int idx, char *buf64) {
    int n = idx + 1;
    switch (l->opts.marker) {
    case SC_LIST_BULLET:
        strcpy(buf64, l->opts.bullet ? l->opts.bullet : "\xe2\x80\xa2"); /* U+2022 • */
        break;
    case SC_LIST_NUMBER:   snprintf(buf64, 32, "%d", n);                         break;
    case SC_LIST_ALPHA_LC: buf64[0] = (char)('a' + (n-1) % 26); buf64[1] = '\0'; break;
    case SC_LIST_ALPHA_UC: buf64[0] = (char)('A' + (n-1) % 26); buf64[1] = '\0'; break;
    case SC_LIST_ROMAN_LC: to_roman(n, buf64, 0);                                break;
    case SC_LIST_ROMAN_UC: to_roman(n, buf64, 1);                                break;
    }
}

static int marker_val_vis_w(const ScList *l, int idx) {
    char buf[64]; format_marker_val(l, idx, buf);
    return (int)sc_utf8_string_length(buf, strlen(buf));
}

static int max_marker_val_w(const ScList *l) {
    if (l->opts.marker == SC_LIST_BULLET) {
        const char *b = l->opts.bullet ? l->opts.bullet : "\xe2\x80\xa2";
        return (int)sc_utf8_string_length(b, strlen(b));
    }
    int mw = 0;
    for (size_t i = 0; i < l->count; i++) {
        int w = marker_val_vis_w(l, (int)i);
        if (w > mw) { mw = w; }
    }
    return mw ? mw : 1;
}

/* total visible width of the marker field (prefix + value + suffix + 1 space) */
static int total_marker_w(const ScList *l) {
    if (l->opts.marker == SC_LIST_BULLET) {
        const char *b = l->opts.bullet ? l->opts.bullet : "\xe2\x80\xa2";
        return (int)sc_utf8_string_length(b, strlen(b)) + 1;
    }
    const char *pre = l->opts.marker_prefix ? l->opts.marker_prefix : "";
    const char *suf = l->opts.marker_suffix ? l->opts.marker_suffix : ".";
    int pw = (int)sc_utf8_string_length(pre, strlen(pre));
    int sw = (int)sc_utf8_string_length(suf, strlen(suf));
    return pw + max_marker_val_w(l) + sw + 1;
}

/* zero-init ScTextStyle means "no formatting" (index==0 is SC_ANSI_COLOR_BLACK, -2 is SC_ANSI_COLOR_NONE) */
static int opts_has_format(ScTextStyle o) {
    return o.attr != 0 || o.fg.index != 0 || o.fg.r || o.fg.g || o.fg.b
                        || o.bg.index != 0 || o.bg.r || o.bg.g || o.bg.b;
}

/* ── Word wrap ────────────────────────────────────────────────────────────── */

static char **word_wrap(const char *text, int max_w, size_t *out_n) {
    size_t cap = 8, n = 0;
    char **lines = malloc(cap * sizeof(char *));

    if (!text || !*text || max_w < 1) {
        lines[n++] = strdup(text && *text ? text : "");
        *out_n = n; return lines;
    }

    const char *p = text;
    while (*p) {
        /* skip leading spaces at each wrapped-line start */
        while (*p == ' ') { p++; }
        if (!*p) { break; }

        const char *start    = p;
        const char *last_sp  = NULL;
        const char *last_sp_e = NULL;
        int vis = 0;

        while (*p && *p != '\n') {
            unsigned char c = *(unsigned char *)p;
            int bytes = (c < 0x80) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
            if (vis + 1 > max_w) { break; }
            if (*p == ' ') { last_sp = p; last_sp_e = p + 1; }
            vis++;
            p += bytes;
        }

        const char *end;
        if (!*p || *p == '\n') {
            end = p;
            if (*p == '\n') { p++; }
        } else if (last_sp) {
            end = last_sp; p = last_sp_e;
        } else {
            end = p; /* hard break */
        }

        while (end > start && *(end-1) == ' ') { end--; }

        if (n == cap) { cap *= 2; lines = realloc(lines, cap * sizeof(char *)); }
        lines[n++] = strndup(start, (size_t)(end - start));
    }

    if (n == 0) { lines[n++] = strdup(""); }
    *out_n = n;
    return lines;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScList *sc_list_new(ScListOpts opts) {
    ScList *l = calloc(1, sizeof(ScList));
    l->opts   = opts;
    return l;
}

static ScListItem *item_alloc(int is_text, const char *str, ScTextStyle opts,
                               const ScText *text) {
    ScListItem *it = calloc(1, sizeof(ScListItem));
    it->is_text = is_text;
    it->str     = str ? strdup(str) : NULL;
    it->opts    = opts;
    it->text    = text;
    return it;
}

static void list_push(ScList *l, ScListItem *it) {
    if (l->count == l->cap) {
        l->cap   = l->cap ? l->cap * 2 : 4;
        l->items = realloc(l->items, l->cap * sizeof(ScListItem *));
    }
    l->items[l->count++] = it;
}

ScListItem *sc_list_add_str(ScList *l, const char *str, ScTextStyle opts) {
    ScListItem *it = item_alloc(0, str, opts, NULL);
    list_push(l, it);
    return it;
}

ScListItem *sc_list_add_text(ScList *l, const ScText *t) {
    ScTextStyle none = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE };
    ScListItem *it = item_alloc(1, NULL, none, t);
    list_push(l, it);
    return it;
}

ScList *sc_list_add_sub(ScListItem *parent, ScListOpts opts) {
    sc_list_free(parent->sublist);
    parent->sublist = sc_list_new(opts);
    return parent->sublist;
}

/* ── Rendering ───────────────────────────────────────────────────────────── */

static void print_list_r(const ScList *l, int base_indent, int term_w);

static void print_list_r(const ScList *l, int base_indent, int term_w) {
    if (!l || !l->count) { return; }

    int mw         = total_marker_w(l);
    int max_vw     = max_marker_val_w(l);
    int list_left  = base_indent + l->opts.indent;
    int text_start = list_left + mw;

    int avail_w = term_w - l->opts.margin.left - l->opts.margin.right - text_start;
    if (avail_w < 4) { avail_w = 4; }

    int is_bullet = (l->opts.marker == SC_LIST_BULLET);
    const char *pre = is_bullet ? "" : (l->opts.marker_prefix ? l->opts.marker_prefix : "");
    const char *suf = is_bullet ? "" : (l->opts.marker_suffix ? l->opts.marker_suffix : ".");

    for (size_t i = 0; i < l->count; i++) {
        ScListItem *it = l->items[i];

        /* ── marker ── */
        char val[64];
        format_marker_val(l, (int)i, val);
        int vw  = (int)sc_utf8_string_length(val, strlen(val));
        int pad = max_vw - vw;  /* right-align value within field */

        char marker[256];
        if (is_bullet) {
            snprintf(marker, sizeof(marker), "%s ", val);
        } else {
            snprintf(marker, sizeof(marker), "%s%*s%s%s ", pre, pad, "", val, suf);
        }

        /* print left margin + list indent */
        for (int k = 0; k < l->opts.margin.left + list_left; k++) { fputc(' ', stdout); }
        if (opts_has_format(l->opts.marker_opts)) {
            sc_print(marker, l->opts.marker_opts);
        } else {
            fputs(marker, stdout);
        }

        /* ── text ── */
        if (it->is_text) {
            if (it->text) { sc_print_text(it->text); }
            fputc('\n', stdout);
        } else {
            const char *txt = it->str ? it->str : "";
            size_t nlines;
            char **wrapped = word_wrap(txt, avail_w, &nlines);
            for (size_t li = 0; li < nlines; li++) {
                if (li > 0) {
                    for (int k = 0; k < l->opts.margin.left + text_start; k++) { fputc(' ', stdout); }
                }
                sc_print(wrapped[li], it->opts);
                fputc('\n', stdout);
                free(wrapped[li]);
            }
            free(wrapped);
        }

        /* ── sublist ── */
        if (it->sublist) {
            print_list_r(it->sublist, text_start, term_w);
        }

        /* ── item gap ── */
        for (int g = 0; g < l->opts.item_gap; g++) { fputc('\n', stdout); }
    }
}

void sc_list_print(const ScList *l) {
    if (!l) { return; }
    for (int i = 0; i < l->opts.margin.top; i++) { fputc('\n', stdout); }
    int term_w = l->opts.width > 0 ? l->opts.width : sc_terminal_width();
    print_list_r(l, 0, term_w);
    for (int i = 0; i < l->opts.margin.bottom; i++) { fputc('\n', stdout); }
}

/* ── Memory management ───────────────────────────────────────────────────── */

static void item_free(ScListItem *it) {
    if (!it) { return; }
    free(it->str);
    sc_list_free(it->sublist);
    free(it);
}

void sc_list_free(ScList *l) {
    if (!l) { return; }
    for (size_t i = 0; i < l->count; i++) { item_free(l->items[i]); }
    free(l->items);
    free(l);
}
