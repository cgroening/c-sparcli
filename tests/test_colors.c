#include "sparcli.h"
#include <stdio.h>


void test_colors(void) {
    ScOptions red       = { SC_STYLE_NONE, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions bold_red  = { SC_STYLE_BOLD, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions on_blue   = { SC_STYLE_NONE, SC_COLOR_WHITE, SC_COLOR_BLUE };
    ScOptions cyan_bold = { SC_STYLE_BOLD, SC_COLOR_CYAN,  SC_COLOR_NONE };
    ScOptions green_rgb = {
        SC_STYLE_NONE, sc_rgb(255, 192, 0), SC_COLOR_NONE
    };
    ScOptions rgb_bg    = {
        SC_STYLE_ITALIC, SC_COLOR_WHITE, sc_rgb(80, 0, 120)
    };

    sc_println("Red text",               red);
    sc_println("Bold red text",          bold_red);
    sc_println("White on blue",          on_blue);
    sc_println("Bold cyan",              cyan_bold);
    sc_println("RGB orange text",        green_rgb);
    sc_println("Italic white on purple", rgb_bg);

    // ScText *t = sc_text_new();
    // sc_text_append(t, "Red text", red);
    // sc_text_append(t, "Bold red text", bold_red);
    // sc_text_append(t, "White on blue", on_blue);
    // sc_text_append(t, "Bold cyan", cyan_bold);
    // sc_text_append(t, "RGB orange text", green_rgb);
    // sc_text_append(t, "Italic white on purple", rgb_bg);
    //
    // sc_align_text(t, SC_ALIGN_CENTER, 0);
    //
    // sc_text_free(t);

}
