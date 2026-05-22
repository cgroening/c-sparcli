#include "sparcli.h"
#include <stdlib.h>
#include <string.h>

ScText *sc_text_new(void) {
    ScText *sc_text = malloc(sizeof(ScText));
    if (!sc_text) return NULL;
    sc_text->spans = NULL;
    sc_text->count = 0;
    sc_text->cap   = 0;
    return sc_text;
}

void sc_text_append(ScText *sc_text, const char *text, ScTextStyle style) {
    if (sc_text->count == sc_text->cap) {
        size_t new_cap = sc_text->cap ? sc_text->cap * 2 : 4;
        ScSpan *s = realloc(sc_text->spans, new_cap * sizeof(ScSpan));
        if (!s) return;
        sc_text->spans = s;
        sc_text->cap   = new_cap;
    }
    sc_text->spans[sc_text->count].raw_str = strdup(text);
    sc_text->spans[sc_text->count].style = style;
    sc_text->count++;
}

void sc_text_free(ScText *t) {
    for (size_t i = 0; i < t->count; i++)
        free(t->spans[i].raw_str);
    free(t->spans);
    free(t);
}

void sc_print_text(const ScText *text) {
    for (size_t i = 0; i < text->count; i++) {
        const char *s     = text->spans[i].raw_str;
        ScTextStyle   style  = text->spans[i].style;
        const char *start = s;

        while (*s) {
            if (*s == '\n') {
                if (s > start) {
                    char *seg = strndup(start, (size_t)(s - start));
                    sc_print(seg, style);
                    free(seg);
                }
                fputc('\n', stdout);
                start = s + 1;
            }
            s++;
        }
        if (*start)
            sc_print(start, style);
    }
}

size_t sc_text_visible_width(const ScText *text) {
    size_t max_w = 0, cur_w = 0;
    for (size_t i = 0; i < text->count; i++) {
        for (const unsigned char *s = (const unsigned char *)text->spans[i].raw_str; *s; s++) {
            if (*s == '\n') {
                if (cur_w > max_w) max_w = cur_w;
                cur_w = 0;
            } else if ((*s & 0xC0) != 0x80) {  /* skip UTF-8 continuation bytes */
                cur_w++;
            }
        }
    }
    return cur_w > max_w ? cur_w : max_w;
}
