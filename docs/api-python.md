# sparcli – Python API Reference

Safe, idiomatic Python bindings for sparcli, in `bindings/python/`.

Built with **cffi** in API / out-of-line mode: `build_sparcli.py` compiles the vendored C sources into the `sparcli._sparcli_cffi` extension, so a build needs only a C compiler (no prior `make`, no system install). The struct layout is verified against the real headers by the C compiler – there is no hand-maintained ABI.

```python
import sparcli as sc
```

## Design

- **RAII handles** (`Text`, `Table`, `List`, `Tree`, `Kv`, `Columns`, `Rendered`, `ProgressBar`, `Spinner`, `Select`, `Fuzzy`) free their C object automatically (via `weakref.finalize`) and also work as context managers (`with sc.Table() as t: …`). They hold raw pointers, so build and use a handle on **one thread** (the C output target is thread-local; the input session is process-global).
- **Options are `@dataclass`es** with keyword arguments and defaults, e.g. `sc.PanelOpts(title="Hi", border=sc.BorderStyle(sc.BorderType.ROUNDED), full_width=True)`. A field left at its default selects sparcli's "unset" behaviour, so partial options just work.
- **Colors / enums.** `sc.Color.{NONE, RED, …, rgb(r,g,b)}`; `sc.Style` (+ the shortcuts `Style.bold()/dim()/italic()/underline()`); `Attr` (an `IntFlag`, combine with `|`), `Align`, `VAlign`, `BorderType`, `Position`, `ListMarker`, `ProgressType`, `SpinnerType`, `AlertType`, `WeekStart`, `HintLayout`, `HintPos`.
- **Prompts** return the value on success, `None` on cancel (Esc / Ctrl-C), and raise `sc.SparcliInputUnavailable` when there is no TTY / on read error. Constructors raise `MemoryError` on allocation failure.
- **Escape hatch.** The raw cffi `ffi`/`lib` are re-exported as `sparcli.sys`.

## Output

```python
sc.println("plain")
sc.print_("bold red", sc.Style.bold(sc.Color.RED))          # print_ avoids the builtin

sc.panel("Hello", sc.PanelOpts(title="greeting", full_width=True))
sc.rule("section", sc.RuleOpts(type=sc.BorderType.DOUBLE))
sc.badge("NEW", sc.BadgeOpts(text_style=sc.Style.bold(bg=sc.Color.GREEN)))
sc.alert.success("done")                                    # info/debug/warning/error/success

t = sc.Table()
t.column("Name", sc.ColOpts(style=sc.Style.bold(sc.Color.CYAN)))   # per-column style
t.column("Age", sc.ColOpts(halign=sc.Align.RIGHT))
t.row(["Ada", "36"]).row(["Alan", "41"])
t.row([sc.Cell("Subtotal", colspan=2), sc.Cell.skip()])     # colspan + skip placeholder
t.footer_row(["Total", "77"])
t.print(sc.TableOpts(border=sc.BorderType.ROUNDED, header_row=True, striped=True))

lst = sc.List(sc.ListOpts(marker=sc.ListMarker.NUMBER))
item = lst.add("top")
item.sub().add("nested")                                    # owned by the parent list
lst.print()

tree = sc.Tree()
root = tree.add("project", style=sc.Style.bold())
tree.add("README", root)
tree.print()

kv = sc.Kv(sc.KvOpts(key_style=sc.Style.bold()))
kv.add("Version", sc.version_string())
kv.print()
```

Cells accept a plain `str`, a `sc.Text`, or a `sc.Cell` (for alignment, colspan/rowspan, markup or rich text – `sc.Cell.markup("[green]ok[/]")`, `sc.Cell.text(t)`, `sc.Cell.skip()`, `sc.Cell.row_skip()`).

### Rich text & markup

```python
t = sc.Text()
t.append("status: ").append("OK", sc.Style.bold(sc.Color.GREEN))
t.append_link("docs", "https://example.com/docs")           # OSC-8 hyperlink
t.print()

sc.markup.println("[bold red]Error:[/] file not found")
sc.markup.println("Open [link=https://example.com]the docs[/link]")
parsed = sc.Text.from_markup("[italic]hi[/]")               # use with any *_text widget
```

`append_link` and the `[link=URL]text[/link]` markup tag emit OSC-8 terminal hyperlinks (clickable in supporting terminals, plain text elsewhere); the escape bytes have zero visible width.

### Capture / compose

```python
r = sc.capture.panel("hi", sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE)))
r.pad(sc.PadOpts(left=4))
r.align(sc.Align.CENTER)                # 0 / omitted = terminal width
lines = r.lines                         # list[str], ANSI codes included
stacked = sc.vstack([r, r], gap=1)      # one column, two widgets

cols = sc.Columns(sc.ColumnsOpts(gap=3))
cols.add_rendered(r).add_str("text")
cols.print()
```

