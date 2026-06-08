# sparcli – Examples

Self-contained, copy-pasteable examples for every sparcli component, in all
four languages (C, C++, Rust, Python). Each file is small and focused on one
group of widgets, with comments explaining the API as it is used. Together
they cover the full public surface of each binding, so they double as a
secondary test suite – every example builds warning-free and runs to
completion (the interactive ones fall back gracefully without a terminal).

The examples live under `examples/`, grouped by language and then by area:

```
examples/
  c/output/     c/input/     c/app/     c/data/   c/apps/   (+ readme_screenshots_*.c)
  cpp/output/   cpp/input/   cpp/app/   cpp/data/  cpp/apps/
  rust/output/  rust/input/  rust/app/
  python/output/ python/input/ python/app/
  cli/
```

The `data/` group covers the **serde** layer (structured read/write parsers) and
is C and C++ only – Rust uses `serde`/`toml`/`serde_yaml` and Python its own
ecosystem, so those bindings deliberately don't wrap it.

## Interactive launcher

[`examples/run.zsh`](../examples/run.zsh) is a zsh launcher that drives the
`sparcli` CLI on its own demo tree, picking an example in three steps –
**language → group → example** (e.g. `c → output → table_basic`) – then running
it. The last step is a fuzzy finder, so you can type to filter. Needs the
`sparcli` binary (`make`) and a real terminal.

```sh
./examples/run.zsh        # or: SPARCLI=/usr/local/bin/sparcli ./examples/run.zsh
```

Navigate with the arrow keys: **→ selects / goes forward**, **← goes back one
level** (Enter and Esc still work too; Esc on the language menu exits). This
uses the CLI's `select`/`fuzzy --arrow-nav` flag, which reports the back key
via exit code `3`. The `cli` category has no groups, so it lists its demos
directly.

## Running an example

One command dispatches on the language prefix:

```sh
make run-example EX=c/output/panel_alert       # compile + run the C example
make run-example EX=cpp/input/fuzzy            # C++ (built at C++23)
make run-example EX=rust/output/table_basic    # cargo run --example ...
make run-example EX=python/app/paths           # run against the cffi package
```

Equivalent direct invocations:

```sh
# C / C++: built into build.examples.nosync/ (linked against libsparcli.a)
make examples
./build.examples.nosync/c/output/panel_alert
./build.examples.nosync/cpp/input/fuzzy

# Rust: every example is registered in bindings/rust/sparcli/Cargo.toml under a
# flattened name (group_file). From bindings/rust/ :
cargo run -p sparcli --example output_table_basic

# Python: needs the built extension (make python) on the path. From the repo root:
PYTHONPATH=bindings/python/src python3 examples/python/app/paths.py
```

Interactive examples (anything under `input/`, plus the pager and live
display) need a real terminal. Run without one and they print a notice and
exit cleanly; the pure helpers they also exercise (`calc_eval`,
`fuzzy_match`, …) still run.

## Feature coverage by binding

The C and Python bindings expose the full library. The C++ wrapper mirrors it
closely. The Rust wrapper now covers the same feature surface; the only
intentional omission is the argument parser (Rust uses `clap`):

| Feature | C | C++ | Rust | Python |
|---------|:-:|:---:|:----:|:------:|
| Output widgets, capture/compose, live, pager | ✓ | ✓ | ✓ | ✓ |
| Argument parser (`args`) | ✓ | ✓ | – (use `clap`) | – (use `argparse`) |
| Input theme | ✓ | ✓ | ✓ | ✓ |
| Fuzzy table view | ✓ | ✓ | ✓ | ✓ |
| Text-input validation callback | ✓ | ✓ | ✓ | ✓ |
| Exact decimal input | via `out_text` | via `number_input_text` | via `number_input_text` | `decimal_input` |

---

## Output

Non-interactive; safe to run anywhere.

