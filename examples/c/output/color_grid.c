/*
 * color_grid.c - every color model sparcli can render.
 *
 * sparcli's `ScColor` covers three cases (see the "Core Types" docs):
 *   - SC_ANSI_COLOR_NONE  (index 0)  - unset; no escape code emitted
 *   - SC_ANSI_COLOR_BLACK..WHITE     - the 8 named ANSI colors (index 1..8)
 *   - sc_color_from_rgb(r, g, b)     - 24-bit true color (index -1)
 *
 * This example shows the named palette (as text and as backgrounds), the
 * text attributes, and a few 24-bit gradients so you can see what your
 * terminal supports.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/color_grid
 */

#include <sparcli.h>

#include <stdint.h>
#include <stdio.h>

/* The full-block glyph U+2588 ("█"), used to paint a colored swatch with a
 * foreground color (so it works regardless of the terminal background). */
#define BLOCK "\xe2\x96\x88"

/** Prints `n` block glyphs in foreground color `fg`, no trailing newline. */
static void blocks(ScColor fg, int n) {
    if (n < 0) { n = 0; }
    if (n > 120) { n = 120; }
    char buf[120 * 3 + 1];
    int p = 0;
    for (int i = 0; i < n; i++) {
        buf[p++] = (char)0xe2;
        buf[p++] = (char)0x96;
        buf[p++] = (char)0x88;
    }
    buf[p] = '\0';
    sc_print(buf, (ScTextStyle){ .fg = fg });
}

/** Maps a hue `h` in [0, 360) to a fully saturated 24-bit RGB color. */
static ScColor hue(int h) {
    int seg = (h / 60) % 6;
    uint8_t up = (uint8_t)((h % 60) * 255 / 59);
    uint8_t down = (uint8_t)(255 - up);
    switch (seg) {
        case 0:  return sc_color_from_rgb(255, up, 0);
        case 1:  return sc_color_from_rgb(down, 255, 0);
        case 2:  return sc_color_from_rgb(0, 255, up);
        case 3:  return sc_color_from_rgb(0, down, 255);
        case 4:  return sc_color_from_rgb(up, 0, 255);
        default: return sc_color_from_rgb(255, 0, down);
    }
}

/** Prints a dim, fixed-width row label so every gradient starts in the same
 *  column. */
static void ramp_label(const char *name) {
    char label[24];
    snprintf(label, sizeof label, "  %-11s", name);
    sc_print(label, (ScTextStyle){ .attr = SC_TEXT_ATTR_DIM });
}

/** A left-aligned section header rule. */
static void section(const char *title) {
    printf("\n");
    sc_rule_str(title, (ScRuleOpts){
        .title = { .halign = SC_ALIGN_LEFT, .pad = 1,
                   .style = { .attr = SC_TEXT_ATTR_BOLD } },
        .color = SC_ANSI_COLOR_NONE,
    });
}

int main(void) {
    /* ── Named ANSI colors (index 1..8) ─────────────────────────────────── */
    static const struct { const char *name; ScColor color; bool dark; } NAMED[] = {
        { "black",   SC_ANSI_COLOR_BLACK,   true  },
        { "red",     SC_ANSI_COLOR_RED,     true  },
        { "green",   SC_ANSI_COLOR_GREEN,   false },
        { "yellow",  SC_ANSI_COLOR_YELLOW,  false },
        { "blue",    SC_ANSI_COLOR_BLUE,    true  },
        { "magenta", SC_ANSI_COLOR_MAGENTA, true  },
        { "cyan",    SC_ANSI_COLOR_CYAN,    false },
        { "white",   SC_ANSI_COLOR_WHITE,   false },
    };

    section("Named colors  (SC_ANSI_COLOR_*)");
    for (size_t i = 0; i < sizeof NAMED / sizeof NAMED[0]; i++) {
        char label[16];
        snprintf(label, sizeof label, " %-8s", NAMED[i].name);

        sc_print("  ", (ScTextStyle){ 0 });
        blocks(NAMED[i].color, 6);                       /* fg swatch */
        sc_print(label, (ScTextStyle){ 0 });             /* legible name */

        /* The same color as a background, with a readable foreground. */
        ScColor text = NAMED[i].dark ? SC_ANSI_COLOR_WHITE : SC_ANSI_COLOR_BLACK;
        sc_print("   ", (ScTextStyle){ 0 });
        sc_print("  on background  ",
                 (ScTextStyle){ .fg = text, .bg = NAMED[i].color });
        printf("\n");
    }

    /* ── Text attributes ────────────────────────────────────────────────── */
    section("Attributes  (combine with |)");
    sc_print("  ", (ScTextStyle){ 0 });
    sc_print(" normal ", (ScTextStyle){ .fg = SC_ANSI_COLOR_CYAN });
    sc_print(" bold ", (ScTextStyle){
        .attr = SC_TEXT_ATTR_BOLD, .fg = SC_ANSI_COLOR_CYAN });
    sc_print(" dim ", (ScTextStyle){
        .attr = SC_TEXT_ATTR_DIM, .fg = SC_ANSI_COLOR_CYAN });
    sc_print(" italic ", (ScTextStyle){
        .attr = SC_TEXT_ATTR_ITALIC, .fg = SC_ANSI_COLOR_CYAN });
    sc_print(" underline ", (ScTextStyle){
        .attr = SC_TEXT_ATTR_UNDER, .fg = SC_ANSI_COLOR_CYAN });
    printf("\n");

    /* ── 24-bit true color (sc_color_from_rgb) ──────────────────────────── */
    section("24-bit true color  (sc_color_from_rgb)");

    ramp_label("hue");
    for (int h = 0; h < 360; h += 6) { blocks(hue(h), 1); }
    printf("\n");

    static const struct { const char *name; int axis; } RAMP[] = {
        { "red", 0 }, { "green", 1 }, { "blue", 2 },
    };
    for (size_t r = 0; r < sizeof RAMP / sizeof RAMP[0]; r++) {
        ramp_label(RAMP[r].name);
        for (int i = 0; i < 60; i++) {
            uint8_t v = (uint8_t)(i * 255 / 59);
            ScColor c = RAMP[r].axis == 0 ? sc_color_from_rgb(v, 0, 0)
                      : RAMP[r].axis == 1 ? sc_color_from_rgb(0, v, 0)
                                          : sc_color_from_rgb(0, 0, v);
            blocks(c, 1);
        }
        printf("\n");
    }

    ramp_label("grayscale");
    for (int i = 0; i < 60; i++) {
        uint8_t v = (uint8_t)(i * 255 / 59);
        blocks(sc_color_from_rgb(v, v, v), 1);
    }
    printf("\n");

    /* ── Note ───────────────────────────────────────────────────────────── */
    section("Unset");
    sc_println("  SC_ANSI_COLOR_NONE emits no escape code - the terminal's "
               "own default is used.", (ScTextStyle){ .attr = SC_TEXT_ATTR_DIM });
    printf("\n");
    return 0;
}
