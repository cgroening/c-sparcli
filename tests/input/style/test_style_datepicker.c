#include "test_style.h"
#include "input/input_internal.h"

#include <time.h>


/** Fixed seed so snapshots are deterministic: 2026-05-15. */
static struct tm seed_date(void) {
    struct tm t = { 0 };
    t.tm_year = 2026 - 1900;
    t.tm_mon  = 4;   /* May */
    t.tm_mday = 15;
    return t;
}

void style_datepicker(void) {
    struct tm seed = seed_date();

    /* Default (Monday week start, cyan accent). */
    style_show("date: default",
        sc_datepicker_frame(&seed, (ScDatePickerOpts){ .prompt = "Pick a date" }));

    /* Sunday week start + named accent. */
    style_show("date: Sunday start, accent=green",
        sc_datepicker_frame(&seed, (ScDatePickerOpts){
            .prompt = "Pick a date", .week_start = SC_WEEK_START_SUNDAY,
            .accent = SC_ANSI_COLOR_GREEN }));

    /* Custom header glyphs + styles. */
    style_show("date: custom < > glyphs, underline header, RGB selected",
        sc_datepicker_frame(&seed, (ScDatePickerOpts){
            .prompt = "Date",
            .header_prev = "<", .header_next = ">",
            .header_style   = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
                                SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE },
            .weekday_style  = { SC_TEXT_ATTR_ITALIC, SC_ANSI_COLOR_NONE,
                                SC_ANSI_COLOR_NONE },
            .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                                { -1, 255, 140, 0 } } }));

    /* Styled prompt heading. */
    style_show("prompt_style: italic cyan heading",
        sc_datepicker_frame(&seed, (ScDatePickerOpts){
            .prompt = "When?",
            .prompt_style = { SC_TEXT_ATTR_ITALIC, SC_ANSI_COLOR_CYAN,
                              SC_ANSI_COLOR_NONE } }));
}
