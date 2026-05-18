#include "sparcli.h"
#include <stdlib.h>
#include <string.h>

ScText *sc_text_new(void) {
    ScText *t = malloc(sizeof(ScText));
    if (!t) return NULL;
    t->spans = NULL;
    t->count = 0;
    t->cap   = 0;
    return t;
}

void sc_text_append(ScText *t, const char *text, ScOptions opts) {
    if (t->count == t->cap) {
        size_t new_cap = t->cap ? t->cap * 2 : 4;
        ScSpan *s = realloc(t->spans, new_cap * sizeof(ScSpan));
        if (!s) return;
        t->spans = s;
        t->cap   = new_cap;
    }
    t->spans[t->count].text = strdup(text);
    t->spans[t->count].opts = opts;
    t->count++;
}

void sc_text_free(ScText *t) {
    for (size_t i = 0; i < t->count; i++)
        free(t->spans[i].text);
    free(t->spans);
    free(t);
}

void sc_print_text(const ScText *t) {
    for (size_t i = 0; i < t->count; i++) {
        const char *s     = t->spans[i].text;
        ScOptions   opts  = t->spans[i].opts;
        const char *start = s;

        while (*s) {
            if (*s == '\n') {
                if (s > start) {
                    char *seg = strndup(start, (size_t)(s - start));
                    sc_print(seg, opts);
                    free(seg);
                }
                fputc('\n', stdout);
                start = s + 1;
            }
            s++;
        }
        if (*start)
            sc_print(start, opts);
    }
}

size_t sc_text_visible_width(const ScText *t) {
    size_t max_w = 0, cur_w = 0;
    for (size_t i = 0; i < t->count; i++) {
        for (const char *s = t->spans[i].text; *s; s++) {
            if (*s == '\n') {
                if (cur_w > max_w) max_w = cur_w;
                cur_w = 0;
            } else {
                cur_w++;
            }
        }
    }
    return cur_w > max_w ? cur_w : max_w;
}
