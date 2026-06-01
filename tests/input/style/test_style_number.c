#include "test_style.h"
#include "input/input_internal.h"


void style_number(void) {
    style_show("number: inline, bounded (range shown)",
        sc_number_frame("Quantity", 10,
            (ScNumberOpts){ .min = 0, .max = 100, .step = 5 }));

    style_show("number: inline, 2 decimals",
        sc_number_frame("Price", 9.99,
            (ScNumberOpts){
                .min = 0, .max = 1000, .step = 0.5, .decimals = 2 }));

    style_show("number: comma decimal separator",
        sc_number_frame("Amount", 12.99,
            (ScNumberOpts){
                .min = 0, .max = 100, .step = 0.5, .decimals = 2,
                .decimal_sep = ',' }));

    style_show("number: boxed (panel), range on bottom-right",
        sc_number_frame("Quantity", 42,
            (ScNumberOpts){
                .min = 0, .max = 100, .boxed = true, .width = 28 }));

    style_show("number: boxed, double border + cyan prompt",
        sc_number_frame("Port", 8080,
            (ScNumberOpts){ .min = 1, .max = 65535, .boxed = true, .width = 30,
                .border = {
                    .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_BLUE
                },
                .prompt_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN,
                                  SC_ANSI_COLOR_NONE } }));
}
