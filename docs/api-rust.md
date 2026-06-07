# sparcli – Rust API Reference

Safe, idiomatic Rust bindings for sparcli, in the `bindings/rust/` cargo workspace:

- **`sparcli-sys`** – raw FFI. `build.rs` compiles the C sources with the `cc` crate, so a plain `cargo build` needs only a Rust toolchain (no prior `make`, no system install). Bindings are committed (`src/bindings.rs`); build with `--features regen` to regenerate them with bindgen (needs libclang).
- **`sparcli`** – the safe wrapper: RAII handles, builder-style option structs, `Result<Option<T>>` prompts, closures for callbacks.

```toml
[dependencies]
sparcli = { path = "bindings/rust/sparcli" }
```

## Design

- **RAII handles** (`Text`, `Table`, `List`, `Tree`, `Kv`, `Columns`, `Rendered`, `ProgressBar`, `Spinner`, `Select`, `Fuzzy`, `History`) free themselves on drop. They hold raw pointers and are therefore **not `Send`/`Sync`**: the C output target is thread-local and the input session is process-global, so build and use a handle on one thread (each thread may build its own).
- **Builders.** Every `*Opts` is a plain struct with `Default` and chainable setters; public fields give full access. Borrowed strings (`Title`, cell text, …) are interned into an internal arena for the duration of the call.
- **Colors / enums.** `Color::{NONE, RED, …, rgb(r,g,b)}`, `Style::bold()`, `Attr`, `Align`, `VAlign`, `BorderType`, `Position`, `ListMarker`, `ProgressType`, `SpinnerType`, `AlertType`, `WeekStart`, `HintLayout`, `HintPos`.
- **Named RGB palette.** The curated `SC_COLOR_*` set as `pub const Color`s under `sparcli::palette` — `palette::ACCENT`, `palette::ERROR`, `palette::ORANGE`, `palette::RED_VIVID`, `palette::BG_DARKEN_1`, … (all 53; additional to the eight ANSI `Color` constants). Also usable in markup as `[accent]`, `[error]`, … Example: `BorderStyle::new(BorderType::Rounded).color(palette::ACCENT)`. **Runtime override:** `palette::set("accent", color)` / `palette::get("accent")` / `palette::reset()` recolor a name at runtime — honored by markup, the CLI and palette-name widget defaults (e.g. the fuzzy accent). Set once before spawning threads.
- **Prompts** return `Result<Option<T>>`: `Ok(Some(v))` = value, `Ok(None)` = cancelled (Esc/Ctrl-C), `Err(Error::Unavailable)` = no TTY / read error. The env var `SPARCLI_NO_TTY=1` forces the no-TTY error even with a terminal attached (used by `make rust-test`).
- **Escape hatch.** The raw FFI is re-exported as `sparcli::sys`.

## Output

```rust
use sparcli::*;

println("plain", Style::default());
print("bold red", Style::bold().fg(Color::RED));

panel("Hello", PanelOpts::new().rounded().title("greeting").full_width());
rule(RuleOpts::new().kind(BorderType::Double).title("section"));
badge("NEW", BadgeOpts::new().style(Style::bold().bg(Color::GREEN)));
alert::success("done");

// Color by name (ANSI or palette names; same resolver as markup `[accent]`):
let accent = Color::by_name("accent").unwrap();   // None for unknown names

// Rich-text variants of the str widgets: alert::text(kind, &t), rule_text(&t,
// opts), pad_text(&t, opts), align_text(&t, align, width); Text::append_badge.

let mut t = Table::new();
t.column("Name", ColOpts::new().style(Style::bold().fg(Color::CYAN)))   // per-column style
    .column("Age", ColOpts::new().align(Align::Right));
t.row(["Ada", "36"]).row(["Alan", "41"]);
t.footer_row(["", "2 people"]);   // footer section
t.print(TableOpts {                // TableHeader / TableFooter / TableBorder
    header: TableHeader { row: true, ..Default::default() },
    footer: TableFooter { style: Style::dim(), ..Default::default() },
    ..Default::default()
});

let mut list = List::new(ListOpts::new().marker(ListMarker::Number));
let item = list.add("top", Style::default());
item.sub(ListOpts::new()).add("nested", Style::default());
list.print();

let mut kv = Kv::new(KvOpts::new().key_style(Style::bold()));
kv.add("Version", version_string());
kv.print();
```

### Rich text & markup

