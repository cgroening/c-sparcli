/*
 * text_styles.c - styled text: colors, attributes, rich text, links, badges.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/text_styles
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


int main(void) {
    // Plain styled lines: sc_println(text, style) appends a newline and a
    // style reset, so every line starts clean.
    sc_println("Bold red on the default background", (ScTextStyle){
        SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_RED, SC_ANSI_COLOR_NONE,
    });
    sc_println("Italic + underline in 24-bit orange", (ScTextStyle){
        .attr = SC_TEXT_ATTR_ITALIC | SC_TEXT_ATTR_UNDER,
        .fg   = sc_color_from_rgb(255, 165, 0),
    });
    sc_println("Dim text on a blue background", (ScTextStyle){
        .attr = SC_TEXT_ATTR_DIM,
        .bg   = SC_ANSI_COLOR_BLUE,
    });
    printf("\n");

    // Rich text: one ScText holds multiple spans, each with its own style.
    ScText *line = sc_text_new();
    sc_text_append(line, "status: ", (ScTextStyle){ 0 });
    sc_text_append(line, "passed", (ScTextStyle){
        SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE,
    });
    sc_text_append(line, "  (3 warnings)", (ScTextStyle){
        .attr = SC_TEXT_ATTR_DIM,
    });
    sc_print_text(line);
    printf("\n");

    // Width helpers are ANSI- and UTF-8-aware (codepoints, not bytes).
    printf("visible width: %zu columns\n\n", sc_text_visible_width(line));
    sc_text_free(line);

    // OSC-8 hyperlink: clickable in supporting terminals, plain elsewhere.
    ScText *link_line = sc_text_new();
    sc_text_append(link_line, "Docs: ", (ScTextStyle){ 0 });
    sc_text_append_link(link_line, "sparcli on GitHub",
                        "https://github.com/example/sparcli",
                        (ScTextStyle){ .fg = SC_ANSI_COLOR_CYAN });
    sc_print_text(link_line);
    printf("\n\n");
    sc_text_free(link_line);

    // Badges: inline [ TAG ]-style tokens (no trailing newline).
    sc_print_badge("PASS", (ScBadgeOpts){
        .text_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN,
                        SC_ANSI_COLOR_NONE },
    });
    printf(" ");
    sc_print_badge("v1.2.0", (ScBadgeOpts){
        .left_cap   = "(",
        .right_cap  = ")",
        .pad        = 1,
        .text_style = { .fg = SC_ANSI_COLOR_MAGENTA },
    });
    printf("\n\n");

    // Utilities: strip ANSI escape codes / truncate to a column width.
    char *plain = sc_strip_ansi("\033[1;32mgreen bold\033[0m");
    char *cut   = sc_truncate("A sentence that is far too long", 16, "…");
    if (plain && cut) {
        printf("stripped:  \"%s\"\n", plain);
        printf("truncated: \"%s\"\n", cut);
    }
    free(plain);
    free(cut);

    return 0;
}
