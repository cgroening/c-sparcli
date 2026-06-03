//! Tables: colspan, stripes, per-row background and markup cells.
//!
//!   cargo run -p sparcli --example output_table_advanced

use sparcli::*;

fn main() {
    print_spans_table();
    println("", Style::default());
    print_styled_table();
}

/// Colspan via `Cell::colspan`; the covered slot uses `Cell::skip`.
fn print_spans_table() {
    let mut table = Table::new();
    table
        .column("Quarter", ColOpts::new())
        .column("Region", ColOpts::new())
        .column("Revenue", ColOpts::new().align(Align::Right));

    table.row(["Q1", "Europe", "1.2 M"]);
    table.row(["Q1", "Americas", "2.4 M"]);
    table.row([
        Cell::new("Q2"),
        Cell::new("no data yet").colspan(2).align(Align::Center),
        Cell::skip(),
    ]);

    table.print(
        TableOpts::new()
            .border(BorderType::Single)
            .header_row(true)
            .title("Spans"),
    );
}

/// Stripes, a header column and markup cells; per-row background highlight.
fn print_styled_table() {
    let mut table = Table::new();
    table
        .column("Check", ColOpts::new())
        .column("Result", ColOpts::new())
        .column("Duration", ColOpts::new().align(Align::Right));

    table.row([Cell::new("build"), Cell::markup("[green]✔ passed[/]"),
               Cell::new("41 s")]);
    table.row([Cell::new("lint"), Cell::markup("[green]✔ passed[/]"),
               Cell::new("3 s")]);
    // Per-row background highlights the failing check.
    table.row_bg(
        [Cell::new("fuzz"), Cell::markup("[red]✖ failed[/]"),
         Cell::new("122 s")],
        Color::rgb(60, 30, 30),
    );

    table.print(
        TableOpts::new()
            .border(BorderType::Rounded)
            .header_col(true)
            .striped(true),
    );
}
