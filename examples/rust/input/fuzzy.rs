//! Fuzzy finder over a list and over a table, plus the pure match scorer.
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

    // Table view: multi-column rows; the query searches every column and
    // highlights the matched cells. `.table(headers)` switches the view.
    let hosts = [
        ["web-01", "eu-central", "healthy"],
        ["web-02", "eu-central", "degraded"],
        ["db-01", "us-east", "healthy"],
        ["cache-01", "ap-south", "healthy"],
    ];
    let mut grid = Fuzzy::new(
        FuzzyOpts::new()
            .prompt("Pick a host")
            .accent(Color::BLUE)
            .table(["Host", "Region", "Status"])
            .table_opts(
                TableOpts::new()
                    .border(BorderType::Rounded)
                    .header_row(true),
            ),
    );
    for host in hosts {
        grid.add_row(host);
    }
    if let Some(index) = grid.run()? {
        println(&format!("  -> row {} ({})", index, hosts[index][0]), Style::default());
    }
    Ok(())
}
