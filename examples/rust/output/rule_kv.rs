//! Horizontal rules and key-value lists.
//!
//!   cargo run -p sparcli --example output_rule_kv

use sparcli::*;

fn main() {
    // Plain divider, then a titled one.
    rule(RuleOpts::new().kind(BorderType::Single));
    rule(
        RuleOpts::new()
            .kind(BorderType::Double)
            .color(Color::CYAN)
            .title("Results")
            .align(Align::Center),
    );

    // Fixed width, left placement.
    rule(
        RuleOpts::new()
            .kind(BorderType::Thick)
            .title("left")
            .width(40)
            .align(Align::Left),
    );
    println("", Style::default());

    // Key-value list with a styled key column.
    let mut info = Kv::new(KvOpts::new().key_style(Style::bold().fg(Color::CYAN)));
    info.add("Version", "1.2.0")
        .add("License", "MIT")
        .add("Platform", "macOS / Linux");
    info.print();
    println("", Style::default());

    // Fixed key width and wrapped values.
    let mut details = Kv::new(KvOpts::new().key_width(12).wrap_val(true));
    details.add(
        "Description",
        "A C11 library for styled terminal output: panels, tables, rules, \
         lists, trees and interactive input widgets.",
    );
    details.add("Homepage", "https://github.com/example/sparcli");
    details.print();
}
