#include "sparcli.h"
#include "output/sparcli_diff.h"
#include "output/sparcli_capture.h"

#include <stdlib.h>
#include <string.h>


/* Guard against a pathologically large LCS table; above this many cells we
 * fall back to a non-minimal (delete-all + add-all) diff to bound memory. */
#define DIFF_LCS_CELL_LIMIT 8000000u


/* ── line splitting ──────────────────────────────────────────────────────── */

typedef struct { char *buf; char **items; size_t count; } Lines;

/** Splits `s` into NUL-terminated lines (a trailing newline adds no empty
 *  line; empty input yields zero lines). Returns false on OOM. */
static bool split_lines(const char *s, Lines *out) {
    out->buf = NULL; out->items = NULL; out->count = 0;
    if (!s) { s = ""; }
    char *buf = strdup(s);
    if (!buf) { return false; }
    if (buf[0] == '\0') { out->buf = buf; return true; }   /* zero lines */

    size_t cap = 8, count = 0;
    char **items = malloc(cap * sizeof *items);
    if (!items) { free(buf); return false; }

    char *line = buf;
    for (char *p = buf;; p++) {
        if (*p == '\n' || *p == '\0') {
            bool end = (*p == '\0');
            *p = '\0';
            if (count == cap) {
                cap *= 2;
                char **grown = realloc(items, cap * sizeof *items);
                if (!grown) { free(items); free(buf); return false; }
                items = grown;
            }
            items[count++] = line;
            line = p + 1;
            if (end) { break; }
        }
    }
    /* Drop the empty trailing segment a final '\n' produces. */
    size_t slen = strlen(s);
    if (count > 0 && slen > 0 && s[slen - 1] == '\n') { count--; }

    out->buf = buf; out->items = items; out->count = count;
    return true;
}

static void free_lines(Lines *l) { free(l->buf); free(l->items); }


/* ── edit script (LCS backtrack) ─────────────────────────────────────────── */

typedef struct { char tag; size_t ai; size_t bj; } Op;  /* tag: ' ' '-' '+' */

/** Appends one op, growing the array. Returns false on OOM. */
static bool push_op(Op **ops, size_t *n, size_t *cap, Op op) {
    if (*n == *cap) {
        size_t nc = *cap ? *cap * 2 : 64;
        Op *g = realloc(*ops, nc * sizeof **ops);
        if (!g) { return false; }
        *ops = g; *cap = nc;
    }
    (*ops)[(*n)++] = op;
    return true;
}

/** Non-minimal fallback: all deletions, then all additions. */
static Op *fallback_ops(const Lines *a, const Lines *b, size_t *out_n) {
    Op *ops = NULL; size_t n = 0, cap = 0;
    for (size_t i = 0; i < a->count; i++) {
        if (!push_op(&ops, &n, &cap, (Op){ '-', i, 0 })) { free(ops); return NULL; }
    }
    for (size_t j = 0; j < b->count; j++) {
        if (!push_op(&ops, &n, &cap, (Op){ '+', 0, j })) { free(ops); return NULL; }
    }
    *out_n = n;
    return ops;
}

/** Computes the edit script via an LCS DP table. Returns NULL on OOM. */
static Op *diff_ops(const Lines *a, const Lines *b, size_t *out_n) {
    size_t n = a->count, m = b->count;
    if ((n + 1) * (m + 1) > DIFF_LCS_CELL_LIMIT) {
        return fallback_ops(a, b, out_n);
    }

    size_t w = m + 1;
    unsigned *dp = calloc((n + 1) * w, sizeof *dp);
    if (!dp) { return NULL; }

    for (size_t i = n; i-- > 0;) {
        for (size_t j = m; j-- > 0;) {
            if (strcmp(a->items[i], b->items[j]) == 0) {
                dp[i * w + j] = dp[(i + 1) * w + (j + 1)] + 1;
            } else {
                unsigned down = dp[(i + 1) * w + j];
                unsigned right = dp[i * w + (j + 1)];
                dp[i * w + j] = down >= right ? down : right;
            }
        }
    }

    Op *ops = NULL; size_t cnt = 0, cap = 0;
    size_t i = 0, j = 0;
    bool ok = true;
    while (i < n && j < m && ok) {
        if (strcmp(a->items[i], b->items[j]) == 0) {
            ok = push_op(&ops, &cnt, &cap, (Op){ ' ', i, j }); i++; j++;
        } else if (dp[(i + 1) * w + j] >= dp[i * w + (j + 1)]) {
            ok = push_op(&ops, &cnt, &cap, (Op){ '-', i, j }); i++;
        } else {
            ok = push_op(&ops, &cnt, &cap, (Op){ '+', i, j }); j++;
        }
    }
    while (i < n && ok) { ok = push_op(&ops, &cnt, &cap, (Op){ '-', i, 0 }); i++; }
    while (j < m && ok) { ok = push_op(&ops, &cnt, &cap, (Op){ '+', 0, j }); j++; }

    free(dp);
    if (!ok) { free(ops); return NULL; }
    *out_n = cnt;
    return ops;
}


/* ── rendering ───────────────────────────────────────────────────────────── */

static ScColor or_color(ScColor c, ScColor def) {
    return c.index != 0 ? c : def;
}

