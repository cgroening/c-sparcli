//! Calendar month grid for picking a date.
//!
//!   cargo run -p sparcli --example input_datepicker
//!
//! Keys: arrows move by day/week, PageUp/Down change the month, Enter picks.

use sparcli::*;

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    // `None` seeds the picker with today's date.
    if let Some(date) = datepicker(
        None,
        DatePickerOpts::new().prompt("Delivery date").week_start(WeekStart::Monday),
    )? {
        println(
            &format!("  -> {:04}-{:02}-{:02}", date.year, date.month, date.day),
            Style::default(),
        );
    }

    // Pre-seed a specific start date.
    let seed = Date { year: 2027, month: 1, day: 1 };
    if let Some(date) = datepicker(
        Some(seed),
        DatePickerOpts::new()
            .prompt("Starts at 2027-01-01 (Sunday weeks)")
            .week_start(WeekStart::Sunday),
    )? {
        println(
            &format!("  -> {:04}-{:02}-{:02}", date.year, date.month, date.day),
            Style::default(),
        );
    }

    Ok(())
}
