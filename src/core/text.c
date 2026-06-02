#include "sparcli.h"
#include "core/text_internal.h"
#include "internal.h"

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
    // Public entry: user strings cross the trust boundary here
    char *clean = sc_sanitize_copy(raw_str, sc_allow_ansi());
    if (!clean) { return; }
    sc_text_append_raw(sc_text, clean, style);
    free(clean);
}

void sc_text_append_raw(
    ScText *sc_text, const char *raw_str, ScTextStyle style
) {
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

void sc_text_append_link(
    ScText *sc_text, const char *raw_str, const char *url, ScTextStyle style
) {
    if (!sc_text || !raw_str) { return; }

    // Public entry: the visible text crosses the trust boundary here
    char *clean_text = sc_sanitize_copy(raw_str, sc_allow_ansi());
    if (!clean_text) { return; }

    // The URL is reduced to printable ASCII so it cannot break out of the
    // OSC-8 sequence; an empty result degrades to a plain span.
    char *safe_url = url ? sc_osc8_scrub_url(url) : NULL;
    if (safe_url && safe_url[0] != '\0') {
        char *wrapped = sc_osc8_wrap(clean_text, safe_url);
        if (wrapped) {
            sc_text_append_raw(sc_text, wrapped, style);
            free(wrapped);
        }
    } else {
        sc_text_append_raw(sc_text, clean_text, style);
    }

    free(safe_url);
    free(clean_text);
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
                    sc_print_raw(seg, style);
                    free(seg);
                }
                fputc('\n', sc_output_stream());
                start = raw_str + 1;
            }
            raw_str++;
        }
        if (*start) {
            sc_print_raw(start, style);
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
        while (*current_byte) {
            if (*current_byte == 0x1B) {
                // ANSI escape sequences occupy no columns
                const char *seq_end =
                    sc_ansi_skip_seq((const char *)current_byte);
                current_byte = seq_end != (const char *)current_byte
                    ? (const unsigned char *)seq_end : current_byte + 1;
                continue;
            }
            if (*current_byte == '\n') {
                if (current_width > max_width) { max_width = current_width; }
                current_width = 0;
            } else if ((*current_byte & 0xC0) != 0x80) {
                // Skip UTF-8 continuation bytes
                current_width++;
            }
            current_byte++;
        }
    }
    return current_width > max_width ? current_width : max_width;
}
