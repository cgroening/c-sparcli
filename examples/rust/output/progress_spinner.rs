//! Animated progress bars and spinners.
//!
//!   cargo run -p sparcli --example output_progress_spinner

use std::thread::sleep;
use std::time::Duration;

use sparcli::*;

fn main() {
    run_progress_bar();
    println("", Style::default());
    run_spinner();
    println("", Style::default());
    run_transient_line();
}

/// clear_line() overwrites the current line in place, for a transient status
/// that should leave no trace afterwards.
fn run_transient_line() {
    use std::io::Write;
    print("Preparing...", Style::default());
    std::io::stdout().flush().ok();
    sleep(Duration::from_millis(500));
    clear_line();
    println("Ready.", Style::default());
}

/// A styled block bar with brackets and a percentage.
fn run_progress_bar() {
    let mut bar = ProgressBar::new(
        ProgressBarOpts::new()
            .kind(ProgressType::Block)
            .brackets()
            .show_percent(true)
            .fill_color(Color::CYAN)
            .width(60),
    );
    bar.set_label("download");
    for value in 0..=100 {
        bar.draw(value as f64, 100.0);
        sleep(Duration::from_millis(8));
    }
    bar.finish(100.0, 100.0);
}

/// A spinner whose label changes mid-animation.
fn run_spinner() {
    let mut spinner = Spinner::new(
        "Connecting...",
        SpinnerOpts::new().kind(SpinnerType::Braille).color(Color::CYAN),
    );
    for _ in 0..25 {
        spinner.tick();
        sleep(Duration::from_millis(40));
    }
    spinner.set_label("Fetching data...");
    for _ in 0..25 {
        spinner.tick();
        sleep(Duration::from_millis(40));
    }
    spinner.finish(true, "Done");
}
