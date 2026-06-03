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
fn calc_eval_pure() {
    // Pure expression evaluator behind the calculator mode.
    assert_eq!(sparcli::calc_eval("1+2*3"), Some(7.0));
    assert_eq!(sparcli::calc_eval("1,5+2*3"), Some(7.5));
    assert_eq!(sparcli::calc_eval("(1+2)*3"), Some(9.0));
    assert_eq!(sparcli::calc_eval("1/0"), None);
    assert_eq!(sparcli::calc_eval("garbage"), None);

    // The calculator opts cross the FFI boundary cleanly (no TTY -> error).
    let r = sparcli::number_input(
        "Amount",
        sparcli::NumberOpts::new()
            .decimals(2)
            .calculator(true)
            .calc_store_rounded(true)
            .calc_show_precise(true),
    );
    assert!(r.is_err());
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
fn text_link_has_zero_width_escapes() {
    let mut t = Text::new();
    t.append_link("click", "https://example.com", Style::default());
    assert_eq!(t.span_count(), 1);
    // OSC-8 escape bytes occupy no visible columns
    assert_eq!(t.visible_width(), 5);
}

#[test]
fn paths_reject_invalid_app_names() {
    // Invalid names must never resolve (no filesystem side effects)
    assert!(sparcli::paths::config("a/b").is_none());
    assert!(sparcli::paths::config("..").is_none());
    assert!(sparcli::paths::config("").is_none());
    assert!(sparcli::paths::file(
        sparcli::paths::Kind::Config,
        "app",
        "../escape"
    )
    .is_none());
}

#[test]
fn live_buffers_frames_off_terminal() {
    // Off-terminal (cargo test pipes output), a live session must buffer
    // updates silently and survive begin/update/end without crashing; the
    // FFI structs must cross the boundary cleanly.
    let live = sparcli::Live::begin(sparcli::LiveOpts::new());
    let frame = sparcli::capture::str("frame one");
    live.update(&frame);
    live.update_str("frame two\nsecond line");
    live.end();

    // Transient + builder options exercise every opts field.
    let transient = sparcli::Live::begin(
        sparcli::LiveOpts::new().transient().show_cursor(),
    );
    transient.update_str("never shown");
    drop(transient);   // Drop ends the session
}

#[test]
fn live_prompt_rows_builder() {
    // prompt_rows reserves rows below the frame for a REPL prompt. Off
    // terminal the session buffers, but the opts must cross the FFI cleanly.
    let live = sparcli::Live::begin(sparcli::LiveOpts::new().prompt_rows(2));
    live.update_str("dashboard frame");
    live.end();
}

#[test]
fn history_stores_and_recalls_entries() {
    use sparcli::{History, HistoryOpts};

    // Storage semantics: copy, dedupe consecutive duplicates, cap eviction.
    let mut history = History::new(HistoryOpts::new());
    history.add("first").add("first").add("second");
    assert_eq!(history.count(), 2);
    assert_eq!(history.get(0).as_deref(), Some("first"));
    assert_eq!(history.get(1).as_deref(), Some("second"));
    assert_eq!(history.get(99), None);

    let mut capped = History::new(HistoryOpts::new().max_entries(2));
    capped.add("one").add("two").add("three");
    assert_eq!(capped.count(), 2);
    assert_eq!(capped.get(0).as_deref(), Some("two"));

    // keep_duplicates retains consecutive duplicates.
    let mut dup = History::new(HistoryOpts::new().keep_duplicates());
    dup.add("x").add("x");
    assert_eq!(dup.count(), 2);
}

#[test]
fn history_persists_to_file() {
    use sparcli::{History, HistoryOpts};

    let dir = std::env::temp_dir().join(format!(
        "sparcli-rust-hist-{}",
        std::process::id()
    ));
    std::fs::create_dir_all(&dir).unwrap();
    let file = dir.join("history").to_string_lossy().into_owned();

    {
        let mut history =
            History::new(HistoryOpts::new().file(file.clone()));
        history.add("persisted line");
    } // drop saves the file

    let reloaded = History::new(HistoryOpts::new().file(file));
    assert_eq!(reloaded.count(), 1);
    assert_eq!(reloaded.get(0).as_deref(), Some("persisted line"));

    drop(reloaded);
    let _ = std::fs::remove_dir_all(&dir);
}

#[test]
fn text_input_with_history_without_tty_errors() {
    use sparcli::{text_input, History, HistoryOpts, TextInputOpts};

    // Under cargo test there is no TTY: the prompt must fail cleanly with
    // Unavailable and never touch the attached history.
    let history = History::new(HistoryOpts::new());
    let result = text_input(
        "repl>",
        TextInputOpts::new().history(&history).no_history_add(),
    );
    assert!(result.is_err());
    assert_eq!(history.count(), 0);
}

#[test]
fn pager_is_noop_off_terminal() {
    // Under `cargo test` stdout is not a terminal -> the session is a no-op
    // and must end cleanly with status 0 without spawning a pager.
    let pager = sparcli::Pager::begin(sparcli::PagerOpts::new());
    assert_eq!(pager.end(), 0);
}

#[test]
fn logger_writes_plain_file_records() {
    use std::io::Read;

    let path = std::env::temp_dir().join(format!(
        "sparcli-rust-log-{}.log",
        std::process::id()
    ));
    let _ = std::fs::remove_file(&path);

    {
        let logger = sparcli::Logger::new(
            sparcli::LoggerOpts::new().hide_timestamps(),
        )
        .add_file(path.to_str().unwrap(), sparcli::LogLevel::Debug);
        logger.info("rust record %d");   // literal %d must NOT be interpreted
        logger.debug("debug detail");
    } // drop flushes + closes the file sink

    let mut content = String::new();
    std::fs::File::open(&path)
        .unwrap()
        .read_to_string(&mut content)
        .unwrap();
    let _ = std::fs::remove_file(&path);

    // Records are plain text (no ESC bytes), and the message is data:
    // the "%d" arrives literally instead of being format-expanded.
    assert!(content.contains("INFO"));
    assert!(content.contains("rust record %d"));
    assert!(content.contains("debug detail"));
    assert!(!content.contains('\u{1b}'));
}

#[test]
fn global_log_level_roundtrip() {
    sparcli::log::set_level(sparcli::LogLevel::Error);
    assert_eq!(sparcli::log::level(), sparcli::LogLevel::Error);
    sparcli::log::reset();
    assert_eq!(sparcli::log::level(), sparcli::LogLevel::Info);
}

#[test]
fn error_report_builder() {
    // Builder chains, stores the exit code, and printing does not exit
    let report = sparcli::ErrorReport::new("boom")
        .cause("a reason")
        .hint("a hint")
        .code(7);
    assert_eq!(report.exit_code(), 7);
    report.print(); // renders to the (non-TTY) output stream; must not panic
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
fn text_input_suggest_dropdown_without_tty_errors() {
    // The dropdown SuggestOpts must cross the FFI boundary cleanly; without
    // a TTY the prompt still errors instead of crashing on the new fields.
    let r = sparcli::text_input(
        "Cmd",
        sparcli::TextInputOpts::new()
            .suggestions(vec!["commit".into(), "checkout".into()])
            .suggest(
                sparcli::SuggestOpts::dropdown()
                    .fuzzy()
                    .max_visible(3)
                    .border(sparcli::BorderStyle::new(
                        sparcli::BorderType::Rounded,
                    ))
                    .selected_style(
                        sparcli::Style::new()
                            .fg(sparcli::Color::BLACK)
                            .bg(sparcli::Color::CYAN),
                    ),
            ),
    );
    assert!(r.is_err());
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
