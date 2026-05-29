#include "test_style.h"
#include "input/input_internal.h"


static ScSelect *build(ScSelectOpts opts, const char *const *items, size_t n) {
    ScSelect *s = sc_select_new(opts);
    for (size_t i = 0; i < n; i++) { sc_select_add(s, items[i]); }
    return s;
}

void style_select(void) {
    const char *items[] = { "Apple", "Banana", "Cherry", "Date" };
    size_t n = sizeof items / sizeof items[0];

    /* Default single-select. */
    ScSelect *a = build((ScSelectOpts){ .prompt = "Pick one" }, items, n);
    style_show("select: default", sc_select_frame(a));
    sc_select_free(a);

    /* Custom accent + cursor marker + bold-italic row style. */
    ScSelect *b = build((ScSelectOpts){
        .prompt = "Pick one",
        .accent = SC_ANSI_COLOR_YELLOW,
        .cursor_marker = "\xe2\x86\x92 ",  /* → */
        .marker = "  ",
        .selected_style = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_ITALIC,
                            SC_ANSI_COLOR_YELLOW, SC_ANSI_COLOR_NONE },
    }, items, n);
    style_show("select: yellow accent, → marker, bold-italic row", sc_select_frame(b));
    sc_select_free(b);

    /* Multi-select with custom checkboxes; pre-check a couple of items. */
    ScSelect *c = build((ScSelectOpts){
        .prompt = "Pick many", .multi = true, .accent = SC_ANSI_COLOR_GREEN,
        .checkbox_on = "[\xe2\x9c\x94] ",   /* [✔] */
        .checkbox_off = "[ ] ",
    }, items, n);
    sc_select_check(c, 1, true);
    sc_select_check(c, 3, true);
    style_show("multi-select: custom check glyph, two checked", sc_select_frame(c));
    sc_select_free(c);
}
