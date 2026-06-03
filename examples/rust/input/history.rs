//! Input history with Up/Down recall and persistence.
//!
//!   cargo run -p sparcli --example input_history
//!
//! Dropping the `History` saves the configured file, so a second run recalls
//! lines from the first.

use sparcli::*;

const APP_NAME: &str = "sparcli-history-example";

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    // `.app(...)` persists to ~/.local/state/<app>/history; loaded on new().
    let history = History::new(HistoryOpts::new().app(APP_NAME).max_entries(100));

    if history.count() > 0 {
        println(
            &format!(
                "Loaded {} line(s) from the previous run; press Up to recall.",
                history.count()
            ),
            Style::default(),
        );
    }

    // Submitted lines are auto-added (empty + consecutive duplicates skipped).
    for _ in 0..3 {
        match text_input(">", TextInputOpts::new().history(&history).placeholder("type, or press Up")) {
            Ok(Some(_)) => {}
            _ => break,
        }
    }

    println("\nHistory contents:", Style::default());
    for i in 0..history.count() {
        println(
            &format!("  {i:2}: {}", history.get(i).unwrap_or_default()),
            Style::default(),
        );
    }

    // Dropping `history` saves it to the state file.
    Ok(())
}
