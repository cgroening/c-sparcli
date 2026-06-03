//! Live display: re-render a composed frame in place (dashboard).
//!
//!   cargo run -p sparcli --example output_live
//!
//! Off a TTY (piped) only the final frame is printed.

use std::thread::sleep;
use std::time::Duration;

use sparcli::*;

const TOTAL_STEPS: i32 = 30;

fn main() {
    markup::println("[bold]Live dashboard[/] [dim](redraws in place)[/]\n");

    let live = Live::begin(LiveOpts::new());
    for step in 0..=TOTAL_STEPS {
        let frame = build_frame(step);
        live.update(&frame);
        sleep(Duration::from_millis(60));
    }
    live.end();

    markup::println("\n[green]✔[/] Frame above was redrawn in place \
                     on every update.");

    // Fullscreen variant: alt_screen takes over the whole terminal and
    // restores the previous screen on end (a no-op off a TTY).
    markup::println("[dim]Next: the same dashboard on the alternate \
                     screen...[/]");
    sleep(Duration::from_secs(1));
    let full = Live::begin(LiveOpts::new().alt_screen());
    for step in 0..=TOTAL_STEPS {
        full.update(&build_frame(step));
        sleep(Duration::from_millis(40));
    }
    full.end();
    markup::println("[green]✔[/] The previous screen was restored.");
}

/// Composes one dashboard frame: a status table plus a progress line.
fn build_frame(step: i32) -> Rendered {
    let percent = step * 100 / TOTAL_STEPS;

    let mut table = Table::new();
    table
        .column("Job", ColOpts::new().min_width(10))
        .column("Status", ColOpts::new().min_width(14));
    table.row(["build", if percent >= 50 { "done" } else { "running" }]);
    table.row([
        "package",
        if percent >= 100 { "done" } else if percent >= 50 { "running" } else { "waiting" },
    ]);

    let table_part = capture::table(
        &table,
        TableOpts::new().border(BorderType::Rounded).header_row(true),
    );
    let line_part = capture::str(&format!("progress: {percent} %"));

    vstack(&[&table_part, &line_part], 1).expect("two parts")
}
