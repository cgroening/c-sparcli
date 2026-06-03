//! Confirmation and single/multi selection.
//!
//!   cargo run -p sparcli --example input_confirm_select
//!
//! Each prompt returns `Result<Option<T>>`: `Ok(None)` = cancelled,
//! `Err(Unavailable)` = no interactive terminal.

use sparcli::*;

const LANGUAGES: [&str; 6] = ["C", "C++", "Rust", "Python", "Zig", "Go"];

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    run_confirm()?;
    run_single_select()?;
    run_multi_select()?;
    Ok(())
}

/// Yes/no with a default answer.
fn run_confirm() -> sparcli::Result<()> {
    if let Some(deploy) =
        confirm("Deploy to production?", ConfirmOpts::new().default_yes(false))?
    {
        markup::println(if deploy {
            "  [green]-> deploying[/]"
        } else {
            "  [yellow]-> skipped[/]"
        });
    }
    Ok(())
}

/// Single selection: `run_one` returns the chosen index.
fn run_single_select() -> sparcli::Result<()> {
    let mut select = Select::new(SelectOpts::new().prompt("Primary language"));
    for language in LANGUAGES {
        select.add(language);
    }
    if let Some(index) = select.run_one()? {
        println(&format!("  -> {}", LANGUAGES[index]), Style::default());
    }
    Ok(())
}

/// Multi selection: `run` returns the indices in display order.
fn run_multi_select() -> sparcli::Result<()> {
    let mut select = Select::new(
        SelectOpts::new()
            .prompt("Languages you use (Space toggles)")
            .multi(true)
            .max_visible(5),
    );
    for language in LANGUAGES {
        select.add(language);
    }
    select.set_checked(0, true);
    select.set_cursor(1);

    if let Some(indices) = select.run()? {
        let picked: Vec<&str> = indices.iter().map(|&i| LANGUAGES[i]).collect();
        println(&format!("  -> {picked:?}"), Style::default());
    }
    Ok(())
}
