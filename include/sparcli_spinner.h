#pragma once

#include "sparcli_core.h"

typedef enum {
    SC_SPINNER_BRAILLE,
    SC_SPINNER_PIPE,
    SC_SPINNER_DOTS,
    SC_SPINNER_ARROW,
} ScSpinnerStyle;

typedef struct {
    ScSpinnerStyle style;
    ScColor        color;
    ScOptions      label_opts;
} ScSpinnerOpts;

typedef struct ScSpinner ScSpinner;

ScSpinner *sc_spinner_new      (const char *label, ScSpinnerOpts opts);
void       sc_spinner_set_label(ScSpinner *s, const char *label);
void       sc_spinner_tick     (ScSpinner *s);
void       sc_spinner_finish   (ScSpinner *s, int success, const char *label);
void       sc_spinner_free     (ScSpinner *s);
