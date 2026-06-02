# sparcli – Rust API Reference

Safe, idiomatic Rust bindings for sparcli, in the `bindings/rust/` cargo workspace:

- **`sparcli-sys`** – raw FFI. `build.rs` compiles the C sources with the `cc` crate, so a plain `cargo build` needs only a Rust toolchain (no prior `make`, no system install). Bindings are committed (`src/bindings.rs`); build with `--features regen` to regenerate them with bindgen (needs libclang).
- **`sparcli`** – the safe wrapper: RAII handles, builder-style option structs, `Result<Option<T>>` prompts, closures for callbacks.

```toml
[dependencies]
sparcli = { path = "bindings/rust/sparcli" }
```

## Design

- **RAII handles** (`Text`, `Table`, `List`, `Tree`, `Kv`, `Columns`, `Rendered`, `ProgressBar`, `Spinner`, `Select`, `Fuzzy`) free themselves on drop. They hold raw pointers and are therefore **not `Send`/`Sync`**: the C output target is thread-local and the input session is process-global, so build and use a handle on one thread (each thread may build its own).
- **Builders.** Every `*Opts` is a plain struct with `Default` and chainable setters; public fields give full access. Borrowed strings (`Title`, cell text, …) are interned into an internal arena for the duration of the call.
- **Colors / enums.** `Color::{NONE, RED, …, rgb(r,g,b)}`, `Style::bold()`, `Attr`, `Align`, `VAlign`, `BorderType`, `Position`, `ListMarker`, `ProgressType`, `SpinnerType`, `AlertType`, `WeekStart`, `HintLayout`, `HintPos`.
- **Prompts** return `Result<Option<T>>`: `Ok(Some(v))` = value, `Ok(None)` = cancelled (Esc/Ctrl-C), `Err(Error::Unavailable)` = no TTY / read error.
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

let mut t = Table::new();
t.column("Name", ColOpts::new().style(Style::bold().fg(Color::CYAN)))   // per-column style
    .column("Age", ColOpts::new().align(Align::Right));
t.row(["Ada", "36"]).row(["Alan", "41"]);
t.print(TableOpts::new().border(BorderType::Rounded).header_row(true).striped(true));

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
 .append("OK", Style::bold().fg(Color::GREEN));
t.print();

markup::println("[bold red]Error:[/] file not found");
let parsed: Text = Text::markup("[italic]hi[/]");
```

### Capture / compose

```rust
let r = capture::panel("hi", PanelOpts::new().single());
r.pad(PadOpts { left: 4, ..Default::default() });
r.align(Align::Center, 0);
let lines: Vec<String> = r.lines();         // ANSI-included
let stacked = vstack(&[&r, &r], 1).unwrap();

let mut cols = Columns::new(ColumnsOpts::new().gap(3));
cols.add_rendered(&r, ColItem::new()).add_str("text", ColItem::new());
cols.print();
```

### Progress & spinner

```rust
let mut bar = ProgressBar::new(ProgressBarOpts::new().brackets().show_percent(true));
bar.set_label("Installing");
for v in 0..=100 { bar.draw(v as f64, 100.0); }
bar.finish(100.0, 100.0);

let mut sp = Spinner::new("Loading", SpinnerOpts::new());
sp.tick();
sp.finish(true, "Done");
```

### ANSI-injection protection

User strings are sanitized by default (control bytes and escape sequences removed). Opt out globally or per widget:

```rust
sparcli::set_allow_ansi(true);              // sparcli::allow_ansi() reads it
panel("\x1b[31mred\x1b[0m", PanelOpts::new().single().ansi(AnsiMode::Allow));
// AnsiMode::Default (inherit global) / Allow / Sanitize
```

## Input

```rust
use sparcli::*;

if let Some(yes) = confirm("Proceed?", ConfirmOpts::new().default_yes(true))? { /* … */ }
if let Some(name) = text_input("Name", TextInputOpts::new().placeholder("Ada"))? { /* … */ }
if let Some(pw)   = password_input("Password", PasswordOpts::new())? { /* … */ }
if let Some(n)    = number_input("Qty", NumberOpts::new().range(0.0, 100.0).step(5.0))? { /* … */ }
if let Some(text) = textarea("Notes", TextareaOpts::new().boxed(48))? { /* … */ }

// Exact value as text ('.'-normalized, never via f64) – e.g. for money.
// decimal_sep(',') shows/accepts a comma while editing; start_empty(true)
// starts with an empty field instead of "0,00" (Enter on empty is ignored).
if let Some(amount) = number_input_text(
    "Amount", NumberOpts::new().decimals(2).decimal_sep(',').start_empty(true),
)? { /* amount == "12.99" – parse into rust_decimal etc. */ }

let mut sel = Select::new(SelectOpts::new().prompt("Pick").multi(true));
sel.add("a").add("b").add("c");
if let Some(indices) = sel.run()? { /* Vec<usize> */ }

let mut fz = Fuzzy::new(FuzzyOpts::new().prompt("Find"));
fz.add("Tokyo").add("London");
if let Some(i) = fz.run()? { /* add-order index */ }

if let Some(d) = datepicker(None, DatePickerOpts::new().prompt("Date"))? {
    println!("{:04}-{:02}-{:02}", d.year, d.month, d.day);
}

let pure = fuzzy_match("ab", "cab");          // (bool, score), no TTY
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

Live editing of `Select`/`Fuzzy` from a callback: `select.cursor()`, `select.label(i)`, `select.set_label(i, "…")`, `select.remove(i)`, `fuzzy.cursor_index()`, `fuzzy.remove(i)`.

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

# from bindings/rust/ :
cargo run -p sparcli --example demo            # complete showcase (all widgets)
cargo run -p sparcli --example output_gallery  # output only
cargo run -p sparcli --example input_demo      # input only (needs a terminal)
```

`demo` shows every output component and, in a real terminal, every input widget (it auto-skips the input section when piped / no TTY).

> Publishing to crates.io would vendor the C sources into `sparcli-sys` (currently referenced from the repo root by `build.rs`).
