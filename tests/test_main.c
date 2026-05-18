#include "sparcli.h"


int main(void) {
    sc_println("",                SP_STYLE_NONE);
    sc_println("Normal text",     SP_STYLE_NONE);
    sc_println("Bold text",       SP_STYLE_BOLD);
    sc_println("Italic text",     SP_STYLE_ITALIC);
    sc_println("Underlined text", SP_STYLE_UNDER);

    sc_println("",                        SP_STYLE_NONE);
    sc_println("Bold + Italic",           SP_STYLE_BOLD | SP_STYLE_ITALIC);
    sc_println("Bold + Underlined",       SP_STYLE_BOLD | SP_STYLE_UNDER);
    sc_println("Bold + Italic + Under",   SP_STYLE_BOLD | SP_STYLE_ITALIC | SP_STYLE_UNDER);
    return 0;
}
