# sparcli – Polished CLI output & prompts

A C11 library for **styled terminal output** and **interactive prompts**:

- panels, tables, rules, columns, lists, trees, key/value blocks, alerts,
  badges, progress bars and spinners;
- confirm, text, password, number, textarea, select, fuzzy and date-picker
  prompts.

Ships with **Rich-compatible inline markup** and a header-only **C++ wrapper**.

![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)
![Language: C11](https://img.shields.io/badge/c-11-blue.svg)
![Version: 0.1.0](https://img.shields.io/badge/version-0.1.0-orange.svg)

![sparcli hero](docs/images/output_hero.png)

---

![sparcli hero](docs/images/input_hero.png)

---

## Table of contents

- [sparcli – Polished CLI output \& prompts](#sparcli--polished-cli-output--prompts)
  - [Table of contents](#table-of-contents)
  - [Features](#features)
  - [Quick start](#quick-start)
    - [C](#c)
    - [C++ (header-only)](#c-header-only)
  - [Installation](#installation)
    - [From source](#from-source)
    - [Linking](#linking)
    - [Requirements](#requirements)
  - [Output widgets](#output-widgets)
  - [Input widgets](#input-widgets)
  - [Rich-compatible markup](#rich-compatible-markup)
  - [Development](#development)
  - [Roadmap](#roadmap)
  - [Inspiration](#inspiration)
  - [License](#license)

---

## Features

- **Large set of widgets**: panels, tables, rules, side-by-side columns, lists,
  trees, key/value blocks, alerts, badges, progress bars, spinners.
- **Interactive prompts**: confirm, text/password, number, textarea, single &
  multi select, fuzzy finder, and a date picker – each with a non-TTY fallback.
- **Rich-compatible markup**: `[bold red]error[/]`, `[on cyan] OK [/]`,
  `[rgb(120,200,255)]…[/]` – same syntax as
  [Rich](https://github.com/Textualize/rich)/[Textual](https://github.com/Textualize/textual).
- **Truecolor + 8-color ANSI**, with graceful sentinels for "no color".
- **UTF-8 & ANSI-aware** width math everywhere (codepoints, not bytes).
- **Composable**: capture any widget into a buffer, then pad, align, or place it
  inside a columns layout.
- **C++ wrapper included**: a header-only RAII C++20 layer (`<sparcli.hpp>`,
  namespace `sparcli`) – no manual `free`, owned strings, `std::optional` inputs.
- **FFI-ready**: `extern "C"`, hidden symbol visibility, opaque types, NULL-safe
  entry points – ready for future Python/Rust bindings.
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

A header-only C++20 wrapper ships in [`include/sparcli.hpp`](include/sparcli.hpp):
RAII handles (no manual `free`), owned strings where the C API borrows (so
temporaries are safe), and `std::optional` returns for input prompts. Full
reference: [`docs/api-cpp.md`](docs/api-cpp.md).

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

These guarantees (auto-free, owned cell strings, surviving a move) are verified
by [`tests/cpp/test_cpp.cpp`](tests/cpp/test_cpp.cpp).

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
- `libsparcli.<version>.dylib` / `libsparcli.so.<version>` plus the usual
  versioned symlinks (`libsparcli.dylib` / `libsparcli.so`)
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
- Truecolor support recommended for `[rgb(…)]` markup; 8-color ANSI works
  everywhere

---

## Output widgets

A one-line summary per widget. The full reference – every type, every option,
every macro – lives in [`docs/api-c.md`](docs/api-c.md).

| Widget | Function family | What it does |
|--------|----------------|--------------|
| **Panel** | `sc_panel_*` | Bordered frame with title, padding, margin, optional background. |
| **Table** | `sc_table_*` | Headers, footers, colspan, rowspan, striping, word-wrap, per-column widths. |
| **Rule** | `sc_rule_*` | Horizontal line with optional centered/aligned title. |
| **Columns** | `sc_columns_*` | Side-by-side layout for any other widgets (with optional separator). |
| **List** | `sc_list_*` | Bulleted, numbered, alpha, or roman lists with hanging indent and nesting. |
| **Tree** | `sc_tree_*` | Hierarchical tree view with connectors. |
| **Key/Value** | `sc_kv_*` | Aligned `key: value` pairs with key-column padding and optional value wrap. |
| **Alert** | `sc_alert_*` | Preset info / warning / error / success boxes (icon + color). |
| **Badge** | `sc_badge_*` | Inline styled token (`[ DONE ]`). |
| **Progress bar** | `sc_progressbar_*` | Animated progress bar with thresholds and percent/value display. |
| **Spinner** | `sc_spinner_*` | Animated activity indicator with success/failure finish. |
| **Markup** | `sc_markup_*` | Rich-compatible `[bold red]…[/]` parser. |
| **Capture** | `sc_capture_*`, `sc_vstack` | Render any widget into a reusable in-memory buffer; `sc_vstack` stacks several buffers into one column. |
| **Pad** | `sc_pad_*` | Add top/right/bottom/left space around a rendered widget. |
| **Align** | `sc_align_*` | Center- or right-align a rendered widget within a width. |

---

## Input widgets

Interactive prompts that drive a real terminal in raw mode. Each returns an
`ScInputStatus` – Esc and Ctrl-C cancel, and a non-TTY context (output piped,
CI) returns an error so callers can fall back to a default. The full reference
lives in [`docs/api-c.md`](docs/api-c.md#input-widgets).

| Widget | Function family | What it does |
|--------|----------------|--------------|
| **Confirm** | `sc_confirm` | Yes/No prompt; arrow / `y` / `n` selection. |
| **Text input** | `sc_text_input` | Single-line entry with placeholder, validation, autocomplete, char filters, optional boxed panel. |
| **Password** | `sc_password_input` | Masked single-line entry (configurable mask glyph). |
| **Number** | `sc_number_input` | Numeric entry with min/max/step and ↑/↓ adjustment. |
| **Textarea** | `sc_textarea` | Multi-line entry (Ctrl-D submits) with soft-wrap. |
| **Select** | `sc_select_*` | Single- or multi-choice list with a scrolling viewport. |
| **Fuzzy finder** | `sc_fuzzy_*` | Incremental fuzzy search; optional table view. |
| **Date picker** | `sc_datepicker` | Month-grid calendar; day/week/month/year navigation. |
| **Theme** | `sc_input_set_theme` | Process-wide style defaults inherited by every input widget. |

Every widget shows a key-hint footer that is fully configurable: its layout
(`hint_layout` — inline, stacked one-per-line, or hidden) and its placement
(`hint_pos` — above, below, left, or right of the widget).

```c
char *name = NULL;
if (sc_text_input("Your name", &name, (ScTextInputOpts){ .placeholder = "Ada" })
        == SC_INPUT_OK) {
    sc_markup_println("[green]Hello[/], it's nice to meet you.");
    free(name);
}
```

Build a runnable demo of every input widget with
[`examples/input_demo.c`](examples/input_demo.c):

```sh
make run-example EX=input_demo
```

---

## Rich-compatible markup

sparcli's inline markup mirrors the syntax used by
[Rich](https://github.com/Textualize/rich) and
[Textual](https://github.com/Textualize/textual). Existing Rich strings drop in
unchanged for the supported tag set:

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
| `[rgb(120,200,255)]…[/]` | `[rgb(120,200,255)]…[/]` |
| `[[`                     | `[[`                     |

By default, unknown tags such as `[blink]` are emitted verbatim. Pass
`ScMarkupOpts{ .strip_unknown = 1 }` to silently drop them and keep only the
inner content.

Any widget that takes an `ScText *` accepts markup via `sc_markup_parse()`. For
tables, use the `SC_CELL_M("…")` macro to embed markup directly into a cell.

![markup demo](docs/images/output_rich_markup.png)

---

## Development

```sh
make            # static + shared + pkg-config
make test       # run the full non-interactive test suite (all headless gates)
make test-output # visual output gallery (ARGS=--focus / --no-animated)
make test-input # interactive widget suite (needs a real terminal)
make clean
```

Project layout:

```
include/{core,output,input}/   public headers (sparcli.h is the umbrella)
src/{core,output,tty,input}/   implementation
src/output/table/              table sub-modules (see docs/api-c.md)
tests/output/                  output suite
tests/input/{logic,style,pty}/ interactive / snapshot / PTY suites
docs/                          API reference and developer guide
```

See **[`docs/DEVELOPMENT.md`](docs/DEVELOPMENT.md)** for the full build/test/
install workflow: every `make` target, what each test suite covers and how to
run it, the golden-file workflow, and the pre-commit checklist.

---

## Roadmap

- **C++ wrapper** – ✅ ships as the header-only [`include/sparcli.hpp`](include/sparcli.hpp)
  (RAII over `ScText`/`ScTableData`/`ScColumns`/…; see below).
- **Python bindings** (`sparcli-py`): `cffi`/`ctypes`-based wrapper with
  Pythonic constructors.
- **Rust crate** (`sparcli-rs`): safe `&str`-friendly wrappers around the C API.
- **Output theming** – a process-wide `sc_output_set_theme(...)` for output
  components (default border style/color, title styling, …), mirroring the
  existing [`sc_input_set_theme`](#input-widgets) for input widgets.
- **`examples/` directory** with self-contained copy-pasteable snippets.
- **More widgets** – open an issue with ideas.

---

## Inspiration

Heavily inspired by the wonderful [Rich](https://github.com/Textualize/rich) and
[Textual](https://github.com/Textualize/textual) projects by Will McGugan and
the Textualize team. The goal of sparcli is to give plain C programs the same
level of polish for one-shot CLI output – without taking on a full TUI runtime.

---

## License

MIT. See [LICENSE](LICENSE) for the full text.
