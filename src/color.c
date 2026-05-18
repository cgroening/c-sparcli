#include "sparcli.h"
#include "internal.h"

ScColor sc_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return (ScColor){ -1, r, g, b };
}

static void apply_fg(ScColor c) {
    if (c.index == -2) return;
    if (c.index == -1) fprintf(stdout, "\033[38;2;%d;%d;%dm", c.r, c.g, c.b);
    else               fprintf(stdout, "\033[3%dm", c.index);
}

static void apply_bg(ScColor c) {
    if (c.index == -2) return;
    if (c.index == -1) fprintf(stdout, "\033[48;2;%d;%d;%dm", c.r, c.g, c.b);
    else               fprintf(stdout, "\033[4%dm", c.index);
}

void sc_apply_colors(ScColor fg, ScColor bg) {
    apply_fg(fg);
    apply_bg(bg);
}
