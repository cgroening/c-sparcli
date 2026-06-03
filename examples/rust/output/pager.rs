//! Route long output through $PAGER / `less -R`.
//!
//!   cargo run -p sparcli --example output_pager
//!
//! In a terminal the table opens in the pager (press `q`). Off a TTY the
//! pager is skipped and the output prints directly.

use sparcli::*;

const ROW_COUNT: i32 = 100;

fn main() {
    // Everything until end()/drop is paged.
    let pager = Pager::begin(PagerOpts::new());

    rule(RuleOpts::new().kind(BorderType::Double).title("100 rows"));

    let mut table = Table::new();
    table
        .column("#", ColOpts::new().align(Align::Right))
        .column("Square", ColOpts::new().align(Align::Right));
    for i in 1..=ROW_COUNT {
        table.row([i.to_string(), (i * i).to_string()]);
    }
    table.print(TableOpts::new().border(BorderType::Single).header_row(true));

    let status = pager.end();
    println(&format!("pager exit status: {status}"), Style::default());
}
