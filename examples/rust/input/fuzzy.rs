//! Fuzzy finder over a list, plus the pure match scorer.
//!
//!   cargo run -p sparcli --example input_fuzzy

use sparcli::*;

fn main() -> sparcli::Result<()> {
    // The scorer is pure and works without a terminal.
    let (hit, score) = fuzzy_match("ffind", "feature/fuzzy-finder");
    println(
        &format!("match \"ffind\" in \"feature/fuzzy-finder\": {hit} (score {score})"),
        Style::dim(),
    );

    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    let branches = [
        "main",
        "develop",
        "feature/fuzzy-finder",
        "feature/live-display",
        "fix/table-wrap",
        "release/1.2",
    ];

    // Rounded frame + dark content background: result rows inherit the
    // background and the cursor row is a full-width highlight bar.
    let mut finder = Fuzzy::new(
        FuzzyOpts::new()
            .prompt("Switch branch")
            .selected_style(Style::bold().fg(Color::WHITE).bg(Color::rgb(255, 121, 198)))
            .box_style(
                BoxStyle::new(BorderStyle::new(BorderType::Rounded)
                    .color(Color::rgb(255, 121, 198)))
                    .bg(Color::rgb(30, 30, 46))
                    .padding(Edges::symmetric(0, 1)),
            ),
    );
    for branch in branches {
        finder.add(branch);
    }

    // The returned index refers to the original add order.
    if let Some(index) = finder.run()? {
        println(&format!("  -> {}", branches[index]), Style::default());
    }
    Ok(())
}
