#include "sparcli.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Color name table ────────────────────────────────────────────────────── */

static const struct { const char *name; ScColor color; } color_map[] = {
    { "black",   { 0, 0, 0, 0 } },
    { "red",     { 1, 0, 0, 0 } },
    { "green",   { 2, 0, 0, 0 } },
    { "yellow",  { 3, 0, 0, 0 } },
    { "blue",    { 4, 0, 0, 0 } },
    { "magenta", { 5, 0, 0, 0 } },
    { "cyan",    { 6, 0, 0, 0 } },
    { "white",   { 7, 0, 0, 0 } },
    { NULL, { 0, 0, 0, 0 } },
};

/* ── Helpers ─────────────────────────────────────────────────────────────── */

static int lookup_color(const char *s, size_t len, ScColor *out) {
    for (int i = 0; color_map[i].name; i++) {
        if (strlen(color_map[i].name) == len && memcmp(s, color_map[i].name, len) == 0) {
            *out = color_map[i].color;
            return 1;
        }
    }
    return 0;
}

/* Returns 1 if (s, len) is a recognized bare style or color name. */
static int is_recognized_name(const char *s, size_t len) {
    static const struct { const char *n; size_t l; } styles[] = {
        {"bold",9},{"italic",6},{"underline",9},{"u",1},{"dim",3},{NULL,0}
    };
    /* hand-coded lengths to avoid strlen at runtime */
    static const size_t slens[] = { 4, 6, 9, 1, 3 };
    for (int i = 0; styles[i].n; i++) {
        if (slens[i] == len && memcmp(s, styles[i].n, len) == 0) return 1;
    }
    ScColor dummy;
    return lookup_color(s, len, &dummy);
}

/* Parse all space-separated tokens in [content, content+len).
 * Inherits *base; accumulates results into *out.
 * Returns 1 if ALL tokens recognized, 0 if any token is unknown. */
static int parse_open_tag(const char *content, size_t len,
                          const ScOptions *base, ScOptions *out) {
    *out = *base;
    const char *p   = content;
    const char *end = content + len;

    while (p < end) {
        while (p < end && *p == ' ') p++;
        if (p >= end) break;

        int is_bg = 0;
        if ((size_t)(end - p) >= 3 && memcmp(p, "on ", 3) == 0) {
            is_bg = 1;
            p += 3;
            while (p < end && *p == ' ') p++;
            if (p >= end) return 0;
        }

        /* rgb(r,g,b) */
        if ((size_t)(end - p) >= 4 && memcmp(p, "rgb(", 4) == 0) {
            const char *paren_end = memchr(p, ')', (size_t)(end - p));
            if (!paren_end) return 0;
            int r, g, b;
            if (sscanf(p + 4, "%d,%d,%d", &r, &g, &b) != 3) return 0;
            if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return 0;
            ScColor c = sc_color_from_rgb((uint8_t)r, (uint8_t)g, (uint8_t)b);
            if (is_bg) out->bg = c; else out->fg = c;
            p = paren_end + 1;
            continue;
        }

        /* bare token */
        const char *tok = p;
        while (p < end && *p != ' ') p++;
        size_t tok_len = (size_t)(p - tok);

        if (!is_bg) {
            if (tok_len == 4 && memcmp(tok, "bold",      4) == 0) { out->style |= SC_STYLE_BOLD;   continue; }
            if (tok_len == 6 && memcmp(tok, "italic",    6) == 0) { out->style |= SC_STYLE_ITALIC; continue; }
            if (tok_len == 9 && memcmp(tok, "underline", 9) == 0) { out->style |= SC_STYLE_UNDER;  continue; }
            if (tok_len == 1 && memcmp(tok, "u",         1) == 0) { out->style |= SC_STYLE_UNDER;  continue; }
            if (tok_len == 3 && memcmp(tok, "dim",       3) == 0) { out->style |= SC_STYLE_DIM;    continue; }
            ScColor c;
            if (lookup_color(tok, tok_len, &c)) { out->fg = c; continue; }
        } else {
            ScColor c;
            if (lookup_color(tok, tok_len, &c)) { out->bg = c; continue; }
        }

        return 0;
    }

    return 1;
}

