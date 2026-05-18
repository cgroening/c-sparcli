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
    ScOptions plain   = { SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions bold    = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };
    ScOptions red     = { SC_STYLE_NONE, SC_COLOR_RED,  SC_COLOR_NONE };
    ScOptions bold_ttl = { SC_STYLE_BOLD, SC_COLOR_NONE, SC_COLOR_NONE };

    printf("\n");

    /* No border */
    sc_panel_str("No border panel", (ScPanelOpts){
        SC_BORDER_NONE, SC_COLOR_NONE, NULL, plain,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 1, 0, 0, 1
    });

    printf("\n");

    /* ASCII border, title top-left */
    sc_panel_str("ASCII border", (ScPanelOpts){
        SC_BORDER_ASCII, SC_COLOR_NONE, "Title", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
    });

    printf("\n");

    /* Single border, title top-center, cyan, padding, multi-line */
    sc_panel_str("Single border\nSecond line\nThird line", (ScPanelOpts){
        SC_BORDER_SINGLE, SC_COLOR_CYAN, "Info", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_CENTER, 2, 1, 0, 1
    });

    printf("\n");

    /* Double border, title bottom-right, yellow */
    sc_panel_str("Double border", (ScPanelOpts){
        SC_BORDER_DOUBLE, SC_COLOR_YELLOW, "Warning", bold_ttl,
        SC_TITLE_BOTTOM, SC_ALIGN_RIGHT, 2, 0, 0, 1
    });

    printf("\n");

    /* Rounded border, no title, RGB border color */
    sc_panel_str("Rounded, no title", (ScPanelOpts){
        SC_BORDER_ROUNDED, sc_rgb(180, 100, 255), NULL, plain,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
    });

    printf("\n");

    /* Thick border, title top-left, green */
    sc_panel_str("Thick border", (ScPanelOpts){
        SC_BORDER_THICK, SC_COLOR_GREEN, "Thick", bold_ttl,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 0, 0, 1
    });

    printf("\n");

    /* ScText content with mixed styles inside a panel */
    ScOptions on_blue   = { SC_STYLE_NONE, SC_COLOR_BLACK, SC_COLOR_GREEN };

    ScText *t = sc_text_new();
    sc_text_append(t, "Normal ",    plain);
    sc_text_append(t, "bold ",      bold);
    sc_text_append(t, "and red\n",  red);
    sc_text_append(t, "second line", plain);
    sc_panel_text(t, (ScPanelOpts){
        SC_BORDER_ROUNDED, SC_COLOR_GREEN, " Styled Content ", on_blue,
        SC_TITLE_TOP, SC_ALIGN_LEFT, 2, 1, 0, 0
    });
    sc_text_free(t);
}
