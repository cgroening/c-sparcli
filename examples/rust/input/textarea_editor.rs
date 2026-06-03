//! Multi-line entry and the external-editor hook.
//!
//!   cargo run -p sparcli --example input_textarea_editor
//!
//! Keys: Enter inserts a newline, Ctrl-D submits, Ctrl-G opens $EDITOR.

use sparcli::*;

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    // Boxed multi-line editor with soft-wrapped long lines.
    if let Some(notes) =
        textarea("Release notes (Ctrl-D submits)", TextareaOpts::new().boxed(52))?
    {
        println(&format!("  -> {} bytes", notes.len()), Style::default());
    }

    // External editor: Ctrl-G opens $EDITOR; text_input collapses newlines.
    if let Some(message) = text_input(
        "Commit message (Ctrl-G opens the editor)",
        TextInputOpts::new().external_editor(true),
    )? {
        println(&format!("  -> {message:?}"), Style::default());
    }

    Ok(())
}