```rust
let mut t = Text::new();
t.append("status: ", Style::default())
 .append("OK", Style::bold().fg(Color::GREEN))
 .append_link("docs", "https://example.com/docs", Style::default());
t.print();

markup::println("[bold red]Error:[/] file not found");
markup::println("Open [link=https://example.com]the docs[/link]");
markup::println("run `make qa` first");            // backtick code span (magenta)
let parsed: Text = Text::markup("[italic]hi[/]");

// Custom inline-code style instead of the default magenta:
let t = markup::parse_opts("see `code`", MarkupOpts {
    code_style: Style::bold().fg(Color::CYAN),
    ..Default::default()
});
```

`append_link` and the `[link=URL]text[/link]` markup tag emit OSC-8 terminal hyperlinks (clickable in supporting terminals, plain text elsewhere); the escape bytes have zero visible width.

Backtick `` `inline code` `` spans render in magenta with the backticks removed; the body is literal (tags are not parsed inside). Escape a literal backtick with `` \` ``. `MarkupOpts::code_style` overrides the default style.

### Capture / compose

```rust
let r = capture::panel("hi", PanelOpts::new().single());
r.pad(PadOpts { left: 4, ..Default::default() });
r.align(Align::Center, 0);
let lines: Vec<String> = r.lines();         // ANSI-included
let stacked = vstack(&[&r, &r], 1).unwrap();

// Rich-text / re-framing variants: capture::panel_text, capture::rule_text,
// and capture::panel_rendered (frame an already-captured rendering in a panel).
let framed = capture::panel_rendered(&r, PanelOpts::new().single());

let mut cols = Columns::new(ColumnsOpts::new().gap(3));
cols.add_rendered(&r, ColItem::new()).add_str("text", ColItem::new());
cols.print();

// Capture print-style calls that have no capture::* counterpart (error
// reports, live sessions, plain prints): redirects the thread-local C
// output stream into a buffer for the duration of the closure.
let text = capture::output(|| report.print());
assert!(text.contains("boom"));
```

### Progress & spinner

```rust
let mut bar = ProgressBar::new(ProgressBarOpts::new().brackets().show_percent(true));
bar.set_label("Installing");
for v in 0..=100 { bar.draw(v as f64, 100.0); }
bar.finish(100.0, 100.0);

// Threshold coloring: green below mid, yellow up to high, red above.
let bar = ProgressBar::new(ProgressBarOpts::new().thresholds(
    ProgressThresholds::new(Color::GREEN, Color::YELLOW, Color::RED).ratios(0.5, 0.8)));

let mut sp = Spinner::new("Loading", SpinnerOpts::new());
sp.tick();
sp.finish(true, "Done");
```

### Multi-progress, diff & humanize

```rust
// Several bars updated together in place (RAII; ends on drop or .end()).
let mut mp = MultiProgress::begin(MultiProgressOpts::new());
let a = mp.add("download", ProgressBarOpts::new().show_percent(true));
let b = mp.add("extract",  ProgressBarOpts::new().show_percent(true));
mp.update(a, 100.0, 100.0);
mp.update(b, 50.0, 100.0);
mp.end();

// Colored unified diff: print, build a Text, or capture a Rendered.
diff_print(old, new, DiffOpts::new().context(1).old_label("a").new_label("b"));
let t = diff_text(old, new, DiffOpts::new());
let r = capture::diff(old, new, DiffOpts::new().no_header(true));

// Human-readable formatting (module `sparcli::humanize`).
use sparcli::humanize::{self, ByteUnit, HumanizeOpts};
humanize::bytes(1536, ByteUnit::Si);   // "1.5 KB"
humanize::number(1_234_567.0, HumanizeOpts::new());   // "1,234,567"
humanize::duration(8054.0);            // "2h 14m"
humanize::relative(then, now);         // "3 hours ago"
// de_DE: HumanizeOpts::new().decimals(2).decimal_sep(',').group_sep('.')
```

### ANSI-injection protection

User strings are sanitized by default (control bytes and escape sequences removed). Opt out globally or per widget:

```rust
sparcli::set_allow_ansi(true);              // sparcli::allow_ansi() reads it
panel("\x1b[31mred\x1b[0m", PanelOpts::new().single().ansi(AnsiMode::Allow));
// AnsiMode::Default (inherit global) / Allow / Sanitize
```

### Application helpers (XDG paths, pager)

```rust
// XDG directories (created on first use); None on failure
let cfg = sparcli::paths::config("myapp");                  // ~/.config/myapp
let log = sparcli::paths::file(sparcli::paths::Kind::State, "myapp", "run.log");

