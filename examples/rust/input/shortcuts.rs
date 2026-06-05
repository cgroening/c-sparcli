//! Custom keyboard shortcuts on an input widget.
//!
//!   cargo run -p sparcli --example input_shortcuts
//!
//! A RETURN-mode shortcut ends the prompt and is reported via `fired()`; a
//! CALLBACK-mode shortcut runs a closure and keeps the prompt open. Shortcuts
//! attach to confirm / text_input opts. This example also installs a
//! process-wide `Theme` so the widget inherits an accent and marker.

use std::cell::Cell;
use std::rc::Rc;

use sparcli::*;

const ACTION_HELP: i32 = 1;

fn main() -> sparcli::Result<()> {
    if !input_available() {
        alert::warning("Run this example in a real terminal (not piped).");
        return Ok(());
    }

    // Process-wide input defaults inherited by every widget below (a per-call
    // opt would still win). reset_theme() would revert to the built-ins.
    Theme::new()
        .accent(Color::MAGENTA)
        .prompt_style(Style::bold().fg(Color::MAGENTA))
        .apply();

    // The callback must be 'static, so shared state goes through an Rc.
    let reloads = Rc::new(Cell::new(0));
    let reloads_cb = Rc::clone(&reloads);

    let shortcuts = Shortcuts::new()
        .on_return(key_fn(2), ACTION_HELP, Some("help"))
        .on_callback(key_ctrl('r'), Some("reload"), move |_id| {
            reloads_cb.set(reloads_cb.get() + 1);
            true // keep the prompt open
        });

    let result = text_input(
        "Search (F2 help · Ctrl-R reload)",
        TextInputOpts::new().placeholder("query…").shortcuts(&shortcuts),
    )?;

    match result {
        Some(_) if shortcuts.fired() == ACTION_HELP => {
            println("  -> F2 requested help", Style::default());
        }
        Some(query) => {
            println(&format!("  -> searching for {query:?}"), Style::default());
        }
        None => println("  -> cancelled", Style::default()),
    }
    println(&format!("  reload pressed {} time(s)", reloads.get()), Style::dim());

    Ok(())
}