| File | What it shows |
|------|---------------|
| `output/text_styles` | Colors and attributes, rich multi-span `Text`, OSC-8 links, badges, `strip_ansi`/`truncate`, and the ANSI-sanitization opt-out (per-widget `ansi` / `set_allow_ansi`). |
| `output/color_grid` | Every color model `ScColor` renders: the 8 named ANSI colors (as text and backgrounds), text attributes, and 24-bit `sc_color_from_rgb` gradients (hue spectrum, per-channel ramps, grayscale).² |
| `output/palette` | The curated named RGB palette (`SC_COLOR_*`): hue / `_VIVID` / accent / diagnostic swatches, palette names in markup and panels, `sc_color_by_name`, and the runtime override (`sc_palette_set`/`_reset`).² |
| `output/markup` | Rich-style markup tags (incl. `[strike]`/`[s]`), nesting, literal brackets, inline code spans, link tags, `strip_unknown`/`code_style`. |
| `output/panel_alert` | Bordered panels (titles, subtitles, padding, alignment, full width) and the five alert presets. |
| `output/table_basic` | Columns, header/footer rows, alignment, border styles, per-column style. |
| `output/table_advanced` | Colspan/rowspan, stripes, header column, markup cells, per-row background, word-wrap, row limits.¹ |
| `output/list_tree` | Numbered/bulleted/nested lists with styled markers, and a styled tree with prefixes. |
| `output/rule_kv` | Horizontal rules (titles, width, placement) and key-value lists (fixed width, wrapped values). |
| `output/columns_layout` | Side-by-side columns with separators, capture + `vstack` composition, pad/align, and redirecting the output stream into a buffer/file. |
| `output/progress_spinner` | Animated progress bars (block + threshold-colored line), a spinner with a changing label, and `clear_line` for a transient status. |
| `output/live` | Live in-place dashboard re-rendered from a captured + vstacked frame, plus the fullscreen alternate-screen (`alt_screen`) variant. |
| `output/multiprogress` · `output/diff` | Several progress bars updated together in place; a colored unified diff (hunks, `-`/`+`, framed via capture). Both in C, C++, Rust and Python; the C++ `output/diff` also shows `humanize::*` and the C++ `data/markdown_render` shows `view::value_render`. |
| `output/pager` | Routing long output through `$PAGER` / `less -R` (no-op off a terminal). |

