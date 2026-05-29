#include "test_style.h"
#include "input/input_internal.h"


void style_confirm(void) {
    /* Default look (green accent), Yes preselected. */
    style_show("default, Yes selected",
        sc_confirm_frame("Deploy?", true, (ScConfirmOpts){ 0 }));

    /* Named accent + custom labels. */
    style_show("accent=magenta, custom labels, No selected",
        sc_confirm_frame("Overwrite file?", false, (ScConfirmOpts){
            .accent = SC_ANSI_COLOR_MAGENTA,
            .yes_label = "Replace", .no_label = "Keep" }));

    /* RGB accent. */
    style_show("accent=rgb(255,140,0)",
        sc_confirm_frame("Continue?", true, (ScConfirmOpts){
            .accent = { -1, 255, 140, 0 } }));

    /* Fully custom selected/unselected styles. */
    style_show("custom selected (bold underline cyan) + unselected (italic)",
        sc_confirm_frame("Proceed?", true, (ScConfirmOpts){
            .selected_style   = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
                                  SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
            .unselected_style = { SC_TEXT_ATTR_ITALIC, SC_ANSI_COLOR_NONE,
                                  SC_ANSI_COLOR_NONE } }));
}
