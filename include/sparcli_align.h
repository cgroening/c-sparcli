#pragma once

#include "sparcli_rendered.h"
#include "sparcli_types.h"
#include "sparcli_text.h"

/* width=0 uses sc_term_width(); width>0 uses that fixed width */
void sc_align_print(const ScRendered *r, ScAlign align, int width);
void sc_align_str  (const char *s,       ScAlign align, int width);
void sc_align_text (const ScText *t,     ScAlign align, int width);