¹ Word-wrap and `max_rows` are demonstrated in the C and Python variants; the
Rust/C++ table opts cover the same border/stripe/span surface.
² C only; the color model (named / 24-bit RGB / unset) is identical in every
binding (see each language's color helpers).

## Input

Interactive; need a real terminal. Each falls back to a notice without one.

| File | What it shows |
|------|---------------|
| `input/confirm_select` | Yes/no confirmation and single/multi selection (pre-check, cursor, viewport). |
| `input/text_password` | Text input (placeholder, validation, char filter, autocomplete dropdown, boxed, rich `prompt_markup` prompt) and masked password input. |
| `input/number_calc` | Stepping/clamping numeric input, calculator mode (exact value), and the pure `calc_eval`. |
| `input/textarea_editor` | Multi-line textarea and the external-`$EDITOR` hook (Ctrl-G). |
| `input/fuzzy` | Fuzzy finder over a list and a table (all languages), plus the pure `fuzzy_match`. |
| `input/datepicker` | Month-grid date picker, today-seeded and pre-seeded, with week-start choice. |
| `input/history` | Up/Down input history with XDG-file persistence across runs. |
| `input/shortcuts_theme` (C/C++/Python) · `input/shortcuts` (Rust) | Custom RETURN/CALLBACK key shortcuts; all files also set the process-wide input theme. |
| `input/shortcuts_help` (C/C++/Rust/Python) | Per-shortcut footer/help/section metadata + the auto-built keyboard help screen (`sc_shortcut_help_show` / `show_shortcuts`), incl. a hidden-from-footer binding and help-only rows. |

## Application framework

| File | What it shows | Languages |
|------|---------------|-----------|
| `app/paths` | Per-application XDG config/data/cache/state directories and a file path inside one. | C, C++, Rust, Python |
| `app/errors_logging` | Pretty error reports (cause chain + hint + code) and leveled logging (global + handle-based). | C, C++, Rust, Python |
| `app/args` | Declarative argument parser: subcommands, typed options, choices, help/version, did-you-mean. | C, C++ |
| `app/args_repl` | Reusing one parser tree per input line (tokenizer + implicit reset) – the building block for a REPL. | C, C++ |
| `app/completion` | Generate zsh / bash / fish completion scripts from an args command tree (`sc_args_print_{zsh,bash,fish}_completion`). | C, C++ |
| `app/humanize` | Human-readable sizes, durations, relative time and grouped/compact numbers (locale separators). | C, C++, Rust, Python |
| `app/subprocess` | Run a command without a shell, capture stdout, feed stdin, read the exit code (`sc_run`). | C, C++ |
| `app/config` (C) · `app/config_layer` (C++) | Layered config: defaults < environment (`__` nesting) < explicit overrides, dotted-path getters. | C, C++ |

## Data parsers (serde)

Non-interactive; safe to run anywhere. These use the opt-in `serde/` layer
(`#include <serde/sparcli_serde.h>` / `.hpp`, not pulled in by `sparcli.h`).

| File | What it shows | Languages |
|------|---------------|-----------|
| `data/json_roundtrip` | Parse JSON into the `ScValue` tree, navigate it, pretty-print it back. | C, C++ |
| `data/csv_to_table` | Read CSV with a header row and render it as a sparcli table (serde feeding the output renderer). | C, C++ |
| `data/toml_config` | Parse a TOML config, read typed values, present them as a key/value list. | C |
| `data/yaml_convert` | Parse YAML and emit JSON – conversion is "parse one, write the other" over the shared model. | C |
| `data/markdown_outline` | Split front matter, walk the heading outline, then edit the front matter from a value tree and re-serialize. | C, C++ |
| `data/config_merge` | A config workflow over the `ScValue` ergonomics: deep-merge defaults + overlay, read via a dotted `path` and typed getters, drop a key, and round-trip through a file (`*_write_file`/`*_parse_file`). | C, C++ |
| `data/value_render` | jq-style colored pretty-printing of a parsed `ScValue` (the opt-in **view** layer, `view/sparcli_view.h`). | C |
| `data/markdown_render` | Render Markdown to the terminal through the widget stack (headings, lists, code, quotes, pipe tables, inline emphasis) + pretty-print a value (view layer). | C, C++ |

The full serde reference is in [`docs/api-serde.md`](api-serde.md). The view
layer (rendering serde models to the terminal) is documented in
[CLAUDE.md](../CLAUDE.md) under "View" and exercised by `make test-view`.

## Full applications (C)

Larger end-to-end programs that combine many widgets:

| File | What it shows |
|------|---------------|
| `c/apps/repl_minimal` | The smallest REPL: a prompt loop with Up/Down history. |
| `c/apps/repl_demo` | A task-manager REPL built on the argument parser (split + implicit reset + history). |
| `c/apps/repl_dashboard` | A fixed live header above an interactive prompt, with a shortcut action bar. |
| `c/apps/fullscreen_app` · `cpp/apps/fullscreen_app` | An alternate-screen shell: `sc_altscreen_begin`/`_end` owns the screen for the loop, and a fuzzy finder and a form run full-screen (`fullscreen = true`) with a pinned rich-`Text` header panel and `valign`. The finder grows with its items up to the terminal height, then scrolls; the leftover space is distributed by `valign` (below the finder / above the header / both). Enter or `^F` opens the form to edit the selected item; `^R` adds an item live via `sc_fuzzy_refresh`. (C + C++.) |
| `c/apps/todo_fuzzy` · `cpp/apps/todo_fuzzy` | A mini todo app on the fuzzy finder: day sections, time-ordered table, per-cell status colors + a rich priority badge, multi-select with a checkbox column, and done/delete actions on the checked set in a re-run loop. Uses the modal (vim-style) mode: normal mode acts on bare keys (`d` done, `x` delete, `c` clear, `j`/`k` move, space check), `i` enters insert mode to type a filter, `Esc` returns; the query line is badged + tinted per mode. (C + C++.) |
| `c/apps/form_demo` · `cpp/apps/form_demo` | A grid-layout form: framed fields arranged in a raster with per-field width (%/fixed/auto) and col/row spans (a "Tier" select spanning two columns, a tall multiline "Notes" field spanning two rows). Arrow keys move the active box in 2D; Enter opens the editor below the grid (boxed in the accent color), a second Enter saves; select/multiselect/date open list/grid pickers; the multiline Notes field opens `$EDITOR`/nvim with Enter or Ctrl-G; bool fields toggle with Space; Ctrl-D submits. (C + C++.) |

Run them with `make run-example EX=c/apps/repl_demo` (or `EX=cpp/apps/form_demo`) in a real terminal.

## CLI tool

The `sparcli` command-line binary exposes every widget as a shell subcommand.
Two runnable zsh walkthroughs:

| File | What it shows |
|------|---------------|
| `cli/output_demo.zsh` | Every output subcommand (panels, tables, rules, lists, …). |
| `cli/input_demo.zsh` | Every input subcommand via command substitution. |

```sh
make            # build the ./sparcli binary first
./examples/cli/output_demo.zsh
./examples/cli/input_demo.zsh
```

The full CLI reference is in [`docs/cli.md`](cli.md).

## README screenshot generators

`examples/readme_screenshots_output.c` and `examples/readme_screenshots_input.c`
render the exact frames used for the screenshots in the README. They are not
tutorial examples – they are kept for regenerating the images.

---

For the per-language API details each example builds on, see
[`docs/api-c.md`](api-c.md), [`docs/api-cpp.md`](api-cpp.md),
[`docs/api-rust.md`](api-rust.md), [`docs/api-python.md`](api-python.md),
[`docs/api-framework.md`](api-framework.md) and
[`docs/api-serde.md`](api-serde.md).