/** Appends `prefix + content + '\n'` as one styled span. */
static void append_line(ScText *t, char prefix, const char *content,
                        ScColor fg) {
    size_t len = strlen(content);
    char *buf = malloc(len + 3);
    if (!buf) { return; }
    buf[0] = prefix;
    memcpy(buf + 1, content, len);
    buf[len + 1] = '\n';
    buf[len + 2] = '\0';
    sc_text_append(t, buf, (ScTextStyle){ SC_TEXT_ATTR_NONE, fg,
                                          SC_ANSI_COLOR_NONE });
    free(buf);
}

/** Appends `s + '\n'` as one styled span. */
static void append_styled(ScText *t, const char *s, ScColor fg) {
    sc_text_append(t, s, (ScTextStyle){ SC_TEXT_ATTR_NONE, fg,
                                        SC_ANSI_COLOR_NONE });
    sc_text_append(t, "\n", (ScTextStyle){ 0 });
}

ScText *sc_diff_text(const char *old_text, const char *new_text,
                     ScDiffOpts opts) {
    ScColor add_c  = or_color(opts.add_color, SC_ANSI_COLOR_GREEN);
    ScColor del_c  = or_color(opts.del_color, SC_ANSI_COLOR_RED);
    ScColor hunk_c = or_color(opts.hunk_color, SC_ANSI_COLOR_CYAN);
    const char *old_label = opts.old_label ? opts.old_label : "old";
    const char *new_label = opts.new_label ? opts.new_label : "new";
    int context = opts.context == 0 ? 3 : opts.context;
    bool full = context < 0;

    Lines a = { 0 }, b = { 0 };
    ScText *t = sc_text_new();
    if (!t || !split_lines(old_text, &a) || !split_lines(new_text, &b)) {
        free_lines(&a); free_lines(&b);
        return t;
    }

    size_t nops = 0;
    Op *ops = diff_ops(&a, &b, &nops);
    if (!ops) { free_lines(&a); free_lines(&b); return t; }

    /* Decide which ops to show: every change op, plus `context` around it. */
    char *show = calloc(nops ? nops : 1, 1);
    if (!show) { free(ops); free_lines(&a); free_lines(&b); return t; }
    bool any_change = false;
    for (size_t k = 0; k < nops; k++) {
        if (ops[k].tag == ' ') { continue; }
        any_change = true;
        if (full) { memset(show, 1, nops); break; }
        size_t lo = (k >= (size_t)context) ? k - (size_t)context : 0;
        size_t hi = k + (size_t)context;
        if (hi >= nops) { hi = nops - 1; }
        for (size_t x = lo; x <= hi; x++) { show[x] = 1; }
    }

    if (!any_change) {
        free(show); free(ops); free_lines(&a); free_lines(&b);
        return t;   /* identical → empty diff */
    }

    if (!opts.no_header) {
        char h[160];
        snprintf(h, sizeof h, "--- %s", old_label);
        append_styled(t, h, del_c);
        snprintf(h, sizeof h, "+++ %s", new_label);
        append_styled(t, h, add_c);
    }

    /* Walk ops, emitting hunks (maximal runs of shown ops). Track 1-based
       old/new line numbers as we consume lines. */
    size_t oldno = 0, newno = 0;   /* lines consumed so far */
    size_t k = 0;
    while (k < nops) {
        if (!show[k]) {
            if (ops[k].tag != '+') { oldno++; }
            if (ops[k].tag != '-') { newno++; }
            k++;
            continue;
        }
        /* Start a hunk: snapshot positions, find run end. */
        size_t old_before = oldno, new_before = newno;
        size_t start = k, old_cnt = 0, new_cnt = 0;
        while (k < nops && show[k]) {
            if (ops[k].tag != '+') { old_cnt++; }
            if (ops[k].tag != '-') { new_cnt++; }
            k++;
        }

        char hdr[96];
        snprintf(hdr, sizeof hdr, "@@ -%zu,%zu +%zu,%zu @@",
                 old_cnt ? old_before + 1 : old_before, old_cnt,
                 new_cnt ? new_before + 1 : new_before, new_cnt);
        append_styled(t, hdr, hunk_c);

        for (size_t x = start; x < k; x++) {
            Op o = ops[x];
            if (o.tag == ' ') {
                append_line(t, ' ', a.items[o.ai], SC_ANSI_COLOR_NONE);
            } else if (o.tag == '-') {
                append_line(t, '-', a.items[o.ai], del_c);
            } else {
                append_line(t, '+', b.items[o.bj], add_c);
            }
        }
    }

    free(show); free(ops); free_lines(&a); free_lines(&b);
    return t;
}

void sc_diff_print(const char *old_text, const char *new_text,
                   ScDiffOpts opts) {
    ScText *t = sc_diff_text(old_text, new_text, opts);
    sc_print_text(t);
    sc_text_free(t);
}

ScRendered *sc_capture_diff(const char *old_text, const char *new_text,
                            ScDiffOpts opts) {
    ScText *t = sc_diff_text(old_text, new_text, opts);
    ScRendered *r = sc_capture_text(t);
    sc_text_free(t);
    return r;
}