`capture` has `string`, `text`, `table`, `list`, `tree`, `kv`, `columns`, `panel`, `rule`. One-step helpers: `sc.pad_str/pad_text`, `sc.align_str/align_text`.

### Progress & spinner

```python
bar = sc.ProgressBar(sc.ProgressBarOpts(left_cap="[", right_cap="]", show_percent=True))
bar.set_label("Installing")
for v in range(101):
    bar.draw(v, 100)
bar.finish(100, 100)

sp = sc.Spinner("Loading", sc.SpinnerOpts(type=sc.SpinnerType.DOTS))
sp.tick()
sp.finish(True, "Done")
```

### Redirecting output

```python
with open("out.txt", "w") as f, sc.ScopedOutput(f):
    sc.println("goes to the file")
```

### Application helpers (XDG paths, pager)

```python
# XDG directories (created on first use); raises sc.SparcliError on failure
cfg = sc.config_dir("myapp")                                # ~/.config/myapp (Path)
log = sc.app_file(sc.PathKind.STATE, "myapp", "logs/run.log")

# Pager: output inside the block is piped through $PAGER / less -R;
# no-op when the output stream is not a terminal (scripts, pipes, CI)
with sc.Pager() as pager:
    table.print(sc.TableOpts(header_row=True))
print(pager.exit_status)

# Live display: re-render a composed frame in place (dashboard).
# Off-terminal, only the final frame is printed when the session ends.
with sc.Live() as live:                       # Live(alt_screen=True) = fullscreen
    for i in range(0, 101, 10):
        live.update(sc.capture.string(f"progress: {i}%"))

# Pretty errors: message + causes + hint + exit code as a red panel.
# die() raises SystemExit (never calls the C exit()), so cleanup runs.
sc.ErrorReport("Config could not be loaded") \
    .cause("file not found: ~/.config/app/config.toml") \
    .hint("Run 'app init' to create a default config") \
    .code(2) \
    .die()

# Logging: global logger (colored stderr at INFO + optional file sinks) ...
sc.log_set_level(sc.LogLevel.DEBUG)
sc.log_add_file("app.log", sc.LogLevel.DEBUG)
sc.log_info("server started")              # message is data, never a format

# ... or an independent handle-based logger
logger = sc.Logger(hide_timestamps=True)
logger.add_terminal(sys.stderr, sc.LogLevel.INFO)
logger.add_file("debug.log")
logger.warning("low disk space")
```

## Input

Every widget returns its value, or `None` on cancel; it raises `sc.SparcliInputUnavailable` with no TTY.

```python
if sc.confirm("Proceed?", sc.ConfirmOpts(default_yes=True)):
    ...

name = sc.text_input("Name", sc.TextInputOpts(placeholder="Ada"))
pw   = sc.password_input("Password", sc.PasswordOpts(mask="•"))
n    = sc.number_input("Qty", sc.NumberOpts(min=0, max=100, step=5))
notes = sc.textarea("Notes", sc.TextareaOpts(boxed=True, width=48))

# Exact decimal.Decimal (never via float) – for money. decimal_sep="," lets
# German users type "12,99"; the Decimal is built from the exact text.
# start_empty=True starts with an empty field instead of "0,00" (Enter on an
# empty field is ignored).
amount = sc.decimal_input(
    "Amount", sc.NumberOpts(decimals=2, decimal_sep=",", start_empty=True)
)
# -> Decimal("12.99") | None

sel = sc.Select(sc.SelectOpts(prompt="Pick", multi=True))
sel.add("a").add("b").add("c")
chosen = sel.run()          # list[int] (multi) – or sel.run_one() -> int

fz = sc.Fuzzy(sc.FuzzyOpts(prompt="Find"))
fz.add("Tokyo").add("London")
i = fz.run()                # add-order index

import datetime
d = sc.datepicker(datetime.date.today(), sc.DatePickerOpts(week_start=sc.WeekStart.MONDAY))
# -> datetime.date

ok, score = sc.fuzzy_match("ab", "cab")     # pure, no TTY
```

`input_available()` reports whether a prompt can run (useful to fall back to a default in non-interactive contexts).

### Input constraints

### Calculator mode (number input)