// Pager: output is piped through $PAGER / less -R until end()/drop;
// no-op when the output stream is not a terminal (scripts, pipes, CI)
let pager = sparcli::Pager::begin(sparcli::PagerOpts::new());
markup::println("[bold]long report…[/]");
let status = pager.end();

// Live display: re-render a composed frame in place (dashboard).
// Off-terminal, only the final frame is printed when the session ends.
// .prompt_rows(n) reserves rows below the frame for an interactive prompt
// (REPL dashboards) - the cursor parks there after every update.
let live = sparcli::Live::begin(sparcli::LiveOpts::new());
for percent in (0..=100).step_by(10) {
    let frame = sparcli::capture::str(&format!("progress: {percent}%"));
    live.update(&frame);   // overwrites the previous frame in place
}
live.end();   // or implicit on drop

// Alt-screen session for full-screen widgets (RAII: ends on drop, no flicker
// on switch). Fuzzy/Form take .fullscreen()/.valign()/.header(&Rendered) - the
// borrowed header must outlive the run; the finder grows then scrolls.
{
    let _screen = sparcli::AltScreen::begin();
    let header = sparcli::capture::str("My App");
    let mut f = sparcli::Fuzzy::new(
        sparcli::FuzzyOpts::new()
            .fullscreen()
            .valign(sparcli::VAlign::Middle)
            .header(&header),
    );
    f.add("alpha").add("beta");
    let _ = f.run();   // or a Form with the same options
}   // drop restores the previous screen

// Pretty errors: message + causes + hint + exit code as a red panel
sparcli::ErrorReport::new("Config could not be loaded")
    .cause("file not found: ~/.config/app/config.toml")
    .hint("Run 'app init' to create a default config")
    .code(2)
    .die(); // renders to stderr, exits(2); print()/print_stderr() don't exit

// Logging: global logger (colored stderr at INFO + optional file sinks)
sparcli::log::set_level(sparcli::LogLevel::Debug);
sparcli::log::add_file("app.log", sparcli::LogLevel::Debug);
sparcli::log::info("server started");      // message is data, never a format

// ... or an independent handle-based logger with its own sinks: colored
// terminal streams (.add_stderr/.add_stdout/.add_terminal(fd, …); the fd is
// duplicated and closed on drop) and/or plain-text files (closed on drop)
let logger = sparcli::Logger::new(sparcli::LoggerOpts::new().hide_timestamps())
    .add_stderr(sparcli::LogLevel::Info)
    .add_file("subsystem.log", sparcli::LogLevel::Debug);
logger.warn("low disk space");

// One-shot pretty error + exit (message + optional hint), no builder needed:
// sparcli::die_msg(2, "config not found", Some("run 'app init'"));
```

The `Live` session also has `.update_text(&text)` and `.update_table(&table, opts)` (rich-text / table frames captured in one call) alongside `.update(&rendered)` / `.update_str(…)`.

## Input

```rust
use sparcli::*;

if let Some(yes) = confirm("Proceed?", ConfirmOpts::new().default_yes(true))? { /* … */ }
if let Some(name) = text_input("Name", TextInputOpts::new().placeholder("Ada"))? { /* … */ }

// Validation: Ok(()) accepts, Err(msg) keeps the prompt open and shows msg
// beneath the field (text_input + password_input).
if let Some(user) = text_input("User", TextInputOpts::new().validate(|v| {
    if v.trim().is_empty() { Err("must not be empty".into()) } else { Ok(()) }
}))? { /* … */ }

// Autocomplete dropdown: suggestions as a navigable list below the field
// (arrows move, Tab/Enter accept; prefix or fuzzy matching).
if let Some(cmd) = text_input("Git command", TextInputOpts::new()
    .suggestions(vec!["commit".into(), "checkout".into(), "cherry-pick".into()])
    .suggest(SuggestOpts::dropdown()
        .fuzzy()
        .border(BorderStyle::new(BorderType::Rounded))
        .selected_style(Style::new().fg(Color::BLACK).bg(Color::CYAN))))? { /* … */ }
if let Some(pw)   = password_input("Password", PasswordOpts::new())? { /* … */ }
if let Some(n)    = number_input("Qty", NumberOpts::new().range(0.0, 100.0).step(5.0))? { /* … */ }
if let Some(text) = textarea("Notes", TextareaOpts::new().boxed(48))? { /* … */ }

