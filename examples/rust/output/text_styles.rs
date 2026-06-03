//! Styled text, rich `Text`, links and badges.
//!
//!   cargo run -p sparcli --example output_text_styles

use sparcli::*;

fn main() {
    // Plain styled lines; Style is a chainable builder.
    println("Bold red on the default background", Style::bold().fg(Color::RED));
    println(
        "Italic + underline in 24-bit orange",
        Style::new()
            .attr(Attr::ITALIC | Attr::UNDERLINE)
            .fg(Color::rgb(255, 165, 0)),
    );
    println("Dim text on a blue background", Style::dim().bg(Color::BLUE));
    println("", Style::default());

    // Rich Text: chained spans, each with its own style.
    let mut line = Text::new();
    line.append("status: ", Style::default())
        .append("passed", Style::bold().fg(Color::GREEN))
        .append("  (3 warnings)", Style::dim());
    line.print();
    println(
        &format!("\nvisible width: {} columns", line.visible_width()),
        Style::default(),
    );
    println("", Style::default());

    // OSC-8 hyperlink (clickable in supporting terminals).
    let mut link_line = Text::new();
    link_line
        .append("Docs: ", Style::default())
        .append_link(
            "sparcli on GitHub",
            "https://github.com/example/sparcli",
            Style::new().fg(Color::CYAN),
        );
    link_line.print();
    println("", Style::default());

    // Inline badges.
    badge("PASS", BadgeOpts::new().style(Style::bold().fg(Color::GREEN)));
    print(" ", Style::default());
    badge(
        "v1.2.0",
        BadgeOpts::new().caps("(", ")").pad(1).style(Style::new().fg(Color::MAGENTA)),
    );
    println("", Style::default());
    println("", Style::default());

    // Utilities return owning Strings.
    let plain = strip_ansi("\x1b[1;32mgreen bold\x1b[0m");
    let cut = truncate("A sentence that is far too long", 16, "…");
    println(&format!("stripped:  \"{plain}\""), Style::default());
    println(&format!("truncated: \"{cut}\""), Style::default());

    // ANSI trust boundary: user strings are sanitized by default, so raw
    // escape codes in untrusted input cannot inject styling. Opt in per widget
    // with .ansi(...) (or process-wide via set_allow_ansi) when input is trusted.
    println("", Style::default());
    let pre_colored = "\x1b[31mpre-colored\x1b[0m input";
    panel(pre_colored, PanelOpts::new().single().title("sanitized (default)"));
    panel(
        pre_colored,
        PanelOpts::new().single().title("ansi allowed").ansi(AnsiMode::Allow),
    );
}
