//! Text input (placeholder, char filter, autocomplete, boxed) and masked
//! password input.
//!
//!   cargo run -p sparcli --example input_text_password

use sparcli::*;

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    run_plain_input()?;
    run_autocomplete_input()?;
    run_boxed_input()?;
    run_password()?;
    Ok(())
}

/// Plain entry with a placeholder.
fn run_plain_input() -> sparcli::Result<()> {
    if let Some(name) =
        text_input("Project name", TextInputOpts::new().placeholder("my-project"))?
    {
        println(&format!("  -> {name:?}"), Style::default());
    }
    Ok(())
}

/// Suggestions in a fuzzy dropdown plus a no-space filter.
fn run_autocomplete_input() -> sparcli::Result<()> {
    let commands = vec![
        "commit".into(),
        "checkout".into(),
        "cherry-pick".into(),
        "clone".into(),
        "config".into(),
    ];
    if let Some(command) = text_input(
        "Git command",
        TextInputOpts::new()
            .char_filter(CharFilter::NoSpace)
            .suggestions(commands)
            .suggest(SuggestOpts::dropdown().fuzzy()),
    )? {
        println(&format!("  -> {command:?}"), Style::default());
    }
    Ok(())
}

/// Boxed mode: panel frame, character counter, length limit.
fn run_boxed_input() -> sparcli::Result<()> {
    if let Some(title) = text_input(
        "Title",
        TextInputOpts::new().placeholder("Short and descriptive").max_chars(32).boxed(44),
    )? {
        println(&format!("  -> {title:?}"), Style::default());
    }
    Ok(())
}

/// Masked input; the value is never echoed.
fn run_password() -> sparcli::Result<()> {
    if let Some(secret) = password_input("Passphrase", PasswordOpts::new().mask("•"))? {
        println(
            &format!("  -> captured {} chars (not echoed)", secret.chars().count()),
            Style::default(),
        );
    }
    Ok(())
}