static void append_chunk(ScText *t, const char *s, size_t len, ScOptions opts) {
    if (len == 0) return;
    char *chunk = strndup(s, len);
    sc_text_append(t, chunk, opts);
    free(chunk);
}

/* ── Core parser ─────────────────────────────────────────────────────────── */

static ScText *parse_internal(const char *s, int strip_unknown) {
    if (!s) return sc_text_new();

    ScText   *t     = sc_text_new();
    ScOptions stack[32];
    int       depth = 0;
    stack[0] = (ScOptions){ SC_STYLE_NONE, { -2, 0, 0, 0 }, { -2, 0, 0, 0 } };

    const char *p         = s;
    const char *seg_start = s;

    while (*p) {
        /* [[ → literal '[' */
        if (p[0] == '[' && p[1] == '[') {
            append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);
            sc_text_append(t, "[", stack[depth]);
            p += 2;
            seg_start = p;
            continue;
        }

        if (p[0] == '[') {
            const char *end = strchr(p + 1, ']');
            if (!end) { p++; continue; }

            const char *tag_content = p + 1;
            size_t      tag_len     = (size_t)(end - tag_content);

            if (tag_len > 0 && tag_content[0] == '/') {
                /* closing tag */
                const char *rest     = tag_content + 1;
                size_t      rest_len = tag_len - 1;
                if (rest_len == 0 || is_recognized_name(rest, rest_len)) {
                    append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);
                    if (depth > 0) depth--;
                    p = end + 1;
                    seg_start = p;
                } else {
                    /* unknown close */
                    if (strip_unknown) {
                        append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);
                        seg_start = end + 1;
                    }
                    p = end + 1;
                }
            } else if (tag_len > 0) {
                /* opening tag */
                ScOptions new_opts;
                if (parse_open_tag(tag_content, tag_len, &stack[depth], &new_opts)) {
                    append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);
                    if (depth + 1 < 32) stack[++depth] = new_opts;
                    p = end + 1;
                    seg_start = p;
                } else {
                    /* unknown open */
                    if (strip_unknown) {
                        append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);
                        seg_start = end + 1;
                    }
                    p = end + 1;
                }
            } else {
                p = end + 1;  /* empty [] → literal */
            }
            continue;
        }

        p++;
    }

    /* flush remaining segment */
    append_chunk(t, seg_start, (size_t)(p - seg_start), stack[depth]);

    return t;
}

/* ── Public API ──────────────────────────────────────────────────────────── */

ScText *sc_markup_parse(const char *s) {
    return parse_internal(s, 0);
}

ScText *sc_markup_parse_opts(const char *s, ScMarkupOpts opts) {
    return parse_internal(s, opts.strip_unknown);
}

void sc_markup_append(ScText *t, const char *markup) {
    ScText *tmp = parse_internal(markup, 0);
    for (size_t i = 0; i < tmp->count; i++)
        sc_text_append(t, tmp->spans[i].text, tmp->spans[i].opts);
    sc_text_free(tmp);
}

void sc_markup_append_opts(ScText *t, const char *markup, ScMarkupOpts opts) {
    ScText *tmp = parse_internal(markup, opts.strip_unknown);
    for (size_t i = 0; i < tmp->count; i++)
        sc_text_append(t, tmp->spans[i].text, tmp->spans[i].opts);
    sc_text_free(tmp);
}

void sc_markup_print(const char *markup) {
    ScText *t = parse_internal(markup, 0);
    sc_print_text(t);
    sc_text_free(t);
}

void sc_markup_print_opts(const char *markup, ScMarkupOpts opts) {
    ScText *t = parse_internal(markup, opts.strip_unknown);
    sc_print_text(t);
    sc_text_free(t);
}

void sc_markup_println(const char *markup) {
    sc_markup_print(markup);
    fputc('\n', stdout);
}

void sc_markup_println_opts(const char *markup, ScMarkupOpts opts) {
    sc_markup_print_opts(markup, opts);
    fputc('\n', stdout);
}
