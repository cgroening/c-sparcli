//! Rich-style inline markup.
//!
//!   cargo run -p sparcli --example output_markup

use sparcli::*;

fn main() {
    markup::println("[bold]Bold[/], [italic]italic[/], [u]underline[/] \
                     and [dim]dim[/].");
    markup::println("[red]Named colors[/], [on blue] backgrounds [/] and \
                     [rgb(255,165,0)]24-bit RGB[/].");
    markup::println("[bold green on white] Combined in one tag [/]");
    markup::println("[bold]Bold [red]and red[/] - still bold[/].");

    // Literal brackets, inline code, OSC-8 hyperlink.
    markup::println("Escape with [[ to print a literal bracket.");
    markup::println("Inline code: run `make qa` before committing.");
    markup::println("Read the [link=https://github.com]docs[/link].");
    println("", Style::default());

    // Parse into a Text and print it (any widget taking rich text accepts it).
    let content = Text::markup(
        "[bold]sparcli[/] renders [green]markup[/] inside panels,\n\
         tables, lists and every other widget.",
    );
    content.print();
    println("", Style::default());

    // Custom code-span style via MarkupOpts.
    let styled = markup::parse_opts(
        "[blink]Unknown tags[/blink] stay verbatim; `code` is cyan here.",
        MarkupOpts {
            code_style: Style::bold().fg(Color::CYAN),
            ..Default::default()
        },
    );
    styled.print();
    println("", Style::default());
}
