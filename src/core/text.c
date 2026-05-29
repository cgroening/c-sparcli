#include "sparcli.h"
#include "core/text_internal.h"

#include <stdlib.h>
#include <string.h>

ScText *sc_text_new(void) {
    ScText *sc_text = malloc(sizeof(ScText));
    if (!sc_text) { return NULL; }
    sc_text->spans = NULL;
    sc_text->count = 0;
    sc_text->capacity = 0;
    return sc_text;
}

ScText *sc_text_from_str(const char *s) {
    static const ScTextStyle plain = {
        SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
    };
    ScText *t = sc_text_new();
    sc_text_append(t, s, plain);
    return t;
}

void sc_text_append(ScText *sc_text, const char *raw_str, ScTextStyle style) {
    if (!sc_text || !raw_str) { return; }
    if (sc_text->count == sc_text->capacity) {
        size_t new_capacity = sc_text->capacity ? sc_text->capacity * 2 : 4;
        ScSpan *spans = realloc(
            sc_text->spans, new_capacity * sizeof(ScSpan)
        );
        if (!spans) { return; }
        sc_text->spans = spans;
        sc_text->capacity = new_capacity;
    }
    char *copy = strdup(raw_str);
    if (!copy) { return; }
    sc_text->spans[sc_text->count].raw_str = copy;
    sc_text->spans[sc_text->count].style = style;
    sc_text->count++;
}

void sc_text_free(ScText *sc_text) {
    if (!sc_text) { return; }
    for (size_t i = 0; i < sc_text->count; i++) {
        free(sc_text->spans[i].raw_str);
    }
    free(sc_text->spans);
    free(sc_text);
}

void sc_print_text(const ScText *sc_text) {
    if (!sc_text) { return; }
    for (size_t i = 0; i < sc_text->count; i++) {
        const char *raw_str = sc_text->spans[i].raw_str;
        ScTextStyle style = sc_text->spans[i].style;
        const char *start = raw_str;

        while (*raw_str) {
            if (*raw_str == '\n') {
                if (raw_str > start) {
                    char *seg = strndup(start, (size_t)(raw_str - start));
                    sc_print(seg, style);
                    free(seg);
                }
                fputc('\n', sc_output_stream());
                start = raw_str + 1;
            }
            raw_str++;
        }
        if (*start) {
            sc_print(start, style);
        }
    }
}

size_t sc_text_span_count(const ScText *text) {
    return text ? text->count : 0;
}

ScSpan sc_text_span(const ScText *text, size_t index) {
    if (!text || index >= text->count) {
        return (ScSpan){ NULL, { 0, { 0, 0, 0, 0 }, { 0, 0, 0, 0 } } };
    }
    return text->spans[index];
}

size_t sc_text_visible_width(const ScText *sc_text) {
    if (!sc_text) { return 0; }
    size_t max_width = 0, current_width = 0;
    for (size_t i = 0; i < sc_text->count; i++) {
        const unsigned char *current_byte =
            (const unsigned char *)sc_text->spans[i].raw_str;
        for (; *current_byte; current_byte++) {
            if (*current_byte == '\n') {
                if (current_width > max_width) { max_width = current_width; }
                current_width = 0;
            } else if ((*current_byte & 0xC0) != 0x80) {
                // Skip UTF-8 continuation bytes
                current_width++;
            }
        }
    }
    return current_width > max_width ? current_width : max_width;
}
