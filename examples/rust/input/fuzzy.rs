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

    let mut finder = Fuzzy::new(FuzzyOpts::new().prompt("Switch branch"));
    for branch in branches {
        finder.add(branch);
    }

    // The returned index refers to the original add order.
    if let Some(index) = finder.run()? {
        println(&format!("  -> {}", branches[index]), Style::default());
    }
    Ok(())
}
