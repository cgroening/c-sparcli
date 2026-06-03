// text_styles.cpp - styled text, rich Text, links and badges (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/text_styles

#include <sparcli.hpp>

#include <print>
#include <string>

using namespace sparcli;


int main() {
    // Plain styled lines; style(attr, fg, bg) builds an ScTextStyle.
    println("Bold red on the default background",
            style(SC_TEXT_ATTR_BOLD, red()));
    println("Italic + underline in 24-bit orange",
            style(static_cast<TextAttribute>(
                      SC_TEXT_ATTR_ITALIC | SC_TEXT_ATTR_UNDER),
                  rgb(255, 165, 0)));
    println("Dim text on a blue background",
            style(SC_TEXT_ATTR_DIM, none(), blue()));
    println("");

    // Rich Text: chained append() calls, each with its own style. The
    // wrapper copies every appended string, so temporaries are safe.
    Text line;
    line.append("status: ")
        .append("passed", style(SC_TEXT_ATTR_BOLD, green()))
        .append("  (3 warnings)", style(SC_TEXT_ATTR_DIM));
    line.print();
    std::println("\nvisible width: {} columns\n", line.visible_width());

    // OSC-8 hyperlink and an inline badge.
    Text link_line;
    link_line.append("Docs: ")
        .append_link("sparcli on GitHub",
                     "https://github.com/example/sparcli");
    link_line.print();
    println("");

    badge("PASS", { .text_style = style(SC_TEXT_ATTR_BOLD, green()) });
    print(" ");
    badge("v1.2.0", { .left_cap = "(", .right_cap = ")",
                      .text_style = style(SC_TEXT_ATTR_NONE, magenta()),
                      .pad = 1 });
    println("");
    println("");

    // Utilities return owning std::strings.
    std::string plain = strip_ansi("\033[1;32mgreen bold\033[0m");
    std::string cut   = truncate("A sentence that is far too long", 16, "…");
    std::println("stripped:  \"{}\"", plain);
    std::println("truncated: \"{}\"", cut);

    return 0;
}
