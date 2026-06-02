//! Non-interactive integration tests: pure functions and render-and-capture.
//! No TTY needed, so they run anywhere (CI included).

use sparcli::{
    capture, fuzzy_match, key_ctrl, key_fn, AnsiMode, ColOpts, Color,
    PanelOpts, Style, Table, TableOpts, Text,
};

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
    t.column("Name", Default::default())
        .column("Age", Default::default());
    t.row(["Ada", "36"]).row(["Alan", "41"]);
    let r = capture::table(
        &t,
        TableOpts::new()
            .border(sparcli::BorderType::Single)
            .header_row(true),
    );
    let lines = r.lines();
    assert!(lines.iter().any(|l| l.contains("Ada")));
    assert!(lines.iter().any(|l| l.contains("Alan")));
}

#[test]
fn capture_table_column_style() {
    // A per-column style must color unstyled data cells (cyan = ESC[36m).
    let mut t = Table::new();
    t.column("Name", ColOpts::new().style(Style::bold().fg(Color::CYAN)))
        .column("Age", Default::default());
    t.row(["Ada", "36"]);
    let r = capture::table(&t, TableOpts::new().header_row(true));
    let raw = r.lines().join("\n");
    assert!(raw.contains("\x1b[36m"));
    assert!(raw.contains("Ada"));
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
    t.append("a", Style::bold().fg(Color::RED))
        .append("b", Style::default());
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

#[test]
fn number_input_text_without_tty_errors() {
    // The exact-text variant follows the same no-TTY contract.
    let r = sparcli::number_input_text(
        "Amount",
        sparcli::NumberOpts::new().decimals(2).decimal_sep(','),
    );
    assert!(r.is_err());
}


#[test]
fn ansi_injection_protection() {
    // All assertions in one test: the global toggle must not race with
    // other tests capturing output in parallel.

    // 1. Default: injected escape codes and control bytes are stripped.
    assert!(!sparcli::allow_ansi());
    let r = capture::panel(
        "\x1b[31mevil\x1b]0;title\x07\x1b[0m content",
        PanelOpts::new().single(),
    );
    let lines = r.lines();
    assert!(!lines.iter().any(|l| l.contains("\x1b[31m")));
    assert!(lines
        .iter()
        .any(|l| sparcli::strip_ansi(l).contains("evil content")));

    // 2. Per-widget override: codes pass through, borders stay aligned.
    let r = capture::panel(
        "\x1b[32mgreen\x1b[0m",
        PanelOpts::new().single().ansi(AnsiMode::Allow),
    );
    let lines = r.lines();
    assert!(lines.iter().any(|l| l.contains("\x1b[32m")));
    let widths: Vec<usize> = lines
        .iter()
        .map(|l| sparcli::strip_ansi(l).chars().count())
        .collect();
    assert!(widths.windows(2).all(|w| w[0] == w[1]));

    // 3. Global setter round-trip (reset to the default afterwards).
    sparcli::set_allow_ansi(true);
    assert!(sparcli::allow_ansi());
    sparcli::set_allow_ansi(false);
    assert!(!sparcli::allow_ansi());
}
