//! Numeric input, calculator mode and the pure evaluator.
//!
//!   cargo run -p sparcli --example input_number_calc

use sparcli::*;

fn main() -> sparcli::Result<()> {
    // The evaluator is pure and works without a terminal.
    if let Some(result) = calc_eval("(2,5 + 1,5) * 4 - 1") {
        println(&format!("(2,5 + 1,5) * 4 - 1 = {result}"), Style::dim());
    }

    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    run_stepper()?;
    run_calculator()?;
    Ok(())
}

/// Up/Down steps the value; the result is clamped to the range.
fn run_stepper() -> sparcli::Result<()> {
    if let Some(quantity) = number_input(
        "Quantity",
        NumberOpts::new().range(1.0, 99.0).step(1.0).initial(1.0).decimals(0),
    )? {
        println(&format!("  -> {quantity}"), Style::default());
    }
    Ok(())
}

/// Calculator mode: `number_input_text` returns the exact value as a string
/// ('.'-normalized), ready to parse into an arbitrary-precision decimal.
fn run_calculator() -> sparcli::Result<()> {
    if let Some(amount) = number_input_text(
        "Amount (try =1/3)",
        NumberOpts::new().decimals(2).decimal_sep(',').start_empty(true).calculator(true),
    )? {
        println(&format!("  -> exact: {amount}"), Style::default());
    }
    Ok(())
}
