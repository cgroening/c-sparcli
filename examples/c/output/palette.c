/*
 * palette.c - the named 24-bit RGB palette (SC_COLOR_*).
 *
 * Additional to the 8 ANSI colors (SC_ANSI_COLOR_*), sparcli ships a curated
 * truecolor palette in <core/sparcli_palette.h> (pulled in by <sparcli.h>):
 * standard hues + _VIVID / _DARK variants, bg/fg shades, accents, status and
 * diagnostic colors. They are plain ScColor values, so they work anywhere a
 * color does - styles, borders, backgrounds - and by name in markup, the CLI
 * and the args parser via sc_color_by_name().
 *
 * Run (after `make`):
 *   make run-example EX=c/output/palette
 */

#include <sparcli.h>

#include <stdio.h>

/** Prints `n` block glyphs (U+2588) in foreground color `fg`. */
static void blocks(ScColor fg, int n) {
    char buf[64 * 3 + 1];
    int p = 0;
    if (n < 0) { n = 0; }
    if (n > 64) { n = 64; }
    for (int i = 0; i < n; i++) {
        buf[p++] = (char)0xe2; buf[p++] = (char)0x96; buf[p++] = (char)0x88;
    }
    buf[p] = '\0';
    sc_print(buf, (ScTextStyle){ .fg = fg });
}

/** A bold, left-aligned section header. */
static void section(const char *title) {
    printf("\n");
    sc_rule_str(title, (ScRuleOpts){
        .title = { .halign = SC_ALIGN_LEFT, .pad = 1,
                   .style = { .attr = SC_TEXT_ATTR_BOLD } },
        .color = SC_ANSI_COLOR_NONE,
    });
}

/** Prints one "swatch  name" cell (fixed width), no newline. */
static void swatch(const char *name, ScColor color) {
    char label[20];
    snprintf(label, sizeof label, " %-15s", name);
    sc_print("  ", (ScTextStyle){ 0 });
    blocks(color, 3);
    sc_print(label, (ScTextStyle){ .attr = SC_TEXT_ATTR_DIM });
}

/** Prints a labelled row of swatches and a trailing newline. */
static void row(const char *a_name, ScColor a,
                const char *b_name, ScColor b,
                const char *c_name, ScColor c) {
    swatch(a_name, a);
    swatch(b_name, b);
    swatch(c_name, c);
    printf("\n");
}

int main(void) {
    section("Standard hues  (SC_COLOR_*)");
    row("red",     SC_COLOR_RED,     "orange",  SC_COLOR_ORANGE,
        "yellow",  SC_COLOR_YELLOW);
    row("green",   SC_COLOR_GREEN,   "cyan",    SC_COLOR_CYAN,
        "blue",    SC_COLOR_BLUE);
    row("purple",  SC_COLOR_PURPLE,  "magenta", SC_COLOR_MAGENTA,
        "white",   SC_COLOR_WHITE);

    section("Vivid  (_VIVID)");
    row("red_vivid",    SC_COLOR_RED_VIVID,    "orange_vivid", SC_COLOR_ORANGE_VIVID,
        "yellow_vivid", SC_COLOR_YELLOW_VIVID);
    row("green_vivid",  SC_COLOR_GREEN_VIVID,  "cyan_vivid",   SC_COLOR_CYAN_VIVID,
        "blue_vivid",   SC_COLOR_BLUE_VIVID);

    section("Accents  (_ACCENT*)");
    row("accent",        SC_COLOR_ACCENT,     "accent_dim",    SC_COLOR_ACCENT_DIM,
        "accent_dark",   SC_COLOR_ACCENT_DARK);

    section("Diagnostics");
    row("error",   SC_COLOR_ERROR,   "warning", SC_COLOR_WARNING,
        "success", SC_COLOR_SUCCESS);
    row("info",    SC_COLOR_INFO,    "hint",    SC_COLOR_HINT,
        "unused",  SC_COLOR_UNUSED);

    /* Palette colors compose with every widget. */
    section("In widgets");
    sc_panel_text(
        sc_markup_parse("Palette names work in [accent]markup[/] too: "
                        "[error]error[/], [warning]warning[/], "
                        "[success]success[/]."),
        (ScPanelOpts){
            .border  = { SC_BORDER_ROUNDED, SC_COLOR_ACCENT, SC_ANSI_COLOR_NONE },
            .bg      = SC_COLOR_BG_DARKEN_1,
            .padding = { 0, 1, 0, 1 },
            .title   = { .text = "sparcli palette",
                         .style = { .attr = SC_TEXT_ATTR_BOLD,
                                    .fg = SC_COLOR_ACCENT_IMPORTANT } },
        });

    section("By name  (sc_color_by_name)");
    ScColor c = { 0 };
    if (sc_color_by_name("orange", &c)) {
        printf("  sc_color_by_name(\"orange\") -> rgb(%d, %d, %d)\n",
               c.r, c.g, c.b);
    }
    sc_println("  The 8 plain hue names stay ANSI; use a palette name, "
               "#rrggbb or the SC_COLOR_* macro for the rest.",
               (ScTextStyle){ .attr = SC_TEXT_ATTR_DIM });
    printf("\n");
    return 0;
}
