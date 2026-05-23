#pragma once

#include "sparcli_core.h"

typedef enum {
    SC_PROGRESS_BLOCK,
    SC_PROGRESS_ASCII,
    SC_PROGRESS_LINE,
    SC_PROGRESS_SHADED,
} ScProgressStyle;

typedef struct {
    bool    enabled;
    double  mid;    /* ratio threshold for mid color (default 0.5) */
    double  high;   /* ratio threshold for high color (default 0.75) */
    ScColor color_low;
    ScColor color_mid;
    ScColor color_high;
} ScProgressThresholds;

typedef struct {
    ScProgressStyle      style;
    const char          *left_cap;
    const char          *right_cap;
    ScColor              fill_color;
    ScColor              empty_color;
    ScProgressThresholds thresholds;
    bool                 show_percent;
    bool             show_value;
    int              bar_width;
    int              width;
    int              label_width;
    ScTextStyle        label_opts;
} ScProgressBarOpts;

typedef struct ScProgressBar ScProgressBar;

ScProgressBar *sc_progressbar_new      (ScProgressBarOpts opts);
void           sc_progressbar_set_label(ScProgressBar *b, const char *label);
void           sc_progressbar_draw     (ScProgressBar *b, double value, double max);
void           sc_progressbar_finish   (ScProgressBar *b, double value, double max);
void           sc_progressbar_free     (ScProgressBar *b);