`NumberOpts(calculator=True)` lets the user type `=` followed by an arithmetic expression (`=1,5+2*3`): a live preview shows the result, Enter accepts it into the field, a second Enter submits. By default the field displays the result rounded to `decimals` while the submitted value (and `decimal_input`'s `Decimal`) keeps full precision; `calc_store_rounded=True` submits the displayed value instead, `calc_show_precise=True` also displays full precision.

```python
amount = sc.decimal_input("Amount", sc.NumberOpts(
    decimals=2, decimal_sep=",", start_empty=True, calculator=True))
# user types "=12,99*3" → Enter → Enter → Decimal("38.97")

sc.calc_eval("1,5+2*3")   # 7.5 – the pure evaluator, no TTY needed
sc.calc_eval("1/0")       # None (invalid)
```

`text_input`/`password_input` accept a `char_filter` – either a built-in (`sc.filter_digits`, `sc.filter_decimal`, `sc.filter_alpha`, `sc.filter_alnum`, `sc.filter_no_space`) or a Python callable `(ch: str) -> bool` – and a `validate` callable `(value: str) -> str | None` (return an error message to keep the prompt open). `text_input` also takes `suggestions=[...]` for Tab autocomplete. `sc.filter_decimal` accepts both `.` and `,` as decimal separator.

### Autocomplete dropdown

By default `suggestions` shows the first prefix match as dim ghost text (Tab accepts). Pass `suggest=sc.SuggestOpts(...)` to present them as a navigable dropdown below the field instead – ↑/↓ move the highlight, Tab/Enter accept it, Enter without a highlight submits the typed value:

```python
cmd = sc.text_input("Git command", sc.TextInputOpts(
    suggestions=["commit", "checkout", "cherry-pick"],
    suggest=sc.SuggestOpts(
        mode=sc.SuggestMode.DROPDOWN,
        match=sc.SuggestMatch.FUZZY,           # or PREFIX (default)
        max_visible=5,                          # rows; more get "… +N more"
        border=sc.BorderStyle(type=sc.BorderType.ROUNDED),
        selected_style=sc.Style(fg=sc.Color.BLACK, bg=sc.Color.CYAN),
    ),
))
```

### Custom shortcuts

Bind extra keys (Ctrl-letter / F1–F12 / Alt) on any widget. RETURN mode ends the prompt and records an id (read with `fired()`); CALLBACK mode runs a Python callable and keeps the prompt open unless it returns `False`.

```python
sc_ = (sc.Shortcuts()
       .on_return(sc.key_fn(2), 1, name="help")
       .on_callback(sc.key_ctrl("r"), lambda: reload_data(), name="reload"))

sc.text_input("Name", sc.TextInputOpts(shortcuts=sc_))
if sc_.fired() == 1:
    show_help()
```

Live editing of `Select`/`Fuzzy` from a callback: `select.cursor()`, `select.label(i)`, `select.set_label(i, "…")`, `select.remove(i)`; `fuzzy.cursor_index()`, `fuzzy.remove(i)`.

### Rich prompts & external editor

```python
# Only part of the prompt styled:
p = sc.Text().append("Rename ").append("Apple", sc.Style.italic()).append(" to")
sc.text_input("", sc.TextInputOpts(prompt_text=p))
# or markup: sc.TextInputOpts(prompt_markup=True) with "Rename [italic]Apple[/] to"

# Open $EDITOR (default chain → nvim) with Ctrl-G (text_input & textarea only):
sc.textarea("Notes", sc.TextareaOpts(external_editor=True))
```

### Theme

```python
sc.set_theme(sc.Theme(accent=sc.Color.CYAN, hint_layout=sc.HintLayout.STACKED))
# per-call opts > theme > built-in default; sc.set_theme(None) clears it
```

### ANSI-injection protection

User strings are sanitized by default (control bytes and escape sequences removed). Opt out globally or per widget:

```python
sc.set_allow_ansi(True)        # process-wide; sc.allow_ansi() reads it back
sc.panel('\x1b[31mred\x1b[0m', sc.PanelOpts(ansi=sc.AnsiMode.ALLOW))
# AnsiMode.DEFAULT (inherit global) / ALLOW / SANITIZE
```

## Build & test

```sh
make python         # build the extension in place (src/sparcli/_sparcli_cffi.*)
make python-test    # build + run the non-interactive pytest suite

# point make at an interpreter that has cffi:
make python-test PY=/path/to/python

# run the examples (after `make python`, from bindings/python/):
PYTHONPATH=src python examples/demo.py            # complete showcase
PYTHONPATH=src python examples/output_gallery.py  # output only
PYTHONPATH=src python examples/input_demo.py      # input only (needs a terminal)
```

Install into an environment – an **editable** (`-e`) install with build isolation **off**, since the C sources are reached through the in-repo `csrc`/`cinclude` symlinks and the build must run in place:

```sh
pip install cffi setuptools wheel
pip install --no-build-isolation -e bindings/python
```

> Publishing to PyPI would vendor (copy) the C sources into the sdist; in-repo the binding references them through the symlinks.
