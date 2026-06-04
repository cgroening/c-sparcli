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
    style_show("select: yellow accent, → marker, bold-italic row",
               sc_select_frame(b));
    sc_select_free(b);

    /* Multi-select with custom checkboxes; pre-check a couple of items. */
    ScSelect *c = build((ScSelectOpts){
        .prompt = "Pick many", .multi = true, .accent = SC_ANSI_COLOR_GREEN,
        .checkbox_on = "[\xe2\x9c\x94] ",   /* [✔] */
        .checkbox_off = "[ ] ",
    }, items, n);
    sc_select_set_checked(c, 1, true);
    sc_select_set_checked(c, 3, true);
    style_show("multi-select: custom check glyph, two checked",
               sc_select_frame(c));
    sc_select_free(c);

    /* Styled prompt heading. */
    ScSelect *d = build((ScSelectOpts){
        .prompt = "Choose environment",
        .prompt_style = { SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_UNDER,
                          SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE },
    }, items, n);
    style_show("prompt_style: bold underline green heading",
               sc_select_frame(d));
    sc_select_free(d);

    /* Long list with a small viewport: shows the scroll indicator + footer. */
    const char *many[] = { "Red", "Green", "Blue", "Yellow", "Cyan",
                           "Magenta", "White", "Black" };
    ScSelect *e = build(
        (ScSelectOpts){ .prompt = "Pick a color", .max_visible = 4 },
        many, sizeof many / sizeof many[0]);
    sc_select_set_cursor(e, 5);  /* scrolled into the middle */
    style_show("scroll indicator (max_visible 4, cursor on #6)",
               sc_select_frame(e));
    sc_select_free(e);

    /* Boxed: rounded frame (default border), fixed width. */
    ScSelect *f = build((ScSelectOpts){
        .prompt = "Pick one",
        .box = { .enabled = true, .width = 30 },
    }, items, n);
    style_show("boxed: default rounded frame, width 30", sc_select_frame(f));
    sc_select_free(f);

    /* Boxed: double border in cyan, blue content background, padding + margin. */
    ScSelect *g = build((ScSelectOpts){
        .prompt = "Pick one",
        .box = {
            .enabled = true, .width = 32,
            .border = { .type = SC_BORDER_DOUBLE, .color = SC_ANSI_COLOR_CYAN },
            .bg = SC_ANSI_COLOR_BLUE,
            .padding = { .top = 1, .right = 2, .bottom = 1, .left = 2 },
            .margin = { .left = 2 },
        },
    }, items, n);
    style_show("boxed: double cyan border, blue bg, padding 1/2, margin l2",
               sc_select_frame(g));
    sc_select_free(g);

    /* Widget background without a frame: rows inherit it, cursor row is a
     * full-width highlight bar (selected_style.bg). */
    ScSelect *h = build((ScSelectOpts){
        .prompt = "Pick one",
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            SC_ANSI_COLOR_MAGENTA },
        .box = { .bg = SC_ANSI_COLOR_BLACK },
    }, items, n);
    sc_select_set_cursor(h, 1);
    style_show("borderless bg: black widget bg, magenta full-width cursor bar",
               sc_select_frame(h));
    sc_select_free(h);

    /* bg_extent = text: the cursor background hugs the text only. */
    ScSelect *i = build((ScSelectOpts){
        .prompt = "Pick one",
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_WHITE,
                            SC_ANSI_COLOR_MAGENTA },
        .box = { .bg_extent = SC_BG_EXTENT_TEXT },
    }, items, n);
    sc_select_set_cursor(i, 1);
    style_show("bg_extent=text: cursor background hugs the text",
               sc_select_frame(i));
    sc_select_free(i);

    /* Content width with a minimum: the bar is at least 24 columns wide. */
    ScSelect *j = build((ScSelectOpts){
        .prompt = "Pick",
        .selected_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_BLACK,
                            SC_ANSI_COLOR_CYAN },
        .box = { .width_mode = SC_WIDTH_CONTENT, .min_width = 24,
                 .bg = SC_ANSI_COLOR_BLUE },
    }, items, n);
    style_show("width content, min_width 24 (bg fills to >= 24)",
               sc_select_frame(j));
    sc_select_free(j);

    /* Fixed width frame with a content background. */
    ScSelect *k = build((ScSelectOpts){
        .prompt = "Pick one",
        .box = { .enabled = true, .width_mode = SC_WIDTH_FIXED, .width = 24,
                 .bg = SC_ANSI_COLOR_BLUE },
    }, items, n);
    style_show("width fixed 24, blue bg, rounded frame", sc_select_frame(k));
    sc_select_free(k);
}
