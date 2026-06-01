//! Interactive walkthrough of the input widgets. Run in a real terminal:
//! `cargo run --example input_demo`.

use sparcli::{
    confirm, datepicker, number_input, number_input_text, password_input,
    text_input, textarea, ConfirmOpts, DatePickerOpts, Fuzzy, FuzzyOpts,
    NumberOpts, PasswordOpts, Select, SelectOpts, Shortcuts, TextInputOpts,
    TextareaOpts,
};

fn main() -> sparcli::Result<()> {
    if !sparcli::input_available() {
        eprintln!("No interactive terminal - run this directly in a terminal.");
        return Ok(());
    }

    if let Some(yes) = confirm(
        "Proceed with the demo?",
        ConfirmOpts::new().default_yes(true),
    )? {
        println!("-> {yes}");
    }

    // Text input with a custom Ctrl-E shortcut + external editor (Ctrl-G).
    let sc = Shortcuts::new().on_return(
        sparcli::key_ctrl('e'),
        1,
        Some("edit elsewhere"),
    );
    if let Some(name) = text_input(
        "Your name (Ctrl-G editor, Ctrl-E action)",
        TextInputOpts::new()
            .placeholder("Ada Lovelace")
            .external_editor(true)
            .shortcuts(&sc),
    )? {
        if sc.fired() == 1 {
            println!("-> Ctrl-E action requested");
        } else {
            println!("-> {name:?}");
        }
    }

    if let Some(pw) = password_input("Password", PasswordOpts::new())? {
        println!("-> {} chars", pw.chars().count());
    }

    if let Some(qty) =
        number_input("Quantity", NumberOpts::new().range(0.0, 100.0).step(5.0))?
    {
        println!("-> {qty}");
    }

    // Exact text (never via f64), comma as decimal separator - feed the
    // result to a decimal type for money amounts. start_empty starts with
    // an empty field instead of "0,00".
    if let Some(amount) = number_input_text(
        "Amount",
        NumberOpts::new()
            .decimals(2)
            .step(0.5)
            .decimal_sep(',')
            .start_empty(true),
    )? {
        println!("-> {amount} (exact)");
    }

    if let Some(notes) = textarea(
        "Notes (Ctrl-D submit, Ctrl-G editor)",
        TextareaOpts::new().boxed(48).external_editor(true),
    )? {
        println!("-> {} bytes", notes.len());
    }

    let mut sel = Select::new(SelectOpts::new().prompt("Pick a fruit"));
    sel.add("Apple").add("Banana").add("Cherry");
    if let Some(i) = sel.run_one()? {
        println!("-> index {i}");
    }

    let mut fz = Fuzzy::new(FuzzyOpts::new().prompt("Find a city"));
    for c in ["Tokyo", "London", "Berlin", "Paris", "Oslo"] {
        fz.add(c);
    }
    if let Some(i) = fz.run()? {
        println!("-> index {i}");
    }

    if let Some(d) =
        datepicker(None, DatePickerOpts::new().prompt("Pick a date"))?
    {
        println!("-> {:04}-{:02}-{:02}", d.year, d.month, d.day);
    }

    Ok(())
}
