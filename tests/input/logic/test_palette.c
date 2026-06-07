#include "test_input.h"
#include "sparcli.h"

#include <stdbool.h>
#include <string.h>


/** True when two colors are byte-identical (index + RGB). */
static bool color_eq(ScColor a, ScColor b) {
    return a.index == b.index && a.r == b.r && a.g == b.g && a.b == b.b;
}

/**
 * Runtime palette overrides: sc_palette_set/get/reset and that the override is
 * honored by the shared name resolver (sc_color_by_name) and thus by markup.
 */
void test_palette(void) {
    sc_palette_reset();

    ScColor c = { 0 };

    // Default "accent" resolves to the palette macro.
    CHECK(sc_color_by_name("accent", &c) && color_eq(c, SC_COLOR_ACCENT),
          "accent defaults to SC_COLOR_ACCENT");

    // Override flows through sc_color_by_name and sc_palette_get.
    CHECK(sc_palette_set("accent", SC_ANSI_COLOR_RED),
          "sc_palette_set on a known name returns true");
    CHECK(sc_color_by_name("accent", &c) && color_eq(c, SC_ANSI_COLOR_RED),
          "sc_color_by_name reflects the override");
    CHECK(sc_palette_get("accent", &c) && color_eq(c, SC_ANSI_COLOR_RED),
          "sc_palette_get reflects the override");

    // Markup [accent] resolves through the same path → red span foreground.
    ScText *t = sc_markup_parse("[accent]x[/]");
    bool span_red = t && sc_text_span_count(t) > 0
                 && color_eq(sc_text_span(t, 0).style.fg, SC_ANSI_COLOR_RED);
    CHECK(span_red, "markup [accent] uses the override");
    sc_text_free(t);

    // Other names are unaffected by the accent override.
    CHECK(sc_color_by_name("error", &c) && color_eq(c, SC_COLOR_ERROR),
          "unrelated palette names keep their default");

    // Unknown names are rejected.
    CHECK(!sc_palette_set("not_a_color", SC_ANSI_COLOR_RED),
          "sc_palette_set on an unknown name returns false");

    // Reset restores the built-in default.
    sc_palette_reset();
    CHECK(sc_color_by_name("accent", &c) && color_eq(c, SC_COLOR_ACCENT),
          "sc_palette_reset restores the default");
}
