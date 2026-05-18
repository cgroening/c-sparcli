#include "sparcli.h"


int main(void) {
    sc_println("",                SC_STYLE_NONE);
    sc_println("Normal text",     SC_STYLE_NONE);
    sc_println("Dim text",        SC_STYLE_DIM);
    sc_println("Bold text",       SC_STYLE_BOLD);
    sc_println("Italic text",     SC_STYLE_ITALIC);
    sc_println("Underlined text", SC_STYLE_UNDER);

    sc_println("",                        SC_STYLE_NONE);
    sc_println("Bold + Italic",           SC_STYLE_BOLD | SC_STYLE_ITALIC);
    sc_println("Bold + Underlined",       SC_STYLE_BOLD | SC_STYLE_UNDER);
    sc_println("Bold + Italic + Under",   SC_STYLE_BOLD | SC_STYLE_ITALIC | SC_STYLE_UNDER);

    return 0;
}
