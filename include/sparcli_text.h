#pragma once

#include "sparcli_core.h"
#include <stddef.h>

typedef struct {
    char     *text;
    ScTextStyle opts;
} ScSpan;

typedef struct {
    ScSpan *spans;
    size_t  count;
    size_t  cap;
} ScText;

ScText *sc_text_new(void);
void    sc_text_append(ScText *t, const char *text, ScTextStyle opts);
void    sc_text_free(ScText *t);
void    sc_print_text(const ScText *t);
size_t  sc_text_visible_width(const ScText *t);
