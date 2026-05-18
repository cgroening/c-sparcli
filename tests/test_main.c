#include "sparcli.h"
#include <stdio.h>


void test_styles(void);
void test_colors(void);
void test_panels(void);


int main(void) {
    test_styles();
    test_colors();
    test_panels();
    return 0;
}


void test_styles(void) {
    ScOptions plain       = { SC_STYLE_NONE,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions dim         = { SC_STYLE_DIM,    SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold        = { SC_STYLE_BOLD,   SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions italic      = { SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions underline   = { SC_STYLE_UNDER,  SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold_italic = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_under = {
        SC_STYLE_BOLD | SC_STYLE_UNDER, SC_COLOR_NONE, SC_COLOR_NONE
    };
    ScOptions bold_italic_under = {
        SC_STYLE_BOLD | SC_STYLE_ITALIC | SC_STYLE_UNDER,
        SC_COLOR_NONE,
        SC_COLOR_NONE
    };

    sc_println("Plain text",             plain);
    sc_println("Dim text",               dim);
    sc_println("Bold text",              bold);
    sc_println("Italic text",            italic);
    sc_println("Underlined text",        underline);

    sc_println("", plain);
    sc_println("Bold + Italic",          bold_italic);
    sc_println("Bold + Underlined",      bold_under);
    sc_println("Bold + Italic + Under",  bold_italic_under);
}


void test_colors(void) {
    ScOptions red       = { SC_STYLE_NONE, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions bold_red  = { SC_STYLE_BOLD, SC_COLOR_RED,   SC_COLOR_NONE };
    ScOptions on_blue   = { SC_STYLE_NONE, SC_COLOR_WHITE, SC_COLOR_BLUE };
    ScOptions cyan_bold = { SC_STYLE_BOLD, SC_COLOR_CYAN,  SC_COLOR_NONE };
    ScOptions green_rgb = {
        SC_STYLE_NONE, sc_rgb(77, 255, 0), SC_COLOR_NONE
    };
    ScOptions rgb_bg    = {
        SC_STYLE_ITALIC, SC_COLOR_WHITE, sc_rgb(80, 0, 120)
    };

    printf("\n");
    sc_println("Red text",               red);
    sc_println("Bold red text",          bold_red);
    sc_println("White on blue",          on_blue);
    sc_println("Bold cyan",              cyan_bold);
    sc_println("RGB orange text",        green_rgb);
    sc_println("Italic white on purple", rgb_bg);
}


void test_panels(void) {
    // Panels are not implemented yet, this is a placeholder for future tests.
}