// Exact value as text ('.'-normalized, never via f64) – e.g. for money.
// decimal_sep(',') shows/accepts a comma while editing; start_empty(true)
// starts with an empty field instead of "0,00" (Enter on empty is ignored).
if let Some(amount) = number_input_text(
    "Amount", NumberOpts::new().decimals(2).decimal_sep(',').start_empty(true),
)? { /* amount == "12.99" – parse into rust_decimal etc. */ }

// Calculator mode: "=" starts an expression (=1,5+2*3); Enter accepts the
// result, a second Enter submits it (full precision by default). A dim
// " = <exact>" indicator marks a pending full-precision result; editing it
// away shows a yellow "exact result discarded" warning automatically.
if let Some(total) = number_input_text(
    "Total", NumberOpts::new().decimals(2).calculator(true).start_empty(true),
)? { /* "=12,99*3" → total == "38.97" */ }

// The evaluator is also available as a pure function:
assert_eq!(sparcli::calc_eval("1,5+2*3"), Some(7.5));

// Select/Fuzzy take `.accent`, `.selected_style` (cursor row; a bg fills the
// full row width as a highlight bar) and `.box_style` (border / background /
// width mode); `box_style` also works borderless (just a background fill).
let mut sel = Select::new(
    SelectOpts::new()
        .prompt("Pick")
        .selected_style(Style::bold().fg(Color::WHITE).bg(Color::MAGENTA))
        .box_style(BoxStyle::default().bg(Color::rgb(30, 30, 46)).width_content(28, 0)));
sel.add("a").add("b").add("c");
if let Some(indices) = sel.run()? { /* Vec<usize> */ }

let mut fz = Fuzzy::new(FuzzyOpts::new().prompt("Find"));
fz.add("Tokyo").add("London");
if let Some(i) = fz.run()? { /* add-order index */ }

// Fuzzy table view: column headers + multi-column rows; the query searches the
// columns selected by .search_columns(mask) (0 = all) and highlights matches.
let mut grid = Fuzzy::new(FuzzyOpts::new()
    .prompt("Pick")
    .table(["Name", "Role"])
    .table_opts(TableOpts::new().header_row(true)));
grid.add_row(["Ada", "Engineer"]).add_row(["Alan", "Founder"]);
if let Some(i) = grid.run()? { /* add-order index */ }

// Todo-style finder: day sections, multi-select, per-cell colors, ordering.
let mut todo = Fuzzy::new(FuzzyOpts::new()
    .table(["Time", "Task", "Status"])
    .multi().checkbox_column().section_counts()
    .order(FuzzyOrder::Column(0))          // chronological per day
    .toggle_all_key(key_ctrl('a')));
todo.add_section("Monday");
todo.add_row_styled(["09:00", "Pay invoice", "overdue"],
                    &[Style::new(), Style::new(), Style::new().fg(Color::RED)]);
todo.set_id(1, 102);                        // stable id (survives remove/reorder)
if let Some(checked) = todo.run_multi()? { /* Vec<usize> of checked rows */ }
// also: add_section / add_styled / add_row_rich, set_disabled, set_checked /
// check_all / checked_count, set_cursor / set_label / set_row / set_row_style,
// id_at / cursor_id. Full demo: examples/c/apps/todo_fuzzy.c.

// Modal (vim-style) mode: normal mode fires bare-letter shortcuts + j/k/g/G,
// insert mode (press `i`) types a filter, `Esc` toggles back; the query line is
// badged + tinted per mode. Build bare-char chords with key_char.
let mut modal = Fuzzy::new(FuzzyOpts::new()
    .modal()                                   // .start_in_insert() to flip
    .mode_labels("CMD", "EDIT")                // badge text per mode
    .clear_key(key_char('c')));                // normal-mode: clear the query

// Process-wide defaults every widget inherits (per-call opts still win):
Theme::new().accent(Color::MAGENTA).marker("➜ ").apply();   // reset_theme() clears


// Input history (REPLs): Up/Down recall + auto-add on submit + persistence.
// Dropping the handle saves the configured file.
let history = History::new(HistoryOpts::new().app("myapp"));
loop {
    match text_input("repl>", TextInputOpts::new().history(&history)) {
        Ok(Some(line)) => dispatch(&line),   // already recorded in the history
        _ => break,                          // cancelled or no terminal
    }
}
// history.add/count/get/save/load for manual control;
// .no_history_add() opts a single prompt out of the automatic recording

