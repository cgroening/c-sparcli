//! Per-shortcut footer/help metadata and the auto-built keyboard help screen.
//!
//!   cargo run -p sparcli --example input_shortcuts_help
//!
//! The prompt's footer shows the visible shortcuts (F1 help, Ctrl-N new); the
//! Ctrl-X binding is kept off the footer (`in_footer: false`). Press F1 to open
//! a full-screen, filterable help screen built from the same `Shortcuts` set,
//! with `section()` headers and a `help_row()` documenting a built-in key.

use sparcli::*;

const ACT_HELP: i32 = 1;
const ACT_NEW: i32 = 2;
const ACT_DELETE: i32 = 3;

fn shortcuts() -> Shortcuts {
    // One builder drives both the footer and the help screen.
    Shortcuts::new()
        .section("Actions")
        .on_return_d(
            key_ctrl('n'),
            ACT_NEW,
            ShortcutDisplay { footer: Some("new"), help: Some("create an item"),
                              in_footer: true },
        )
        .on_return_d(
            key_ctrl('x'),
            ACT_DELETE,
            ShortcutDisplay { footer: Some("delete"),
                              help: Some("delete the highlighted item"),
                              in_footer: false }, // bound, hidden from footer
        )
        .section("Other")
        .on_return(key_fn(1), ACT_HELP, Some("help"))
        .section("Navigation")
        .help_row("↑/↓ or j/k", "move the cursor") // help-only (no binding)
        .help_row("Esc", "cancel")
}

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    let sc = shortcuts();
    loop {
        let answer = text_input(
            "Search (F1 help · ^N new)",
            TextInputOpts::new().placeholder("query…").shortcuts(&sc),
        )?;
        match answer {
            None => break, // Esc / Ctrl-C
            Some(_) if sc.fired() == ACT_HELP => {
                show_shortcuts(
                    &sc,
                    ShortcutHelpOpts { title: Some("My app · shortcuts"),
                                       ..Default::default() },
                );
                continue; // reopen the prompt
            }
            Some(q) => {
                println(&format!("  -> {q:?}"), Style::default());
                break;
            }
        }
    }
    Ok(())
}
