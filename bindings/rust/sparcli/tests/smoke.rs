//! Non-interactive integration tests: pure functions and render-and-capture.
//! No TTY needed, so they run anywhere (CI included).

use sparcli::{capture, fuzzy_match, key_ctrl, key_fn, Color, PanelOpts, Style, Table, TableOpts, Text};

#[test]
fn version_is_reported() {
    let (a, _b, _c) = sparcli::version();
    assert!(a >= 0);
    assert!(!sparcli::version_string().is_empty());
}

#[test]
fn strip_and_truncate() {
    assert_eq!(sparcli::strip_ansi("\x1b[31mhi\x1b[0m"), "hi");
    let t = sparcli::truncate("hello world", 5, "…");
    // 4 visible columns kept + the ellipsis.
    assert!(t.starts_with("hell"));
    assert!(t.ends_with('…'));
}

#[test]
fn fuzzy_match_pure() {
    let (ok, score) = fuzzy_match("ab", "cab");
    assert!(ok);
    assert!(score > 0);
    assert!(!fuzzy_match("xyz", "cab").0);
}

#[test]
fn chord_names() {
    assert_eq!(key_fn(2).name(), "F2");
    assert_eq!(key_ctrl('e').name(), "^E");
}

#[test]
fn capture_panel_contains_text() {
    let r = capture::panel("hello", PanelOpts::new().single().title("hi"));
    let lines = r.lines();
    assert!(lines.iter().any(|l| l.contains("hello")));
    assert!(lines.iter().any(|l| l.contains("hi")));
    assert!(r.line_count() >= 3); // top border, content, bottom border
}

#[test]
fn capture_table_renders() {
    let mut t = Table::new();
    t.column("Name", Default::default()).column("Age", Default::default());
    t.row(["Ada", "36"]).row(["Alan", "41"]);
    let r = capture::table(&t, TableOpts::new().border(sparcli::BorderType::Single).header_row(true));
    let lines = r.lines();
    assert!(lines.iter().any(|l| l.contains("Ada")));
    assert!(lines.iter().any(|l| l.contains("Alan")));
}

#[test]
fn capture_text_markup() {
    let t = Text::markup("[bold]X[/] y");
    let r = capture::text(&t);
    assert!(r.lines().iter().any(|l| l.contains('X')));
}

#[test]
fn styled_text_builds() {
    let mut t = Text::new();
    t.append("a", Style::bold().fg(Color::RED)).append("b", Style::default());
    assert_eq!(t.span_count(), 2);
    assert_eq!(t.visible_width(), 2);
}

#[test]
fn input_without_tty_errors() {
    // No controlling terminal under `cargo test`, so prompts return an error
    // (callers fall back to a default), never a panic.
    let r = sparcli::confirm("Proceed?", sparcli::ConfirmOpts::new());
    assert!(r.is_err());
    assert!(!sparcli::input_available());
}