if let Some(d) = datepicker(None, DatePickerOpts::new().prompt("Date"))? {
    println!("{:04}-{:02}-{:02}", d.year, d.month, d.day);
}

let pure = fuzzy_match("ab", "cab");          // (bool, score), no TTY
```

### Form (grid layout)

`Form` builds a grid of fields; `run()` then the `get_*` getters. `FieldOpts`/
`FormOpts` are plain builder structs; dates use `Date`. (The per-field
`validate` callback is not exposed.)

```rust
use sparcli::{Form, FormOpts, FieldOpts, FieldWidthMode};
let mut f = Form::new(FormOpts { title: Some("Contact".into()), ..Default::default() });
f.row_begin();
let name = f.add_text("Name", "Ada",
    FieldOpts { width_mode: FieldWidthMode::Pct, width: 50, ..Default::default() });
let tier = f.add_select("Tier", &["Bronze", "Silver", "Gold"], 1, FieldOpts::default());
f.add_multiselect("Tags", &["vip", "net-30"], &[0], FieldOpts::default());
f.add_date("Since", None, FieldOpts::default());
f.add_text("Notes", "", FieldOpts { multiline: true, ..Default::default() });
if f.run()? {
    let _who  = f.get_string(name);
    let _t    = f.get_choice(tier);
    let _tags = f.get_checked(2);
    if let Some(d) = f.get_date(3) { let _ = d; }   // sparcli::Date
}
```

### Custom shortcuts

Bind extra keys (Ctrl-letter / F1–F12 / Alt) on any widget. RETURN-mode ends the prompt and reports an id via `fired()`; CALLBACK-mode runs a closure and keeps the prompt open unless it returns `false`.

```rust
let sc = Shortcuts::new()
    .on_return(key_fn(2), 1, Some("help"))
    .on_callback(key_ctrl('r'), Some("reload"), || { /* … */ true });

let yes = confirm("Proceed?", ConfirmOpts::new().shortcuts(&sc))?;
if sc.fired() == 1 { /* F2 pressed */ }
```

Besides `key_ctrl`/`key_fn`/`key_alt`, the named keys `key_left/right/up/down/enter/tab()` build chords for arrows/Enter/Tab - e.g. for Left = back / Right = forward navigation.

For driving the decode loop yourself, `decode_key(&bytes) -> (Key, usize)` is the pure key decoder (`Key::kind()` is a `KeyType`, `.char_value()`, `.is_ctrl/alt/pasted()`); `Chord::matches(&key)` and `Shortcuts::find(&key)` mirror the prompt engine's own dispatch.

Live editing of `Select`/`Fuzzy` from a callback: `select.cursor()`, `select.label(i)`, `select.set_label(i, "…")`, `select.remove(i)`, `fuzzy.cursor_index()`, `fuzzy.remove(i)`; `fuzzy.has_selection()` reports whether a row currently matches (so a forward/submit shortcut can avoid acting on an empty filter).

### Rich prompts & external editor

```rust
// Only part of the prompt styled:
let mut p = Text::new();
p.append("Rename ", Style::default())
 .append("Apple", Style::italic())
 .append(" to", Style::default());
let renamed = text_input("", TextInputOpts::new().prompt_text(&p))?;
// or markup:  TextInputOpts::new().prompt_markup(true)  with "Rename [italic]Apple[/] to"

// Open $EDITOR (default chain → nvim) with Ctrl-G:
let notes = textarea("Notes", TextareaOpts::new().external_editor(true))?;
```

## Build & test

The workspace has no binary, so plain `cargo run` fails – run an **example** (name the package too, since it is a workspace):

```sh
make rust          # cargo build (compiles the C via cc)
make rust-test     # cargo test (non-interactive + doctests)

# from bindings/rust/ : examples are grouped by area (group_file), e.g.
cargo run -p sparcli --example output_table_basic   # an output example
cargo run -p sparcli --example input_fuzzy          # an input example (needs a terminal)
cargo run -p sparcli --example app_paths            # an app-helper example
```

The full list of examples, in every language, is in [`docs/examples.md`](examples.md); the Rust examples are registered in `bindings/rust/sparcli/Cargo.toml`. They can also be run from the repo root with `make run-example EX=rust/output/table_basic`.

> Publishing to crates.io would vendor the C sources into `sparcli-sys` (currently referenced from the repo root by `build.rs`).
