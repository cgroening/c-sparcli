#pragma once

#include "sparcli_rendered.h"
#include "sparcli_text.h"

typedef struct {
    int top;
    int right;   /* trailing spaces per line; mainly useful in composed contexts */
    int bottom;
    int left;
} ScPadOpts;

void sc_pad_print(const ScRendered *r, ScPadOpts opts);
void sc_pad_str  (const char *s,       ScPadOpts opts);
void sc_pad_text (const ScText *t,     ScPadOpts opts);
