#pragma once

#include "sparcli_types.h"

typedef enum {
    SC_PROGRESS_BLOCK,
    SC_PROGRESS_ASCII,
    SC_PROGRESS_LINE,
    SC_PROGRESS_SHADED,
} ScProgressStyle;

typedef struct {
    ScProgressStyle  style;
    const char      *left_cap;
    const char      *right_cap;
    ScColor          fill_color;
    ScColor          empty_color;
    int              use_thresholds;
    double           threshold_mid;
    double           threshold_high;
    ScColor          color_low;
    ScColor          color_mid;
    ScColor          color_high;
    int              show_percent;
    int              show_value;
    int              bar_width;
    int              width;
    int              label_width;
    ScOptions        label_opts;
} ScProgressBarOpts;

typedef struct ScProgressBar ScProgressBar;

ScProgressBar *sc_progressbar_new      (ScProgressBarOpts opts);
void           sc_progressbar_set_label(ScProgressBar *b, const char *label);
void           sc_progressbar_draw     (ScProgressBar *b, double value, double max);
void           sc_progressbar_finish   (ScProgressBar *b, double value, double max);
void           sc_progressbar_free     (ScProgressBar *b);
