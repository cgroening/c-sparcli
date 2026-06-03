/*
 * markup.c - Rich-style inline markup: tags, colors, links, inline code.
 *
 * Run (after `make`):
 *   make run-example EX=c/output/markup
 */

#include <sparcli.h>

#include <stdio.h>


int main(void) {
    // Direct printing: tags style the enclosed text, [/] closes the last tag.
    sc_markup_println("[bold]Bold[/], [italic]italic[/], [u]underline[/] "
                      "and [dim]dim[/].");
    sc_markup_println("[red]Named colors[/], [on blue] backgrounds [/] and "
                      "[rgb(255,165,0)]24-bit RGB[/].");
    sc_markup_println("[bold green on white] Combined in one tag [/]");

    // Tags nest as a stack: closing pops the innermost style frame.
    sc_markup_println("[bold]Bold [red]and red[/] - still bold[/].");

    // Literal brackets and inline code spans.
    sc_markup_println("Escape with [[ to print a literal bracket.");
    sc_markup_println("Inline code: run `make qa` before committing.");

    // OSC-8 hyperlink (clickable in supporting terminals).
    sc_markup_println("Read the [link=https://github.com]docs[/link].");
    printf("\n");

    // Parse into an ScText and feed it to any widget that accepts rich text.
    ScText *content = sc_markup_parse(
        "[bold]sparcli[/] renders [green]markup[/] inside panels,\n"
        "tables, lists and every other widget."
    );
    sc_panel_text(content, (ScPanelOpts){
        .border = { .type = SC_BORDER_ROUNDED },
        .title  = { .text = "markup", .pad = 1 },
    });
    sc_text_free(content);

    // Append markup to an existing rich text.
    ScText *mixed = sc_text_new();
    sc_text_append(mixed, "plain prefix - ", (ScTextStyle){ 0 });
    sc_markup_append(mixed, "[bold cyan]markup suffix[/]");
    sc_print_text(mixed);
    printf("\n\n");
    sc_text_free(mixed);

    // Parser options: strip unknown tags instead of printing them verbatim,
    // and restyle `inline code` spans.
    sc_markup_println_opts(
        "[blink]Unknown tags[/blink] are stripped; `code` is yellow here.",
        (ScMarkupOpts){
            .strip_unknown = 1,
            .code_style    = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_YELLOW,
                               SC_ANSI_COLOR_NONE },
        }
    );

    return 0;
}
