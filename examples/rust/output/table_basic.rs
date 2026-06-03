//! Tables: columns, header row, alignment, borders.
//!
//!   cargo run -p sparcli --example output_table_basic

use sparcli::*;

fn main() {
    // The data is built once; rendering options are passed at print time.
    let mut table = Table::new();
    table
        .column("City", ColOpts::new())
        .column("Country", ColOpts::new())
        .column("Pop. (M)", ColOpts::new().align(Align::Right));

    table.row(["Tokyo", "Japan", "37.4"]);
    table.row(["Delhi", "India", "32.9"]);
    table.row(["Oslo", "Norway", "1.1"]);
    table.footer_row(["Total", "3 countries", "71.4"]);

    // 1. Default look: single border, header row.
    table.print(TableOpts::new().border(BorderType::Single).header_row(true));
    println("", Style::default());

    // 2. Same data, rounded border, title, padding.
    table.print(
        TableOpts::new()
            .border(BorderType::Rounded)
            .header_row(true)
            .title("World cities")
            .cell_pad(Edges { left: 1, right: 1, ..Default::default() }),
    );
    println("", Style::default());

    // 3. Per-column style: bold cyan first column.
    let mut styled = Table::new();
    styled
        .column("Key", ColOpts::new().style(Style::bold().fg(Color::CYAN)))
        .column("Value", ColOpts::new());
    styled.row(["version", "1.2.0"]);
    styled.row(["license", "MIT"]);
    styled.print(TableOpts::new().border(BorderType::Single).header_row(true));
}
