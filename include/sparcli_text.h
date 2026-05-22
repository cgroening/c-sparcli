#pragma once

#include <stddef.h>
#include "sparcli_core.h"

typedef struct {
    char *raw_str;
    ScTextStyle style;
} ScSpan;

typedef struct {
    ScSpan *spans;
    size_t  count;
    size_t  cap;
} ScText;

ScText *sc_text_new(void);
void sc_text_append(ScText *sc_text, const char *raw_str, ScTextStyle style);
void sc_text_free(ScText *sc_text);
void sc_print_text(const ScText *sc_text);
size_t sc_text_visible_width(const ScText *sc_text);
