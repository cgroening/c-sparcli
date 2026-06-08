# sparcli – Polished CLI output & prompts

A C11 library for **styled terminal output**, **interactive prompts** and **CLI applications**:

- panels, tables, rules, columns, lists, trees, key/value blocks, alerts, badges, progress bars, spinners and live-updating dashboards;
- confirm, text, password, number, textarea, select, fuzzy, date-picker prompts and a grid-layout form – every one extensible with custom keyboard shortcuts;
- an application framework: argument parser ("clap for C"), logging, pretty errors, XDG paths, pager integration and REPL building blocks.

Ships with **Rich-compatible inline markup**, a header-only **C++ wrapper**, safe, idiomatic **Rust** and **Python** bindings, and a **`sparcli` command-line tool** that exposes everything to the shell (zsh/bash).

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg) ![Language: C11](https://img.shields.io/badge/c-11-blue.svg) ![Version: 0.1.0](https://img.shields.io/badge/version-0.1.0-orange.svg)

> ⚠️ **Status:** sparcli is under active development. The API is still settling – expect breaking changes between 0.x versions.

![sparcli hero](docs/images/output_hero.png)

---

![sparcli hero](docs/images/input_hero.png)

---

## Table of contents

- [sparcli – Polished CLI output \& prompts](#sparcli--polished-cli-output--prompts)
  - [Table of contents](#table-of-contents)
  - [Features](#features)
    - [Output](#output)
    - [Input](#input)
    - [CLI application framework](#cli-application-framework)
    - [Shell \& language bindings](#shell--language-bindings)
    - [Robustness \& engineering](#robustness--engineering)
  - [Quick start](#quick-start)
    - [C](#c)
    - [C++ (header-only)](#c-header-only)
    - [Rust](#rust)
    - [Python](#python)
  - [Installation](#installation)
    - [From source](#from-source)
    - [Linking](#linking)
    - [Requirements](#requirements)
  - [Output widgets](#output-widgets)
  - [Input widgets](#input-widgets)
  - [Application framework](#application-framework)
  - [Command-line tool](#command-line-tool)
  - [Rich-compatible markup](#rich-compatible-markup)
  - [Development](#development)
  - [Roadmap](#roadmap)
  - [Why sparcli?](#why-sparcli)
  - [License](#license)

---

## Features

### Output

The full reference for every output component lives in [`docs/api-c.md`](docs/api-c.md); a per-widget overview is in [Output widgets](#output-widgets).

- **Large set of widgets**: panels, tables, rules, side-by-side columns, lists, trees, key/value blocks, alerts, badges, progress bars, spinners.
- **Live display & dashboards**: re-render a composed frame in place (`sc_live_*`) – build dashboards from any widgets that update continuously, in-place or on the fullscreen alternate screen. A thin alt-screen session (`sc_altscreen_begin`/`_end`) hosts **full-screen widgets**: the fuzzy finder and the form run with a pinned header and vertical alignment, growing to fill the screen then scrolling ([docs](docs/api-c.md#live-display)).
- **Multi-progress**: several progress bars updated together in place for concurrent tasks (`sc_multiprogress_*`), buffered to the final stack off a terminal. *(C, C++, Rust, Python)*
- **Diff rendering**: colored unified diff of two texts (`sc_diff_*`) – `@@` hunks, red/green `-`/`+` lines. *(C, C++, Rust, Python)*
- **Human-readable formatting**: file sizes, durations, relative time, grouped/compact numbers and percentages (`sc_humanize_*`, e.g. `1536 → "1.5 KB"`, `93 → "1m 33s"`), locale-aware separators. *(C, C++, Rust, Python)*
- **Rich-compatible markup**: `[bold red]error[/]`, `[on cyan] OK [/]`, `[rgb(120,200,255)]…[/]` – same syntax as [Rich](https://github.com/Textualize/rich)/[Textual](https://github.com/Textualize/textual). See [Rich-compatible markup](#rich-compatible-markup).
- **Clickable hyperlinks (OSC-8)**: `[link=https://…]text[/link]` markup or `sc_text_append_link()` – Cmd/Ctrl+click opens the URL in supporting terminals, plain text everywhere else ([docs](docs/api-c.md#hyperlinks-osc-8)).
- **Truecolor + 8-color ANSI**, with graceful sentinels for "no color". Plus a curated **named RGB palette** (`SC_COLOR_ACCENT`, `SC_COLOR_ERROR`, … / `sparcli::palette::accent()` / `palette::ACCENT` / `sc.Palette.ACCENT`) usable in code and in markup (`[accent]`, `[error]`, `[orange]`) and the CLI (`--color accent`) ([docs](docs/api-c.md#named-rgb-palette-sc_color_)).
- **Composable**: capture any widget into a buffer, then pad, align, stack (`sc_vstack`), or place it inside a columns layout ([docs](docs/api-c.md#capture-api)).

### Input

The full reference for every input widget lives in [`docs/api-c.md`](docs/api-c.md#input-widgets); a per-widget overview is in [Input widgets](#input-widgets).

- **Interactive prompts**: confirm, text/password, number, textarea, single & multi select, fuzzy finder, a date picker, and a grid-layout **form** (framed fields with widths/spans, 2D navigation, multiline fields edited in `$EDITOR`) – each with a non-TTY fallback.
- **Custom keyboard shortcuts** on every prompt (Ctrl-letter / F1–F12 / Alt) bound to return-an-action or live callbacks, plus **rich prompts** (mix styles, e.g. a partly-italic label) ([docs](docs/api-c.md#custom-shortcuts)).
- **Input history**: ↑/↓ recall of previous entries in the text input, with optional persistence in the XDG state directory (`sc_history_*`).

### CLI application framework

Everything a complete command-line application needs around the widgets; overview in [Application framework](#application-framework), full reference in [`docs/api-framework.md`](docs/api-framework.md).

- **Argument parser** – *clap for C*: declarative subcommands, typed options, auto-generated `--help` (rendered with sparcli panels and tables), "did you mean ...?" suggestions and zsh completion generation (`sc_args_*`, [docs](docs/api-framework.md#argument-parser)).
- **Logging**: leveled, colored terminal logging (`sc_log_info("port %d", p)`) with timestamps and `file:line`, plus plain-text file sinks with their own levels – thread-safe, render-once architecture ([docs](docs/api-framework.md#logging)).
- **Pretty errors**: report fatal errors as red alert panels – message + cause chain + hint + exit code (`sc_die`, [docs](docs/api-framework.md#pretty-errors)).
- **Pager integration**: pipe long output through `$PAGER` / `less -R` automatically; a no-op in scripts and CI ([docs](docs/api-c.md#pager)).
- **XDG paths**: resolve per-app config/data/cache/state directories (`sc_path_config("myapp")` → `~/.config/myapp`, created on first use) ([docs](docs/api-framework.md#xdg-paths)).
- **Subprocess helper**: run a command without a shell and capture stdout/stderr/exit code without deadlocking (`sc_run`); captured output is sanitized by default.
- **Layered config**: merge defaults < config file (JSON/TOML/YAML by extension) < environment < flags into one tree with dotted-path typed getters (`sc_config_*`); opt-in (serde-backed).
- **Shell completion**: generate zsh, **bash and fish** completion scripts from an args command tree (`sc_args_print_{zsh,bash,fish}_completion`).
- **REPL building blocks**: input history with ↑/↓ recall and XDG persistence (`sc_history_*`), a quote-aware line tokenizer (`sc_args_split`), reusable parse trees (implicit `sc_args_reset`), and live-dashboard + prompt composition (`ScLiveOpts.prompt_rows`) – see the `examples/c/apps/repl_*.c` demos.

### Structured data (serde)

An opt-in `serde/` layer of read/write parsers over one shared `ScValue` tree (C and C++ only); full reference in [`docs/api-serde.md`](docs/api-serde.md). Included via `<serde/sparcli_serde.h>` / `.hpp`, **not** by `<sparcli.h>`, and gated separately (`make qa-serde`).

- **JSON, TOML, YAML** (documented subset) – read and write over the shared model, so converting between them is parse-one/write-the-other (`sc_json_*`, `sc_toml_*`, `sc_yaml_*`).
- **CSV/TSV** – RFC-4180 reader+writer with quoted fields, ragged rows and header lookup (`sc_csv_*`); the same parser the `sparcli table` CLI uses.
- **Markdown** – front-matter split (YAML/TOML → `ScValue`) plus an editable heading/section tree (`sc_markdown_*`).
- **Render to the terminal** (view layer): pretty-print any `ScValue` jq-style with syntax coloring (`sc_value_render`), and render Markdown through the widget stack – headings, lists, code blocks, quotes, pipe tables, inline emphasis (`sc_markdown_render`). C and C++ only, opt-in.

### Shell & language bindings

- **Command-line tool included**: the `sparcli` binary brings every output and input widget to the shell – `sparcli panel`, `name=$(sparcli input "Name:")`, `sparcli confirm && …`. See [Command-line tool](#command-line-tool) and [`docs/cli.md`](docs/cli.md).
- **C++ wrapper included**: a header-only RAII C++20 layer (`<sparcli.hpp>`, namespace `sparcli`) – no manual `free`, owned strings, `std::optional` inputs. See [`docs/api-cpp.md`](docs/api-cpp.md).
- **Rust bindings included**: a safe, idiomatic crate (`bindings/rust/`, builds the C via `cc` – no install needed) with RAII handles, builder options and `Result<Option<T>>` prompts. See [`docs/api-rust.md`](docs/api-rust.md).
- **Python bindings included**: a safe, Pythonic package (`bindings/python/`, a cffi wrapper that compiles the C – no install needed) with RAII handles, `@dataclass` options and `value`/`None` prompts. See [`docs/api-python.md`](docs/api-python.md).

### Robustness & engineering

- **UTF-8 & ANSI-aware** width math everywhere (codepoints, not bytes).
- **ANSI-injection safe by default**: control bytes and escape sequences in user strings are stripped at the API boundary; opt back in globally (`sc_set_allow_ansi`) or per widget (`.ansi = SC_ANSI_MODE_ALLOW`) with widths staying correct.
- **FFI-ready**: `extern "C"`, hidden symbol visibility, opaque types, NULL-safe entry points – the C++, Rust and Python wrappers build on this.
- **No runtime dependencies** beyond libc.
- **Static + shared library**, `pkg-config` file, optional ASan/UBSan build.

---

## Quick start

### C

```c
#include <sparcli.h>

int main(void) {
    sc_markup_println("[bold green]Hello[/], [italic]sparcli[/]!");

    ScPanelOpts opts = {
        .border  = { .type = SC_BORDER_ROUNDED },
        .title   = { .text = " Greeting ", .halign = SC_ALIGN_CENTER },
        .padding = { 0, 2, 0, 2 },
    };
    sc_panel_str("Welcome aboard.", opts);
    return 0;
}
```

```sh
cc hello.c $(pkg-config --cflags --libs sparcli) -o hello && ./hello
```

![hello.c output](docs/images/output_quick_start.png)

### C++ (header-only)

A header-only C++20 wrapper ships in [`include/sparcli.hpp`](include/sparcli.hpp): RAII handles (no manual `free`), owned strings where the C API borrows (so temporaries are safe), and `std::optional` returns for input prompts. Full reference: [`docs/api-cpp.md`](docs/api-cpp.md).

```cpp
#include <sparcli.hpp>
using namespace sparcli;

int main() {
    panel("Welcome aboard.", { .border = { .type = SC_BORDER_ROUNDED },
                               .title  = { .text = " Greeting ",
                                           .halign = SC_ALIGN_CENTER } });
    Table t;                                   // frees itself
    t.add_column("Name");
    t.add_row({ "Ada", std::to_string(42) });  // strings are owned → safe
    t.print({ .header = { .row = true } });

    if (auto name = text_input("Your name"))   // std::optional<std::string>
        markup::println("[green]Hi[/] " + *name);
}
```

```sh
c++ -std=c++20 hello.cpp $(pkg-config --cflags --libs sparcli) -o hello
```

**Why the wrapper?** Two easy C-API mistakes simply can't happen with it:

```cpp
// C API – two footguns:
ScTableData *t = sc_table_new();                      // 1) leaks if you forget
                                                      //    sc_table_free(t)
sc_table_add_row(t, (ScCell[]){                       // 2) the table BORROWS the
    sc_cell(std::to_string(n).c_str()) }, 1);         //    string; the temporary
                                                      //    dies here → dangling
sc_table_print(t, (ScTableOpts){0});                  //    pointer read → garbage

// C++ wrapper – RAII frees, and the cell string is copied into the table:
sparcli::Table t;                                     // frees itself
t.add_row({ std::to_string(n) });                     // owned → temporary is safe
t.print();
```

These guarantees (auto-free, owned cell strings, surviving a move) are verified by [`tests/cpp/test_cpp.cpp`](tests/cpp/test_cpp.cpp).

### Rust

Safe, idiomatic bindings live in [`bindings/rust/`](bindings/rust/) (a cargo workspace). `sparcli-sys` compiles the C with the `cc` crate, so a plain `cargo build` needs only a Rust toolchain – no prior `make` or install. RAII handles free themselves, `*Opts` use builder methods, callbacks are closures, and prompts return `Result<Option<T>>` (`Ok(None)` = cancelled). Full reference: [`docs/api-rust.md`](docs/api-rust.md).

```rust
use sparcli::*;

fn main() -> sparcli::Result<()> {
    panel("Welcome aboard.", PanelOpts::new().rounded().title("Greeting"));

    let mut t = Table::new();                  // frees itself
    t.column("Name", ColOpts::new());
    t.row(["Ada", "42"]);                      // strings owned → temporaries safe
    t.print(TableOpts::new().header_row(true));

    if let Some(name) = text_input("Your name", TextInputOpts::new())? {
        markup::println(&format!("[green]Hi[/] {name}"));
    }
    Ok(())
}
```

```sh
# the workspace has no bin, so run an example (from bindings/rust/):
cargo run -p sparcli --example output_table_basic   # see docs/examples.md
```

### Python

Pythonic bindings live in [`bindings/python/`](bindings/python/) – a **cffi** (API-mode) wrapper that compiles the C sources into an extension, so building needs only a C compiler. RAII handles free themselves, options are `@dataclass`es with keyword args, and prompts return the value or `None` on cancel (and raise `SparcliInputUnavailable` with no TTY). Full reference: [`docs/api-python.md`](docs/api-python.md).

```python
import sparcli as sc

sc.panel("Welcome aboard.", sc.PanelOpts(title="Greeting",
         border=sc.BorderStyle(sc.BorderType.ROUNDED)))

t = sc.Table()                                 # frees itself
t.column("Name").column("Age", sc.ColOpts(halign=sc.Align.RIGHT))
t.row(["Ada", "42"])
t.print(sc.TableOpts(header_row=True))

name = sc.text_input("Your name")              # str, or None if cancelled
if name:
    sc.markup.println(f"[green]Hi[/] {name}")
```

```sh
make python                                    # build the extension in place
PYTHONPATH=bindings/python/src python examples/python/output/table_basic.py  # see docs/examples.md
```

---

## Installation

### From source

```sh
git clone https://github.com/cgroening/c-sparcli.git
cd c-sparcli
make                          # builds libsparcli.a, the shared lib, and sparcli.pc
sudo make install             # installs into /usr/local
# or, install into a user prefix:
make install PREFIX=$HOME/.local
```

`make install` lays down:

- `libsparcli.a` (static)
- `libsparcli.<version>.dylib` / `libsparcli.so.<version>` plus the usual versioned symlinks (`libsparcli.dylib` / `libsparcli.so`)
- All public headers under `<prefix>/include`
- `sparcli.pc` under `<prefix>/lib/pkgconfig`

### Linking

With `pkg-config` (recommended):

```sh
cc app.c $(pkg-config --cflags --libs sparcli) -o app
```

Manual:

```sh
cc app.c -I<prefix>/include -L<prefix>/lib -lsparcli -o app
```

### Requirements

- C11 compiler (`cc`, `gcc`, or `clang`)
- A UTF-8-capable terminal
- Truecolor support recommended for `[rgb(…)]` markup; 8-color ANSI works everywhere

---

## Output widgets

A one-line summary per widget. The full reference – every type, every option, every macro – lives in [`docs/api-c.md`](docs/api-c.md).

| Widget | Function family | What it does |
|--------|----------------|--------------|
| **Panel** | `sc_panel_*` | Bordered frame with title, padding, margin, optional background. |
| **Table** | `sc_table_*` | Headers, footers, colspan, rowspan, striping, word-wrap, per-column widths and styles. |
| **Rule** | `sc_rule_*` | Horizontal line with optional centered/aligned title. |
| **Columns** | `sc_columns_*` | Side-by-side layout for any other widgets (with optional separator). |
| **List** | `sc_list_*` | Bulleted, numbered, alpha, or roman lists with hanging indent and nesting. |
| **Tree** | `sc_tree_*` | Hierarchical tree view with connectors. |
| **Key/Value** | `sc_kv_*` | Aligned `key: value` pairs with key-column padding and optional value wrap. |
| **Alert** | `sc_alert_*` | Preset info / warning / error / success boxes (icon + color). |
| **Badge** | `sc_badge_*` | Inline styled token (`[ DONE ]`). |
| **Progress bar** | `sc_progressbar_*` | Animated progress bar with thresholds and percent/value display. |
| **Spinner** | `sc_spinner_*` | Animated activity indicator with success/failure finish. |
| **Live display** | `sc_live_*` | Re-render a composed frame in place: build dashboards from any widgets that update continuously (in-place or fullscreen alternate screen). |
| **Markup** | `sc_markup_*` | Rich-compatible `[bold red]…[/]` parser. |
| **Capture** | `sc_capture_*`, `sc_vstack` | Render any widget into a reusable in-memory buffer; `sc_vstack` stacks several buffers into one column. |
| **Pad** | `sc_pad_*` | Add top/right/bottom/left space around a rendered widget. |
| **Align** | `sc_align_*` | Center- or right-align a rendered widget within a width. |

---

## Input widgets

Interactive prompts that drive a real terminal in raw mode. Each returns an `ScInputStatus` – Esc and Ctrl-C cancel, and a non-TTY context (output piped, CI) returns an error so callers can fall back to a default. The full reference lives in [`docs/api-c.md`](docs/api-c.md#input-widgets).

| Widget | Function family | What it does |
|--------|----------------|--------------|
| **Confirm** | `sc_confirm` | Yes/No prompt; arrow / `y` / `n` selection. |
| **Text input** | `sc_text_input` | Single-line entry with placeholder, validation, autocomplete (ghost text or navigable dropdown, prefix/fuzzy matching), char filters, optional boxed panel. |
| **Password** | `sc_password_input` | Masked single-line entry (configurable mask glyph). |
| **Number** | `sc_number_input` | Numeric entry with min/max/step, ↑/↓ adjustment, comma/period decimal separator, exact-text output (decimal-type safe) and an opt-in calculator mode (`=1,5+2*3` evaluates inline). |
| **Textarea** | `sc_textarea` | Multi-line entry (Ctrl-D submits) with soft-wrap. |
| **Select** | `sc_select_*` | Single- or multi-choice list with a scrolling viewport. |
| **Fuzzy finder** | `sc_fuzzy_*` | Incremental fuzzy search; optional table view. |
| **Date picker** | `sc_datepicker` | Month-grid calendar; day/week/month/year navigation. |
| **Form** | `sc_form_*` | Grid-layout form: framed fields (text/number/bool/select/multiselect/date) with per-field width and col/row spans, 2D navigation, in-place editing below the grid; multiline fields open `$EDITOR`. |
| **Input history** | `sc_history_*` | ↑/↓ recall of previous entries in the text input; optional persistence in the XDG state directory (REPL building block). |
| **Theme** | `sc_input_set_theme` | Process-wide style defaults inherited by every input widget. |

Every widget shows a key-hint footer that is fully configurable: its layout (`hint_layout` – inline, stacked one-per-line, or hidden) and its placement (`hint_pos` – above, below, left, or right of the widget).

```c
char *name = NULL;
if (sc_text_input("Your name", &name, (ScTextInputOpts){ .placeholder = "Ada" })
        == SC_INPUT_OK) {
    sc_markup_println("[green]Hello[/], it's nice to meet you.");
    free(name);
}
```

**Custom shortcuts** – bind extra keys (Ctrl-letter, F1–F12, Alt) to actions on *any* widget via its opts. A `SC_SHORTCUT_RETURN` shortcut closes the prompt and reports which key fired (the widget still returns its value); a `SC_SHORTCUT_CALLBACK` runs in place and keeps the prompt open – handy with `sc_select_remove` / `sc_select_set_label` for live list editing. Labeled shortcuts appear in a dim footer automatically (`hide_in_footer` keeps a binding active but off the footer), and each shortcut also carries a `help_text` + `section` that feed an **auto-built keyboard help screen** (`sc_shortcut_help_show` / `sc_shortcut_help_show_from`, C++ `show_shortcuts`) – a modal, filterable reference with section headers, so apps never hand-roll a "?" screen ([`examples/c/input/shortcuts_help.c`](examples/c/input/shortcuts_help.c)).

**Rich prompts** – for partial styling (e.g. `Rename `*`Apple`*` to`) set `prompt_markup = true` to parse the prompt as markup, or `prompt_text` to pass a pre-built multi-style `ScText`. Works inline and in boxed mode.

**External editor** – `sc_text_input` / `sc_textarea` can open the value in `$EDITOR` (default chain ending in nvim) with `external_editor = true`; a key (default Ctrl-G) suspends the prompt, and save+quit brings the text back. Runs shell-free with a `0600` temp file; not available for passwords.

Runnable per-widget examples live under `examples/c/input/` (and mirrored for every binding) – e.g. the confirm/select, text/password, fuzzy, datepicker and history demos, plus a custom-shortcuts + theme demo ([`examples/c/input/shortcuts_theme.c`](examples/c/input/shortcuts_theme.c) – F2 archives, Ctrl-X deletes):

```sh
make run-example EX=c/input/confirm_select
make run-example EX=c/input/shortcuts_theme
```

**REPLs** – three full programs combine history, the line tokenizer, the argument parser and the live display into interactive shells: [`examples/c/apps/repl_minimal.c`](examples/c/apps/repl_minimal.c) (prompt loop + history), [`examples/c/apps/repl_demo.c`](examples/c/apps/repl_demo.c) (a task-manager REPL on the args module), and [`examples/c/apps/repl_dashboard.c`](examples/c/apps/repl_dashboard.c) (a fixed header + in-place updating body above the prompt, with a shortcut action bar):

```sh
make run-example EX=c/apps/repl_demo
make run-example EX=c/apps/repl_dashboard
```

---

## Application framework

Beyond the widgets, sparcli covers the plumbing a complete CLI application needs – the parts that clap, argparse, or Rich-style logging provide in other languages. The full reference – every type, option, and invariant – lives in [`docs/api-framework.md`](docs/api-framework.md).

| Module | Function family | What it does |
|--------|----------------|--------------|
| **Argument parser** | `sc_args_*` | Declarative subcommands, typed options (string/int/double/color) with defaults, choices and required arguments, auto-generated `--help` rendered with sparcli widgets, "did you mean ...?" suggestions, zsh completion generation. |
| **Logging** | `sc_log_*`, `sc_logger_*` | Leveled, colored terminal logging with timestamps and `file:line`, plus plain-text file sinks with their own levels – thread-safe. |
| **Pretty errors** | `sc_error_*`, `sc_die` | Fatal errors as red alert panels: message + cause chain + hint + exit code. |
| **XDG paths** | `sc_path_*` | Per-application config/data/cache/state directories per the XDG spec, created on first use. |
| **Pager** | `sc_pager_*` | Pipe long output through `$PAGER` / `less -R`; a no-op when output is not a terminal ([C API docs](docs/api-c.md#pager)). |
| **Line tokenizer** | `sc_args_split` | Split a REPL input line into argv (quotes + escapes) and feed it into the same parse tree, once per line. |

A taste of the argument parser – subcommands, typed options and `--help` in a few lines:

```c
ScArgs *args = sc_args_new((ScArgsOpts){
    .prog = "tool", .version = "1.0.0", .about = "Build things, fast",
});
ScArgsCmd *root  = sc_args_root(args);
ScArgsCmd *build = sc_args_subcommand(root, "build", "Build a target");
sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
sc_args_positional(build, "TARGET", SC_ARG_STR, "What to build", true, false);

/* --help, --version, did-you-mean and parse errors are all handled here */
ScArgsStatus status;
const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
if (status != SC_ARGS_MATCHED) {
    return status == SC_ARGS_HANDLED ? 0 : 2;   /* help/version printed = 0 */
}

if (matched == build) {
    long jobs = sc_args_get_int(args, "jobs");  /* typed, validated access */
    /* ... */
}
sc_args_free(args);
```

The other helpers are one-liners:

```c
sc_log_info("listening on port %d", port);    /* leveled, colored, timestamped */

char *cfg = sc_path_config("myapp");          /* ~/.config/myapp (created) */

sc_die_msg(2, "No config file found", "Run 'myapp init' to create one");
```

A tool built on the parser is [`examples/c/app/args.c`](examples/c/app/args.c) (and the REPL-style [`examples/c/app/args_repl.c`](examples/c/app/args_repl.c)); the REPL demos ([`examples/c/apps/repl_demo.c`](examples/c/apps/repl_demo.c), [`examples/c/apps/repl_dashboard.c`](examples/c/apps/repl_dashboard.c)) combine the parser, input history and the live display into interactive shells:

```sh
make run-example EX=c/app/args
```

---

## Command-line tool

Everything above is also available from the shell: `make` builds a `sparcli` binary (installed to `$(PREFIX)/bin` by `make install`, together with a zsh completion) that wraps every output and input widget as a subcommand.

```sh
# Output: markup, panels, rules, tables, trees, alerts, ...
sparcli print "[bold red]Error:[/] file not found"
echo "All systems operational" | sparcli panel --title "Status" --color green
df -h | tr -s ' ' '\t' | sparcli table --tsv --header-row
sparcli alert success "Deployment finished"
sparcli spin --title "Building" -- make all

# Input: prompts whose UI goes to the terminal, the value to stdout -
# perfect for command substitution and exit-code logic in scripts.
name=$(sparcli input "Your name:")
sparcli confirm "Deploy to production?" && ./deploy.sh
branch=$(git branch --format='%(refname:short)' | sparcli select)
file=$(find . -name '*.c' | sparcli fuzzy)
amount=$(sparcli number "Amount:" --decimals 2 --decimal-sep ,)
when=$(sparcli date --format %Y-%m-%d)
```

Input commands report their outcome through the exit code (`0` = confirmed, `1` = cancelled/no, `2` = error or no TTY), so `&&` / `||` chains and `$(...)` capture work the way shell scripts expect. Markup is parsed everywhere by default (`--no-markup` for literal text); `--no-color` / `NO_COLOR` strip the colors.

Two runnable zsh demos ship in [`examples/cli/output_demo.zsh`](examples/cli/output_demo.zsh) and [`examples/cli/input_demo.zsh`](examples/cli/input_demo.zsh). The full reference – every subcommand, flag, data format and scripting pattern – lives in [`docs/cli.md`](docs/cli.md).

---

## Rich-compatible markup

sparcli's inline markup mirrors the syntax used by [Rich](https://github.com/Textualize/rich) and [Textual](https://github.com/Textualize/textual). Existing Rich strings drop in unchanged for the supported tag set:

```c
sc_markup_println("[bold red]Error:[/] file not found");
sc_markup_println("[on cyan] 200 OK [/]   [dim]42ms[/]");
sc_markup_println("[rgb(120,200,255)]custom[/] color");
```

| sparcli                  | Rich (Python)            |
|--------------------------|--------------------------|
| `[bold red]…[/]`         | `[bold red]…[/]`         |
| `[on yellow]…[/]`        | `[on yellow]…[/]`        |
| `[underline]…[/u]`       | `[underline]…[/u]`       |
| `[strike]…[/s]`          | `[strike]…[/s]`          |
| `[rgb(120,200,255)]…[/]` | `[rgb(120,200,255)]…[/]` |
| `[[`                     | `[[`                     |

By default, unknown tags such as `[blink]` are emitted verbatim. Pass `ScMarkupOpts{ .strip_unknown = 1 }` to silently drop them and keep only the inner content.

**Backtick inline code**: `` `code` `` renders the content in magenta with the backticks removed (Markdown-style); the body is literal, so tags inside are not parsed. Escape a literal backtick with `` \` ``. The style is configurable via `ScMarkupOpts.code_style`.

Clickable **OSC-8 hyperlinks** use the same syntax as Rich: `[link=https://example.com]text[/link]` (or `sc_text_append_link()` from code). Supporting terminals open the URL on Cmd/Ctrl+click; others show just the text.

Any widget that takes an `ScText *` accepts markup via `sc_markup_parse()`. For tables, use the `SC_CELL_M("…")` macro to embed markup directly into a cell.

![markup demo](docs/images/output_rich_markup.png)

---

## Development

```sh
make            # static + shared + pkg-config
make test       # run the full non-interactive test suite (all headless gates)
make test-output # visual output gallery (ARGS=--focus / --no-animated)
make test-input # interactive widget suite (needs a real terminal)
make rust       # build the Rust binding (make rust-test to test it)
make python     # build the Python binding (make python-test to test it)
make rebuild-all # rebuild the C lib + install + Rust + Python in one go
make clean
```

Project layout:

```
include/{core,output,input}/   public headers (sparcli.h is the umbrella)
include/sparcli.hpp            header-only C++20 wrapper
src/{core,output,tty,input}/   implementation
src/output/table/              table sub-modules (see docs/api-c.md)
cli/                           the sparcli command-line tool (see docs/cli.md)
completions/                   zsh completion for the CLI
examples/{c,cpp,rust,python}/  per-language examples (see docs/examples.md)
bindings/rust/                 safe Rust crate (sparcli-sys + sparcli)
bindings/python/               safe Python package (cffi API-mode wrapper)
tests/output/                  output suite
tests/input/{logic,style,pty}/ interactive / snapshot / PTY suites
tests/cli/                     CLI golden-file + PTY suites
docs/                          API reference and developer guide
```

See **[`docs/development.md`](docs/development.md)** for the full build/test/ install workflow: every `make` target, what each test suite covers and how to run it, the golden-file workflow, and the pre-commit checklist.

---

## Roadmap

- **C++ wrapper** – ✅ ships as the header-only [`include/sparcli.hpp`](include/sparcli.hpp) (RAII over `ScText`/`ScTableData`/`ScColumns`/…; see below).
- **Rust bindings** – ✅ ship in [`bindings/rust/`](bindings/rust/) (the safe `sparcli` crate over `sparcli-sys`; see [`docs/api-rust.md`](docs/api-rust.md)).
- **Python bindings** – ✅ ship in [`bindings/python/`](bindings/python/) (the cffi API-mode `sparcli` package; see [`docs/api-python.md`](docs/api-python.md)).
- **Command-line tool** – ✅ ships as the `sparcli` binary ([`cli/`](cli/); every widget as a shell subcommand with zsh completion; see [`docs/cli.md`](docs/cli.md)).
- **CLI application framework** – ✅ argument parser (`sc_args_*`, [docs](docs/api-framework.md)), logging (`sc_log_*`), pretty errors (`sc_die`), XDG paths, pager and OSC-8 hyperlinks.
- **Structured data (serde)** – ✅ the opt-in `serde/` layer: JSON/CSV/TOML/YAML/Markdown readers and writers over a shared `ScValue` model, in C and the C++ wrapper, with its own `make qa-serde` gate; see [`docs/api-serde.md`](docs/api-serde.md).
- **CLI on the args module** – migrate [`cli/`](cli/) from `getopt_long` + hand-written usage strings to the argument parser (`sc_args_*`) once its API has stabilized through real-world use. This would remove the duplicated option/usage/completion definitions and give `sparcli --help` widget-rendered output. Deferred on purpose: the CLI is stable and golden-tested; coupling it to a brand-new API would force follow-up changes on every API adjustment.
- **Output theming** – a process-wide `sc_output_set_theme(...)` for output components (default border style/color, title styling, …), mirroring the existing [`sc_input_set_theme`](#input-widgets) for input widgets.
- **`examples/` directory** – ✅ self-contained, copy-pasteable examples for every component in all four languages, grouped by language and area; see [`docs/examples.md`](docs/examples.md).

---

## Why sparcli?

I picked up C to understand what higher-level languages do under the hood – and ended up falling in love with the language. But coming from Python, I missed the tooling I had taken for granted: [Rich](https://github.com/Textualize/rich) and [Textual](https://github.com/Textualize/textual) for output, [Typer](https://github.com/fastapi/typer)/argparse for command lines, [prompt_toolkit](https://github.com/prompt-toolkit/python-prompt-toolkit) and [questionary](https://github.com/tmbo/questionary) for prompts. In C, the answer to most of this is still `getopt` and `printf`.

sparcli is my attempt to close that gap: a single dependency-free library that gives plain C programs styled output, interactive prompts, an argument parser, logging – the plumbing a modern CLI needs. This project is inspired by the wonderful Rich and Textual projects by Will McGugan and the Textualize team.

To move faster, sparcli is developed with the help of Claude Code (Anthropic's Opus model) under strict requirements and review: every feature ships with tests, and every change has to pass the full QA gate – sanitizers, ThreadSanitizer, fuzzing, golden-file and PTY suites (see [`docs/development.md`](docs/development.md)).

---

## License

MIT. See [LICENSE](LICENSE) for the full text.
