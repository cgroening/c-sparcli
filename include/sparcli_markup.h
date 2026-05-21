#pragma once

#include "sparcli_text.h"

typedef struct {
    int strip_unknown; /* 1 = silently remove unrecognized tags; 0 = pass through verbatim (default) */
} ScMarkupOpts;

ScText *sc_markup_parse       (const char *s);
ScText *sc_markup_parse_opts  (const char *s,      ScMarkupOpts opts);
void    sc_markup_append      (ScText *t, const char *markup);
void    sc_markup_append_opts (ScText *t, const char *markup, ScMarkupOpts opts);
void    sc_markup_print       (const char *markup);
void    sc_markup_print_opts  (const char *markup,  ScMarkupOpts opts);
void    sc_markup_println     (const char *markup);
void    sc_markup_println_opts(const char *markup,  ScMarkupOpts opts);
