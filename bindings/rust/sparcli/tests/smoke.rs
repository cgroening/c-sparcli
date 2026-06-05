//! Non-interactive integration tests: pure functions and render-and-capture.
//!
//! The prompt tests exercise the "no TTY" error path. `make rust-test` sets
//! `SPARCLI_NO_TTY=1` so prompts fail cleanly even when the suite runs from
//! an interactive terminal (cargo only redirects stdin/stdout; `/dev/tty`
//! stays reachable). A direct `cargo test` from a terminal skips those tests
//! instead of letting prompts grab the keyboard.

use sparcli::{
    capture, fuzzy_match, key_ctrl, key_fn, AnsiMode, ColOpts, Color, Fuzzy,
    FuzzyOpts, PanelOpts, Style, Table, TableOpts, Text,
};

/// Guard for tests that assert the library's no-TTY behavior (prompt errors,
/// pager no-op).
///
/// Returns `false` (and logs a skip notice) when an interactive terminal is
/// attached and `SPARCLI_NO_TTY` is not set: a prompt would open `/dev/tty`
/// and block on a keystroke, and the pager would spawn a real `less` that
/// waits for `q`.
fn no_tty_behavior_testable() -> bool {
    if sparcli::input_available() {
        eprintln!("skipped: interactive terminal (run via `make rust-test`)");
        return false;
    }
    true
}

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
    if no_tty_behavior_testable() {
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
fn special_key_chords() {
    assert_eq!(sparcli::key_left().name(), "\u{2190}");
    assert_eq!(sparcli::key_right().name(), "\u{2192}");
}

#[test]
fn fuzzy_has_no_selection_before_run() {
    let mut fz = Fuzzy::new(FuzzyOpts::new().prompt("Find"));
    fz.add("alpha").add("beta");
    assert!(!fz.has_selection());
}

#[test]
fn fuzzy_sections_multi_and_styles() {
    use sparcli::FuzzyOrder;
    let mut fz = Fuzzy::new(
        FuzzyOpts::new()
            .multi()
            .section_counts()
            .order(FuzzyOrder::Column(0))
            .toggle_all_key(key_ctrl('a')),
    );
    fz.add_section("Monday");
    fz.add("buy milk");
    fz.add("call bob");

    // Stable ids + label / set_label.
    fz.set_id(1, 42);
    assert_eq!(fz.id_at(1), 42);
    assert_eq!(fz.id_at(99), 0);
    assert_eq!(fz.label(1).as_deref(), Some("buy milk"));
    fz.set_label(1, "BUY MILK");
    assert_eq!(fz.label(1).as_deref(), Some("BUY MILK"));

    // Checked set.
    fz.set_checked(1, true);
    fz.set_checked(2, true);
    assert_eq!(fz.checked_count(), 2);
    assert!(fz.is_checked(1));
    fz.check_all(false);
    assert_eq!(fz.checked_count(), 0);

    // Disabling clears + blocks the check.
    fz.set_disabled(2, true);
    fz.set_checked(2, true);
    assert!(!fz.is_checked(2));
}

#[test]
fn fuzzy_rich_and_styled_rows() {
    let style = Style::new().fg(Color::RED);
    let mut g = Fuzzy::new(FuzzyOpts::new().table(["Status", "Task"]));
    g.add_row_styled(["overdue", "pay invoice"], &[style, Style::new()]);
    assert_eq!(g.label(0).as_deref(), Some("overdue"));

    let mut badge = Text::new();
    badge.append("HI", Style::new());
    badge.append("GH", Style::bold());
    let mut r = Fuzzy::new(FuzzyOpts::new().table(["x"]));
    r.add_row_rich(&[badge]);
    assert_eq!(r.label(0).as_deref(), Some("HIGH"));
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
fn markup_backtick_inline_code() {
    // Backtick spans render in magenta (ESC[35m) with the backticks removed;
    // \` escapes a literal backtick.
    let t = Text::markup("run `make qa` or \\` now");
    let raw = capture::text(&t).lines().join("\n");
    assert!(raw.contains("\x1b[35m"));
    assert!(raw.contains("make qa"));
    assert!(!raw.contains('`') || raw.matches('`').count() == 1);
}

#[test]
fn markup_backtick_custom_code_style() {
    // A custom code_style overrides the default magenta (cyan = ESC[36m).
    let t = sparcli::markup::parse_opts(
        "see `code`",
        sparcli::MarkupOpts {
            code_style: Style::default().fg(Color::CYAN),
            ..Default::default()
        },
    );
    let raw = capture::text(&t).lines().join("\n");
    assert!(raw.contains("\x1b[36m"));
    assert!(!raw.contains("\x1b[35m"));
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
    // Off-terminal, a live session buffers updates silently and prints only
    // the FINAL frame on end. capture::output keeps the C-level prints out
    // of the harness output and lets the test assert the buffering rule.
    let rendered = capture::output(|| {
        let live = sparcli::Live::begin(sparcli::LiveOpts::new());
        let frame = sparcli::capture::str("frame one");
        live.update(&frame);
        live.update_str("frame two\nsecond line");
        live.end();
    });
    assert!(!rendered.contains("frame one")); // superseded update is dropped
    assert!(rendered.contains("frame two"));
    assert!(rendered.contains("second line"));

    // Transient + builder options exercise every opts field; a transient
    // session prints nothing at all on end.
    let transient_out = capture::output(|| {
        let transient = sparcli::Live::begin(
            sparcli::LiveOpts::new().transient().show_cursor(),
        );
        transient.update_str("never shown");
        drop(transient); // Drop ends the session
    });
    assert!(transient_out.is_empty());
}

#[test]
fn live_prompt_rows_builder() {
    // prompt_rows reserves rows below the frame for a REPL prompt. Off
    // terminal the session buffers, but the opts must cross the FFI cleanly.
    let rendered = capture::output(|| {
        let live =
            sparcli::Live::begin(sparcli::LiveOpts::new().prompt_rows(2));
        live.update_str("dashboard frame");
        live.end();
    });
    assert!(rendered.contains("dashboard frame"));
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

    if !no_tty_behavior_testable() {
        return;
    }
    // Without a usable TTY the prompt must fail cleanly with Unavailable
    // and never touch the attached history.
    let history = History::new(HistoryOpts::new());
    let result = text_input(
        "repl>",
        TextInputOpts::new().history(&history).no_history_add(),
    );
    assert!(result.is_err());
    assert_eq!(history.count(), 0);
}

#[test]
fn box_style_builders_and_boxed_widgets_without_tty() {
    use sparcli::{
        text_input, BorderStyle, BorderType, BoxStyle, Color, Edges, Select,
        SelectOpts, TextInputOpts,
    };

    // The BoxStyle builders compose; .border()/.new() imply enabled.
    let boxed = BoxStyle::new(BorderStyle::new(BorderType::Double))
        .bg(Color::BLUE)
        .padding(Edges::symmetric(0, 2))
        .margin(Edges { left: 1, ..Default::default() })
        .width(40);
    assert!(boxed.enabled && boxed.width == 40);

    // Width-mode + background-extent builders for list widgets.
    let content = BoxStyle::default()
        .bg(Color::BLACK)
        .width_content(20, 50)
        .bg_extent(sparcli::BgExtent::Text);
    assert!(content.min_width == 20 && content.max_width == 50);
    assert!(BoxStyle::default().width_full().width_mode
            == sparcli::WidthMode::Full);

    if !no_tty_behavior_testable() {
        return;
    }
    // A migrated text widget and a newly-boxed list widget both accept a box
    // style and still fail cleanly off a TTY.
    let r = text_input("name", TextInputOpts::new().box_style(boxed));
    assert!(r.is_err());

    let mut sel = Select::new(SelectOpts::new().prompt("pick").box_style(boxed));
    sel.add("a");
    sel.add("b");
    assert!(sel.run().is_err());
}

#[test]
fn pager_is_noop_off_terminal() {
    if !no_tty_behavior_testable() {
        return;
    }
    // Without a usable terminal (or with SPARCLI_NO_TTY set) the session is
    // a no-op and must end cleanly with status 0 without spawning a pager.
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

    // capture::output: assert the rendered panel instead of spilling it
    // into the harness output (libtest cannot intercept C-level stdout).
    let rendered = capture::output(|| report.print());
    assert!(rendered.contains("boom"));
    assert!(rendered.contains("a reason"));
    assert!(rendered.contains("a hint"));
}

#[test]
fn input_without_tty_errors() {
    if !no_tty_behavior_testable() {
        return;
    }
    // Without a usable TTY prompts return an error (callers fall back to a
    // default), never a panic.
    let r = sparcli::confirm("Proceed?", sparcli::ConfirmOpts::new());
    assert!(r.is_err());
    assert!(!sparcli::input_available());
}

#[test]
fn text_input_suggest_dropdown_without_tty_errors() {
    if !no_tty_behavior_testable() {
        return;
    }
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
fn text_input_validate_builder_without_tty() {
    use sparcli::{password_input, text_input, PasswordOpts, TextInputOpts};

    // The validate closure must cross the FFI boundary cleanly (boxed +
    // trampoline); off-TTY the prompt errors before the validator ever runs.
    if !no_tty_behavior_testable() {
        return;
    }
    let r = text_input(
        "Name",
        TextInputOpts::new().validate(|v: &str| {
            if v.trim().is_empty() {
                Err("must not be empty".into())
            } else {
                Ok(())
            }
        }),
    );
    assert!(r.is_err()); // SPARCLI_NO_TTY -> Unavailable, validator not invoked

    let p = password_input(
        "Secret",
        PasswordOpts::new().validate(|v: &str| {
            if v.len() < 8 {
                Err("too short".into())
            } else {
                Ok(())
            }
        }),
    );
    assert!(p.is_err());
}

#[test]
fn number_input_text_without_tty_errors() {
    if !no_tty_behavior_testable() {
        return;
    }
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

#[test]
fn color_by_name_resolves_ansi_palette_and_hex() {
    // ANSI name -> the named color (index 1..8).
    let red = Color::by_name("red").expect("red resolves");
    let raw = capture::panel(
        "x",
        PanelOpts::new().border(
            sparcli::BorderStyle::new(sparcli::BorderType::Single).color(red),
        ),
    );
    assert!(raw.lines().iter().any(|l| l.contains("\x1b[31m")));

    // Palette name -> a 24-bit RGB color (accent = #8cd2cc).
    let accent = Color::by_name("accent").expect("accent resolves");
    let raw = capture::panel(
        "x",
        PanelOpts::new().border(
            sparcli::BorderStyle::new(sparcli::BorderType::Single)
                .color(accent),
        ),
    );
    assert!(raw.lines().iter().any(|l| l.contains("38;2;140;210;204")));

    // Unknown names (incl. hex, which is not a name) resolve to None.
    assert!(Color::by_name("#ff0000").is_none());
    assert!(Color::by_name("definitely-not-a-color").is_none());
}

#[test]
fn theme_apply_and_reset() {
    use sparcli::{Style, Theme};
    // Installing a theme must cross the FFI cleanly (copies markers/strings);
    // reset reverts to the built-in defaults. No TTY needed.
    Theme::new()
        .accent(Color::MAGENTA)
        .marker("> ")
        .checkbox_on("[x]")
        .prompt_style(Style::bold())
        .apply();
    sparcli::reset_theme();
    sparcli::set_theme(None); // idempotent clear
}

#[test]
fn rich_text_alert_rule_pad_align_capture() {
    // The ScText output variants render the rich content through the normal
    // stack; capture::output keeps the C prints out of the harness output.
    let rendered = capture::output(|| {
        let t = Text::markup("[bold]title[/]");
        sparcli::rule_text(&t, sparcli::RuleOpts::new());
        sparcli::alert::text(sparcli::AlertType::Info, &t);
        sparcli::pad_text(&t, sparcli::PadOpts { left: 4, ..Default::default() });
        sparcli::align_text(&t, sparcli::Align::Center, 40);
    });
    assert!(rendered.contains("title"));
}

#[test]
fn text_append_badge_adds_span() {
    let mut t = Text::new();
    t.append("status ", Style::default())
        .append_badge("OK", sparcli::BadgeOpts::new());
    assert_eq!(t.span_count(), 2);
    let r = capture::text(&t);
    let raw = r.lines().join("\n");
    assert!(raw.contains("OK"));
    assert!(raw.contains('['));
}

#[test]
fn columns_panel_text_and_nested() {
    use sparcli::{ColItem, Columns, ColumnsOpts};
    let inner_text = Text::markup("[green]left[/]");
    let mut inner = Columns::new(ColumnsOpts::new());
    inner.add_str("a", ColItem::new()).add_str("b", ColItem::new());

    let mut cols = Columns::new(ColumnsOpts::new().gap(2));
    cols.add_panel_text(&inner_text, PanelOpts::new().single(), ColItem::new())
        .add_columns(&inner, ColItem::new());
    let r = capture::columns(&cols);
    let raw = r.lines().join("\n");
    assert!(raw.contains("left"));
    assert!(raw.contains('a') && raw.contains('b'));
}

#[test]
fn live_update_text_and_table_off_terminal() {
    let rendered = capture::output(|| {
        let live = sparcli::Live::begin(sparcli::LiveOpts::new());
        let t = Text::from_text("text frame");
        live.update_text(&t);
        let mut tbl = Table::new();
        tbl.column("C", Default::default());
        tbl.row(["final cell"]);
        live.update_table(&tbl, TableOpts::new());
        live.end();
    });
    // Only the final (table) frame is printed off-terminal.
    assert!(rendered.contains("final cell"));
    assert!(!rendered.contains("text frame"));
}

#[test]
fn fuzzy_table_view_builds() {
    // Table-view opts (headers, search columns, table_opts) must cross the FFI
    // boundary in sc_fuzzy_new without crashing; add_row populates columns.
    let mut fz = Fuzzy::new(
        FuzzyOpts::new()
            .prompt("Find")
            .table(["Name", "Role"])
            .search_columns(0)
            .table_opts(TableOpts::new().header_row(true)),
    );
    fz.add_row(["Ada", "Engineer"]).add_row(["Alan", "Founder"]);
    assert!(!fz.has_selection());
}

#[test]
fn decode_key_and_chord_matching() {
    use sparcli::{decode_key, KeyType};
    // A plain character.
    let (key, n) = decode_key(b"a");
    assert_eq!(n, 1);
    assert_eq!(key.kind(), KeyType::Char);
    assert_eq!(key.char_value(), Some('a'));

    // An up-arrow CSI sequence.
    let (key, n) = decode_key(b"\x1b[A");
    assert_eq!(n, 3);
    assert_eq!(key.kind(), KeyType::Up);

    // A Ctrl-E control byte, and a chord that matches it.
    let (key, _) = decode_key(&[0x05]);
    assert_eq!(key.kind(), KeyType::CtrlE);
    assert!(key_ctrl('e').matches(&key));
    assert!(!key_fn(2).matches(&key));

    // A lone ESC needs more bytes: 0 consumed, KeyType::None.
    let (_, n) = decode_key(b"\x1b");
    assert_eq!(n, 0);
}

#[test]
fn shortcuts_find_matches_registered_chord() {
    use sparcli::{decode_key, Shortcuts};
    let sc = Shortcuts::new()
        .on_return(key_fn(2), 1, Some("save"))
        .on_return(key_ctrl('e'), 2, None);
    let (key, _) = decode_key(&[0x05]); // Ctrl-E -> second shortcut (index 1)
    assert_eq!(sc.find(&key), Some(1));
    let (other, _) = decode_key(b"a");
    assert_eq!(sc.find(&other), None);
}

#[test]
fn logger_terminal_sink_writes_to_stream() {
    use std::io::Read;
    // add_terminal duplicates the fd and the C sink writes colored records to
    // it; the duplicate is closed on drop, flushing the file.
    let path = std::env::temp_dir()
        .join(format!("sparcli-rust-term-{}.log", std::process::id()));
    let _ = std::fs::remove_file(&path);
    {
        let file = std::fs::File::create(&path).unwrap();
        let logger = sparcli::Logger::new(
            sparcli::LoggerOpts::new().hide_timestamps(),
        )
        .add_terminal(file, sparcli::LogLevel::Debug);
        logger.info("terminal record");
    }
    let mut content = String::new();
    std::fs::File::open(&path)
        .unwrap()
        .read_to_string(&mut content)
        .unwrap();
    let _ = std::fs::remove_file(&path);
    assert!(content.contains("terminal record"));
}

#[test]
fn capture_rich_text_variants_and_tree_prefix() {
    use sparcli::{Tree, TreeNode, TreeOpts};
    // capture::panel_text / rule_text take rich text; panel_rendered frames an
    // already-captured rendering and preserves its embedded ANSI.
    let t = Text::markup("[bold]title[/]");
    assert!(capture::panel_text(&t, PanelOpts::new().single())
        .lines()
        .iter()
        .any(|l| l.contains("title")));
    assert!(capture::rule_text(&t, sparcli::RuleOpts::new())
        .lines()
        .iter()
        .any(|l| l.contains("title")));

    let inner = capture::str("body");
    let framed = capture::panel_rendered(&inner, PanelOpts::new().single());
    assert!(framed.lines().iter().any(|l| l.contains("body")));
    assert!(framed.line_count() >= 3); // top border + content + bottom border

    // Tree::add_prefixed attaches a styled prefix marker before the node text.
    let mut tree = Tree::new(TreeOpts::new());
    tree.add_prefixed(
        "deploy",
        TreeNode::root(),
        Style::default(),
        "✔",
        Style::default().fg(Color::GREEN),
    );
    let raw = capture::tree(&tree).lines().join("\n");
    assert!(raw.contains('✔'));
    assert!(raw.contains("deploy"));
}

#[test]
fn table_footer_and_header_sep_styling() {
    use sparcli::{TableBorder, TableFooter, TableHeader};
    // Footer-section background and the header-row separator color are now
    // configurable (were hardcoded to none). Blue bg = ESC[44m, red fg = ESC[31m.
    let mut t = Table::new();
    t.column("Item", Default::default())
        .column("Qty", Default::default());
    t.row(["apples", "3"]);
    t.footer_row(["total", "3"]);
    let r = capture::table(
        &t,
        TableOpts {
            header: TableHeader { row: true, ..Default::default() },
            footer: TableFooter {
                row_bg: Color::BLUE,
                style: Style::bold(),
                ..Default::default()
            },
            border: TableBorder {
                kind: sparcli::BorderType::Single,
                header_row_sep_color: Color::RED,
                ..Default::default()
            },
            ..Default::default()
        },
    );
    let raw = r.lines().join("\n");
    assert!(raw.contains("\x1b[44m"), "footer bg: {raw:?}");
    assert!(raw.contains("\x1b[31m"), "header sep color: {raw:?}");
}

#[test]
fn progress_thresholds_color_by_ratio() {
    use sparcli::{ProgressBar, ProgressBarOpts, ProgressThresholds};
    // Threshold coloring: at 20% (below the 0.5 mid boundary) the fill uses the
    // low color (green = ESC[32m), not the default fill color.
    let out = capture::output(|| {
        let mut bar = ProgressBar::new(
            ProgressBarOpts::new().width(40).thresholds(
                ProgressThresholds::new(Color::GREEN, Color::YELLOW, Color::RED)
                    .ratios(0.5, 0.8),
            ),
        );
        bar.draw(20.0, 100.0);
        bar.finish(20.0, 100.0);
    });
    assert!(
        out.contains("\x1b[32m"),
        "expected green fill at low ratio, got: {out:?}"
    );
}

#[test]
fn palette_border_emits_rgb_escape() {
    // sparcli::palette::ACCENT is #8cd2cc = rgb(140,210,204); a panel border in
    // that color must emit the 24-bit ANSI sequence for it.
    let border = sparcli::BorderStyle::new(sparcli::BorderType::Single)
        .color(sparcli::palette::ACCENT);
    let r = capture::panel("x", PanelOpts::new().border(border));
    assert!(
        r.lines().iter().any(|l| l.contains("38;2;140;210;204")),
        "expected accent rgb escape in: {:?}",
        r.lines()
    );
}

#[test]
fn humanize_formats() {
    use sparcli::humanize::{self, ByteUnit, HumanizeOpts};
    assert_eq!(humanize::bytes(1536, ByteUnit::Si), "1.5 KB");
    assert_eq!(humanize::bytes(1536, ByteUnit::Iec), "1.5 KiB");
    assert_eq!(humanize::number(1_234_567.0, HumanizeOpts::new()), "1,234,567");
    assert_eq!(humanize::compact(12_400.0, HumanizeOpts::new()), "12.4k");
    assert_eq!(humanize::percent(0.42, HumanizeOpts::new()), "42%");
    assert_eq!(humanize::duration(93.0), "1m 33s");
    assert_eq!(humanize::duration_clock(3725.0), "01:02:05");
    let de = HumanizeOpts::new().decimal_sep(',');
    assert_eq!(humanize::bytes_opts(1536, ByteUnit::Iec, de), "1,5 KiB");
    let now = 1_000_000_000_i64;
    assert_eq!(humanize::relative(now - 3 * 3600, now), "3 hours ago");
}

#[test]
fn diff_capture_has_changes() {
    let r = capture::diff(
        "a\nb\nc\n",
        "a\nB\nc\n",
        sparcli::DiffOpts::new().context(0),
    );
    let lines = r.lines();
    assert!(lines.iter().any(|l| l.contains("-b")), "{:?}", lines);
    assert!(lines.iter().any(|l| l.contains("+B")), "{:?}", lines);
}

#[test]
fn multiprogress_smoke() {
    // Off a TTY the live display buffers; this just exercises the lifecycle.
    let mut mp = sparcli::MultiProgress::begin(sparcli::MultiProgressOpts::new());
    let a = mp.add("a", sparcli::ProgressBarOpts::new().show_percent(true));
    let b = mp.add("b", sparcli::ProgressBarOpts::new().show_percent(true));
    assert_eq!(a, 0);
    assert_eq!(b, 1);
    mp.update(a, 100.0, 100.0);
    mp.update(b, 50.0, 100.0);
    mp.set_label(b, "b2");
    mp.end();
}
