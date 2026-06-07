// markup.cpp - Rich-style inline markup (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/markup

#include <sparcli.hpp>

using namespace sparcli;


int main() {
    markup::println("[bold]Bold[/], [italic]italic[/], [u]underline[/], "
                    "[strike]strike[/] (or [s]s[/]) and [dim]dim[/].");
    markup::println("[red]Named colors[/], [on blue] backgrounds [/] and "
                    "[rgb(255,165,0)]24-bit RGB[/].");
    markup::println("[bold green on white] Combined in one tag [/]");
    markup::println("[bold]Bold [red]and red[/] - still bold[/].");

    // Literal brackets, inline code, and an OSC-8 hyperlink.
    markup::println("Escape with [[ to print a literal bracket.");
    markup::println("Inline code: run `make qa` before committing.");
    markup::println("Read the [link=https://github.com]docs[/link].");
    println("");

    // Parse into a Text and feed it to any widget that takes rich text.
    Text content = markup::parse(
        "[bold]sparcli[/] renders [green]markup[/] inside panels,\n"
        "tables, lists and every other widget.");
    panel(content, { .border = { .type = SC_BORDER_ROUNDED },
                     .title  = { .text = "markup", .pad = 1 } });

    // append_markup mixes markup into an existing Text.
    Text mixed;
    mixed.append("plain prefix - ").append_markup("[bold cyan]markup suffix[/]");
    mixed.print();
    println("");
    println("");

    // Parser options: strip unknown tags, restyle code spans.
    markup::println(
        "[blink]Unknown tags[/blink] are stripped; `code` is yellow here.",
        { .strip_unknown = 1,
          .code_style    = style(SC_TEXT_ATTR_BOLD, yellow()) });

    return 0;
}
