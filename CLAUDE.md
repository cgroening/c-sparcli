# sparcli – Developer Reference

A C11 library for styled terminal output: colored text, bordered panels, feature-rich tables, horizontal rules, and multi-column side-by-side layouts.

## Build

```sh
make            # builds libsparcli.a + shared lib + pkg-config + the sparcli CLI
make cli          # only the sparcli CLI binary (./sparcli)
make qa           # EVERY QA gate in one command: test -Werror, sanitize, tsan,
                  # lint, fuzz, rust-test, rust-lint (clippy -D warnings),
                  # python-test(+debug). The complete pre-commit validation -
                  # run this after any change.
make test         # FULL non-interactive suite: chains the headless gates below
                  # (test-output-check, test-input ARGS=--logic,
                  # test-input-style-check, test-input-pty, test-app, test-args,
                  # test-cpp, test-cli-check, test-cli-pty). The canonical check.
make test-output  # OUTPUT gallery (tests/output/test_main), printed for eyeballing.
                  # ARGS=--focus / --no-animated (and the combo).
make test-output-check / -golden   # OUTPUT golden-file diff / regenerate snapshot
make test-input   # INPUT logic+widget suite (tests/input/logic/) – interactive,
                  # needs a real TTY. `ARGS=--logic` runs only the non-interactive
                  # logic tests (key decoder, line editor, filters, threads) for CI.
make test-input-style # INPUT style snapshots (tests/input/style/) – NON-interactive:
                  # renders every widget in many styles; safe in CI.
make test-input-style-check / -golden  # style golden-file diff / regenerate
make test-input-pty   # INPUT self-driving PTY suite under ASan/UBSan: forks each
                  # widget onto a pseudo-terminal and feeds canned keys – gives
                  # interactive coverage with no human. Runs headless (CI).
make test-app     # APP framework suite (tests/app/): XDG paths, pager, pretty
                  # errors + logging logic tests; headless (CI).
make test-args    # ARGS parser suite (tests/args/): parse loop, typed values,
                  # error reporting, help + completion rendering; headless (CI).
make test-cli-check / -golden  # CLI output golden-file diff / regenerate
                  # (tests/cli/run_output.sh drives every output subcommand)
make test-cli-pty     # CLI input PTY suite under ASan/UBSan: forks the sanitized
                  # CLI binary onto a PTY with stdout on a pipe (= command
                  # substitution), asserts value + exit code. Headless (CI).
make sanitize     # OUTPUT suite under ASan/UBSan
make tsan         # INPUT logic suite under ThreadSanitizer (verifies the
                  # thread-safety invariant incl. concurrent logging; own build tree)
make lint         # static analysis: cppcheck + clang-tidy (.clang-tidy config;
                  # warnings are errors; tools optional, prints install hints
                  # when missing)
make fuzz         # random-input fuzzing of the markup parser, key decoder,
                  # ANSI sanitizer, CLI CSV parser + argument parser under
                  # ASan/UBSan (FUZZ_ITERS / FUZZ_SEED overridable)
make EXTRA_CFLAGS=-Werror   # treat warnings as errors (propagates to sub-makes)
make examples            # build all C + C++ examples (examples/{c,cpp}/**)
make run-example EX=<lang>/<group>/<name>   # build+run one example; dispatches
                  # on the prefix: c/ cpp/ compile, rust/ via cargo, python/ via
                  # the cffi package. E.g. EX=c/output/panel_alert. The full
                  # cross-language catalogue is in docs/examples.md.
make rust / rust-test / rust-lint   # build / test / clippy the safe Rust crate (bindings/rust/)
make python / python-test # build / test the Python package (bindings/python/)
make python-test-debug    # Python suite with poisoned freed memory: makes
                  # use-after-free of cffi buffers deterministic (run after
                  # binding / string-lifetime changes)
make rebuild-all          # C lib + install + Rust + Python in one command
make clean        # removes build trees, .a, shared libs, test binaries
```

Compiler: `cc -std=c11 -Wall -Wextra -Wshadow -Wformat=2 -Wnull-dereference -Wcast-align` plus hardening flags (`-O2 -fstack-protector-strong -D_FORTIFY_SOURCE=2`; RELRO/noexecstack linker flags on Linux). Sanitizer builds undefine `_FORTIFY_SOURCE` (it bypasses their interceptors). The build tracks header dependencies (`-MMD -MP`), so editing a header rebuilds dependents without `make clean`. Golden-file tests (`*-check`) diff rendered output against committed `expected.txt`; see `docs/DEVELOPMENT.md` for the full workflow.

Besides the C library, sparcli ships a header-only **C++ wrapper** (`include/sparcli.hpp`), a safe **Rust** crate (`bindings/rust/`), a Pythonic **Python** package (`bindings/python/`, cffi API-mode) and a **command-line tool** (`cli/`, see below). The Rust and Python wrappers compile the C sources themselves, so they need no prior `make`/install. After changing the C API, rebuild each consumer you use (and update the Python `cdef` / regenerate the Rust bindgen output for new/changed symbols) – see the "Rebuilding the bindings & consumers" section in `docs/DEVELOPMENT.md` and the per-language references `docs/api-{cpp,rust,python}.md`.

### Directory layout

Sources are grouped by concern. The **output/input boundary is physical**: `src/output` (and `core`) is stream-oriented and writes through the redirectable `sc_output_stream()`; `src/tty` + `src/input` are tty-oriented and drive a real terminal in raw mode (never `sc_output_stream`).

```
src/core/     color, text, print, output(stream), render_wrap, sanitize, version
src/output/   panel, rule, list, tree, columns, kv, alert, badge,
              progressbar, spinner, live, markup, pad, util, pager, + table/
src/tty/      term (raw mode + signal restore), key (decoder), screen (redraw)
src/input/    prompt (loop engine), line_editor, shortcut, editor (external),
              theme, confirm, text_input, password_input, number_input,
              calc (expression evaluator), textarea, select, fuzzy, datepicker,
              history (Up/Down recall + persistence)
src/app/      application-framework helpers: paths (XDG dirs), error (sc_die)
src/log/      logging: leveled terminal + plain-text file sinks
src/args/     argument parser: builder, parse loop, typed values, help,
              did-you-mean, zsh completion generation, line tokenizer (split)
cli/          the sparcli command-line tool (main + cli_* helpers + cmd_* files)
completions/  zsh completion (_sparcli) for the CLI
include/core/    include/output/    include/input/    include/app/
                 (sparcli.h stays at root)
tests/output/    tests/input/logic/ (interactive)   tests/input/style/ (snapshots)
tests/app/       framework suite (paths, pager, errors, logging)
tests/args/      argument-parser suite (parse, errors, help, completion)
tests/cli/       CLI golden-file suite (run_output.sh) + CLI PTY suite
```

Public headers live in `include/{core,output,input,app,log,args}/`; cross-includes use **root-relative paths** (`#include "core/sparcli_core.h"`), resolved via `-Iinclude`. `sparcli.h` is the full umbrella; `input/sparcli_input.h` (input widgets) and `app/sparcli_app.h` (framework helpers) are sub-umbrellas. `#include <sparcli.h>` is unchanged for users; only direct single-header includes moved.

When adding a source file, append its path (e.g. `src/output/foo.c`) to `SRC` in the Makefile. The build tree mirrors `src/` automatically. CLI sources go into `CLI_SRC` instead (they compile with `-Iinclude` only – the CLI never includes `src/` internals).

### Command-line tool (`cli/`)

The `sparcli` binary exposes every widget as a shell subcommand (rich-cli + gum in one tool); full reference in `docs/cli.md`. Key facts:

- **Structure:** `cli/main.c` (dispatch table + global flags) → `cli/cmd_*.c` (one file per command group: output, layout, table, tree, progress, input, select) over shared helpers `cli/cli_common.c` (ctx, errors, capture), `cli_parse.c` (string→enum/color lookup tables), `cli_stdin.c` (file-or-stdin), `cli_csv.c` (RFC-4180-ish parser). Linked against the static `libsparcli.a`, public headers only.
- **Conventions:** markup parsed by default in all text (`--no-markup` opt-out); `--no-color`/`NO_COLOR` strips ANSI by rendering through a capture stream + `sc_strip_ansi`; input text is sanitized by default (`--allow-ansi` opt-out → `sc_set_allow_ansi`); exit codes 0 = OK / 1 = cancelled or "no" / 2 = error or no TTY.
- **Input commands** set `hide_summary`, print only the raw value to stdout (the widget UI goes to `/dev/tty`), so `$(sparcli input …)` command substitution works; `--accent` is applied via `sc_input_set_theme`.
- **`spin`** forks the wrapped command and routes the spinner to `/dev/tty` (the spinner is a stream widget – it must not pollute the child's stdout); the child's exit code is propagated.
- **Tests:** `make test-cli-check` (golden, `tests/cli/run_output.sh` + `expected.txt`) and `make test-cli-pty` (`tests/cli/test_cli_pty.c`: forkpty + stdout-pipe redirect, runs the ASan/UBSan `sparcli-sanitize` binary). Both are part of `make test`. New output cases go into `run_output.sh` (+ `make test-cli-golden`), new input cases into the `CASES[]` array.
- **Install:** `make install` puts the binary in `$(BINDIR)` and `completions/_sparcli` in `$(ZSHFUNCDIR)`.
- **Future migration (deliberately deferred):** the CLI still uses `getopt_long` + hand-written usage strings/completion - NOT the `src/args/` parser. Migrating it is a roadmap item (see README), postponed until the args API has stabilized through real-world use, so that args API changes do not force follow-up changes in the stable, golden-tested CLI. Do not propose this migration as new work.

---

## Core Types

### ScColor

```c
typedef struct { int index; uint8_t r, g, b; } ScColor;
```

| `index` | Meaning |
|---------|---------|
| `0`     | **Not set** – `SC_ANSI_COLOR_NONE`; no escape code emitted. **Zero-init `ScColor` lands here**, so any unset color field is "no color" by default. |
| `-1`    | 24-bit RGB mode; uses `r`, `g`, `b` fields |
| `1`–`8` | Named ANSI color (`SC_ANSI_COLOR_BLACK` … `SC_ANSI_COLOR_WHITE`) |

Zero-init friendly: `(ScColor){0}` ≡ `SC_ANSI_COLOR_NONE`, so leaving any color field unset emits no escape codes. Construct explicit black with `SC_ANSI_COLOR_BLACK`; RGB with `sc_color_from_rgb(...)`.

```c
ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);  // index = -1
```

Named macros: `SC_ANSI_COLOR_NONE`, `SC_ANSI_COLOR_BLACK`, `SC_ANSI_COLOR_RED`, `SC_ANSI_COLOR_GREEN`, `SC_ANSI_COLOR_YELLOW`, `SC_ANSI_COLOR_BLUE`, `SC_ANSI_COLOR_MAGENTA`, `SC_ANSI_COLOR_CYAN`, `SC_ANSI_COLOR_WHITE`.

### ScTextAttribute

Bitmask – combine with `|`:

```c
SC_TEXT_ATTR_NONE | SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_DIM | SC_TEXT_ATTR_ITALIC | SC_TEXT_ATTR_UNDER
```

### ScTextStyle

```c
typedef struct { ScTextAttribute attr; ScColor fg; ScColor bg; } ScTextStyle;
```

Used everywhere a styled text span is needed (cell headers, titles, rule labels, …).

### ScText / ScSpan

Multi-span rich text. Each span has its own `ScTextStyle`.

```c
ScText *t = sc_text_new();
sc_text_append(t, "hello ",  (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
sc_text_append(t, "world",   (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE });
sc_print_text(t);
sc_text_free(t);
```

`sc_text_visible_width(t)` – returns the max visible width across lines (ANSI-aware, UTF-8-aware, counts codepoints not bytes).

`sc_text_append_link(t, text, url, style)` – appends a span wrapped in an **OSC-8 terminal hyperlink** (clickable in supporting terminals, plain text elsewhere). The URL is scrubbed to printable ASCII (no ESC/control bytes), the visible text is sanitized normally, and the escape bytes occupy zero visible columns. Markup equivalent: `[link=URL]text[/link]`.

### ScBorderType

```c
SC_BORDER_NONE | SC_BORDER_ASCII | SC_BORDER_SINGLE |
SC_BORDER_DOUBLE | SC_BORDER_ROUNDED | SC_BORDER_THICK
```

Used by panels, tables, rules, and column separators.

### ScBoxStyle

Shared "framed box" styling embedded as the `box` field in **every** input
widget's opts and in `ScInputTheme`. Zero-init = no frame (inline render).

```c
typedef struct {
    bool          enabled;     /* draw a default (rounded) border */
    ScBorderStyle border;      /* type/color/bg; NONE+enabled → rounded, else borderless */
    ScColor       bg;          /* content background; fills behind rows (rows inherit it) */
    ScEdges       padding;     /* inner padding; all-zero = default (1 left/right) */
    ScEdges       margin;      /* outer margin; zero-init = none */
    ScWidthMode   width_mode;  /* DEFAULT(0)/CONTENT/FIXED/FULL */
    int           width;       /* FIXED width (and DEFAULT when >0) */
    int           min_width;   /* CONTENT clamp; 0 = none */
    int           max_width;   /* CONTENT clamp; 0 = none */
    ScBgExtent    bg_extent;   /* WIDGET(0, fill to width) / TEXT (hug text) */
} ScBoxStyle;
```

Used by `sc_confirm`, `sc_text_input`, `sc_password_input`, `sc_number_input`,
`sc_textarea`, `sc_select`, `sc_fuzzy`, `sc_datepicker` (each `Sc*Opts.box`).
The text-entry widgets route the prompt to the panel **title** (counter/range
on the bottom border); the list/choice widgets (`select`, `fuzzy`, `confirm`,
`datepicker`) keep the prompt inside the frame. The key-hint footer always sits
**outside** the frame.

**Border, background and width are independent.** `enabled` only requests a
default border; a `bg`, `padding` or non-default `width`/`width_mode` alone
routes the widget through a **borderless** panel (background/width fill). The
background fills each line to the resolved width, so list rows without their own
background inherit it (`SC_BG_EXTENT_WIDGET`); `SC_BG_EXTENT_TEXT` keeps it
hugging the text. **Width modes** (`ScWidthMode`): `DEFAULT` = per-widget (lists
fit content, text-entry span full width; a non-zero `width` means fixed);
`CONTENT` = fit content clamped to `[min_width, max_width]`; `FIXED` = `width`
columns; `FULL` = terminal width.

**select/fuzzy** additionally extend the **cursor row into a full-width highlight
bar** (padded to the interior width) when its `selected_style.bg` is set and
`bg_extent == WIDGET` - other rows inherit the panel `bg`. Resolution lives in
`sc_box_layout` (+ `sc_pad_line_to`) in `input_internal.h`; the box auto-fits the
content (`CONTENT`) so the frame matches the list, not the terminal.

**Migration notes:** `ScBoxStyle` replaced the older flat `boxed`/`border`/
`width` fields on the four text-entry widgets and the theme's flat `border`; and
the boxed default width for **list widgets** changed from full-terminal to
**content** width.

### ScTitle

```c
typedef struct {
    const char      *text;  /* NULL = no title */
    const ScText    *rich_text; /* optional rich title; overrides text+style (panels only) */
    ScTextStyle      style; /* text rendering (bold, color, …) */
    ScHAlign         halign; /* LEFT / CENTER / RIGHT */
    int              pad;   /* spaces on each side of the title text */
    ScPosition  pos;   /* SC_POSITION_TOP / SC_POSITION_BOTTOM; unused for rules */
} ScTitle;
```

Used directly everywhere a title is needed. `pos` is ignored by rules (no top/bottom distinction). Access paths:
- `rule_opts.title.text` / `.style` / `.halign` / `.pad`
- `panel_opts.title.text` / `.style` / `.halign` / `.pad` / `.pos`

---

### ScAnsiMode

Per-widget ANSI passthrough for user strings – controls the escape-injection protection.

```c
typedef enum {
    SC_ANSI_MODE_DEFAULT  = 0, /* inherit the sc_set_allow_ansi() global (default) */
    SC_ANSI_MODE_ALLOW    = 1, /* pass well-formed escape sequences through */
    SC_ANSI_MODE_SANITIZE = 2, /* always strip escape sequences */
} ScAnsiMode;

void sc_set_allow_ansi(bool allow);  /* process-wide default; false = strip */
bool sc_allow_ansi(void);            /* current setting */
```

By default every user string entering the library is sanitized: control bytes (except `\n`/`\t`) and ANSI escape sequences are removed, so untrusted data cannot inject terminal escape codes. Every output widget opts struct (`ScPanelOpts`, `ScTableOpts`, `ScRuleOpts`, `ScListOpts`, `ScKVOpts`, `ScTreeOpts`, `ScSpinnerOpts`, `ScProgressBarOpts`, `ScBadgeOpts`, `ScMarkupOpts`) carries an `ScAnsiMode ansi` field; zero-init inherits the `sc_set_allow_ansi` global, the explicit values override it per widget. When ANSI is allowed, well-formed escape sequences pass through, stray control bytes are still removed, and all width calculations skip escape sequences – frames and tables stay aligned. Details: see the "ANSI sanitization / trust boundary" Key Invariant.

---

## Panels

```c
void sc_panel_str (const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);
```

### ScPanelOpts

| Field | Type | Description |
|-------|------|-------------|
| `border` | `ScBorderStyle` | Frame style, color, bg |
| `bg` | `ScColor` | Content area background color; zero-init `{0}` = no color |
| `title` | `ScTitle` | Title text, style, and position |
| `title.text` | `const char *` | NULL = no title |
| `title.style` | `ScTextStyle` | Text style (bold, color, …) |
| `title.halign` | `ScHAlign` | LEFT / CENTER / RIGHT |
| `title.pad` | `int` | Spaces on each side of the title text |
| `title.pos` | `ScPosition` | `SC_POSITION_TOP` / `SC_POSITION_BOTTOM` |
| `subtitle` | `ScTitle` | Optional second title on the edge given by its own `.pos`; zero-init (`text == NULL`) = none. Use for the opposite edge from `title` (e.g. top title + bottom-right caption). |
| `padding` | `ScEdges` | Inner content padding (top/right/bottom/left) |
| `margin` | `ScEdges` | Outer margin |
| `width` | `int` | 0 = auto-fit content |
| `content_align` | `ScHAlign` | Horizontal alignment of content lines |
| `full_width` | `bool` | Stretch to terminal width (overrides `width`) |
| `ansi` | `ScAnsiMode` | ANSI passthrough for content and title strings; zero-init inherits `sc_set_allow_ansi` |

`full_width` takes precedence over `width`. Width is calculated as `terminal_width - 2`.

**Background color sentinel:** `border_bg` and `bg` use `{0,0,0,0}` (zero-init) as "not set" – the same sentinel as `ScProgressBarOpts.fill_color`. A zero-initialized `ScPanelOpts` field means no background. Use `SC_ANSI_COLOR_NONE` or leave unset for no color; use `sc_color_from_rgb(...)` for a specific color.

---

## Tables

Table sources live in `src/table/` and are chained via `#include`: `table.c` → `table_print.c` → `table_print_init.c` → `table_print_render.c` → `table_print_render_cell.c`, `table_print_render_border.c`, `table_print_render_row.c`. Internal types are declared in `src/table/table_internal.h`. Only `src/table/table.c` appears in the Makefile `SRC`; the rest are included implicitly.

```c
ScTableData *sc_table_new(void);
void         sc_table_add_column(ScTableData *table, const char *header, ScColOpts col);
void         sc_table_add_row(ScTableData *table, ScCell *cells, size_t n);
void         sc_table_add_row_bg(ScTableData *table, ScCell *cells, size_t n, ScColor bg);
void         sc_table_add_footer_row(ScTableData *table, ScCell *cells, size_t n);
void         sc_table_print(const ScTableData *table, ScTableOpts opts);
void         sc_table_free(ScTableData *table);
```

`ScTableOpts` is passed at **print time**, not at construction – `sc_table_new()` takes no arguments. The same `ScTableData *` can be printed multiple times with different opts.

### ScTableOpts

| Field | Description |
|-------|-------------|
| `border` | `ScTableBorder` – style + color per border segment |
| `header` | `ScTableHeader` – grouped header settings (see below) |
| `header.row` | `bool` – first added row is the header |
| `header.col` | `bool` – first column is a header column |
| `header.row_bg` / `header.col_bg` | Background for header areas |
| `header.style` | `ScTextStyle` applied to all header cells |
| `striped` | `bool` – alternating row backgrounds |
| `stripe_bg` | Background for odd data rows (0-indexed) |
| `footer` | `ScTableFooter` – grouped footer settings (see below) |
| `footer.row_bg` / `footer.col_bg` | Background for footer rows/column |
| `footer.style` | `ScTextStyle` for footer cells |
| `title` | `ScTitle` – table title |
| `title.text` | Title string; `NULL` = no title |
| `title.style` / `title.halign` / `title.pad` / `title.pos` | Title text appearance and position |
| `cell_pad` | `ScEdges` – inner cell padding |
| `margin` | `ScEdges` – outer table margin (top/right/bottom/left) |
| `total_width` | 0 = auto; >0 = distribute width across flex columns |
| `max_rows` | 0 = unlimited; >0 = truncate with indicator |
| `rtl` | `bool` – right-to-left column order |
| `ansi` | `ScAnsiMode` – ANSI passthrough for cell and title strings; zero-init inherits the `sc_set_allow_ansi` global |

### ScTableHeader / ScTableFooter

```c
typedef struct { bool row; bool col; ScColor row_bg; ScColor col_bg; ScTextStyle style; } ScTableHeader;
typedef struct { ScColor row_bg; ScColor col_bg; ScTextStyle style; } ScTableFooter;
```

### ScTableBorder

```c
typedef struct {
    ScBorderType type;
    ScColor       outer_color;
    ScColor       inner_color;
    ScColor       header_row_sep_color;
    ScColor       header_col_sep_color;
    bool          no_outer;    /* suppress outer frame */
    bool          no_inner_h;  /* suppress inner row separators */
    bool          no_inner_v;  /* suppress inner col separators (except header col) */
} ScTableBorder;
```

### ScColOpts

```c
typedef struct {
    int      min_width;
    int      max_width;
    int      fixed_width;
    ScHAlign  halign;
    ScVAlign valign;
    bool     word_wrap;  /* true = word-wrap, false = truncate */
    ScColor  bg;         /* SC_ANSI_COLOR_NONE = not set */
    ScTextStyle style;   /* default text style for unstyled cells; zero-init = none */
} ScColOpts;
```

**Background priority:** `header/footer bg > per-row bg > stripe bg > col bg`. Per-column bg is the lowest-priority fallback: only applied when no row-level background is active (`row_bg.index == 0`).

**Per-column text style (`style`):** default attributes + foreground for cell spans that carry no styling of their own. Priority: per-cell/markup style > `header.style`/`footer.style` (section) > column `style`; the fallback is per-field (a bold-only header still picks up the column fg). The style's `bg` member is ignored – use the `bg` field for backgrounds.

### ScCell constructors

Cells are built with `static inline` helpers (**not** macros). Native C uses the terse inline forms; FFI bindings that cannot consume `static inline` functions or C99 compound literals call the exported descriptive variants instead.

| Inline helper | FFI variant | Description |
|---------------|-------------|-------------|
| `sc_cell(s)` | `sc_cell_from_str(s)` | Plain string cell |
| `sc_cell_a(s, ha, va)` | `sc_cell_str_aligned(s, ha, va)` | String + h/v alignment |
| `sc_cell_t(t)` | `sc_cell_from_text(t)` | ScText cell (not owned) |
| `sc_cell_ta(t, ha, va)` | `sc_cell_text_aligned(t, ha, va)` | ScText + alignment |
| `sc_cell_cs(s, cs)` | `sc_cell_colspan(s, cs)` | String + colspan |
| `sc_cell_csa(s, cs, ha)` | – | String + colspan + halign |
| `sc_cell_tcs(t, cs)` | – | ScText + colspan |
| `sc_cell_tcsa(t, cs, ha)` | – | ScText + colspan + halign |
| `sc_cell_skip()` | `sc_cell_skip_placeholder()` | Placeholder covered by a colspan |
| `sc_cell_rs(s, rs)` | `sc_cell_rowspan(s, rs)` | String + rowspan |
| `sc_cell_trs(t, rs)` | – | ScText + rowspan |
| `sc_row_skip()` | `sc_row_skip_placeholder()` | Placeholder row covered by a rowspan |
| `sc_cell_m(s)` | `sc_cell_from_markup(s)` | Markup cell (owns the parsed ScText) |

Colspan: use `sc_cell_cs(s, n)` (sets `col_span = n`) on the spanning cell; fill the remaining positions with `sc_cell_skip()`. Rowspan: use `sc_cell_rs(s, n)` (sets `row_span = n`); fill the covered rows with `sc_row_skip()` at the corresponding column position.

---

## Rule

Horizontal line across the terminal (or a fixed width).

```c
void sc_rule_str (const char *title, ScRuleOpts opts);  // title may be NULL
void sc_rule_text(const ScText *title, ScRuleOpts opts); // title may be NULL
```

### ScRuleOpts

| Field | Description |
|-------|-------------|
| `type` | `ScBorderType` – which `h` character to use |
| `color` | Line color; `SC_ANSI_COLOR_NONE` = no escape codes |
| `title` | `ScTitle` – title label (text, style, halign, pad; pos ignored) |
| `title.text` | Title string; `NULL` = no title |
| `title.style` | `ScTextStyle` for the title text |
| `title.halign` | LEFT / CENTER / RIGHT (default CENTER) |
| `title.pad` | Spaces on each side of title, default 1 |
| `width` | 0 = full terminal width; >0 = fixed width |
| `halign` | Placement of the rule when `width > 0` (LEFT/CENTER/RIGHT) |
| `margin` | `ScEdges` – top/bottom = blank lines; left/right = indent |
| `ansi` | `ScAnsiMode` – ANSI passthrough for the title string; zero-init inherits the `sc_set_allow_ansi` global |

---

## Columns

Renders multiple widgets side by side using a capture-and-replay approach: each widget is rendered into a `tmpfile()` via `dup2`, then lines are zipped horizontally.

```c
ScColumns *sc_columns_new(ScColumnsOpts opts);
void sc_columns_add_table     (ScColumns *cl, const ScTableData *t, ScTableOpts opts, ScColItem item);
void sc_columns_add_panel_str (ScColumns *cl, const char      *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_panel_text(ScColumns *cl, const ScText    *content, ScPanelOpts opts, ScColItem item);
void sc_columns_add_text      (ScColumns *cl, const ScText    *t,       ScColItem item);
void sc_columns_add_str       (ScColumns *cl, const char      *s,       ScColItem item);
void sc_columns_add_columns   (ScColumns *cl, const ScColumns *nested,  ScColItem item);
void sc_columns_add_tree      (ScColumns *cl, const ScTree    *tree,    ScColItem item);
void sc_columns_add_list      (ScColumns *cl, const ScList    *list,    ScColItem item);
void sc_columns_print(const ScColumns *cl);
void sc_columns_free(ScColumns *cl);
```

Nested columns are captured eagerly at `sc_columns_add_columns` call time.

### ScColumnsOpts

| Field | Description |
|-------|-------------|
| `gap` | Space between columns. **Without separator:** gap spaces total. **With separator:** gap spaces on each side of the separator (total: 2×gap+1). Default: 3 (no sep) or 2 (with sep). |
| `sep` | `ScBorderStyle` – vertical separator bundle (`type`, `color`, `bg`); `sep.type = SC_BORDER_NONE` = no separator |
| `sep.type` | `ScBorderType` for the separator character |
| `sep.color` | Separator foreground color; **must be set to `SC_ANSI_COLOR_NONE`** if no color desired |
| `sep.bg` | Background color applied to gap spaces and the separator char; zero-init = none |
| `valign` | `SC_VALIGN_TOP` / `SC_VALIGN_MIDDLE` / `SC_VALIGN_BOTTOM` |
| `total_width` | 0 = auto; >0 = distribute across flex columns |

### ScColItem

Per-column options passed with each `sc_columns_add_*` call:

```c
typedef struct {
    int      min_w;      // 0 = none
    int      max_w;      // 0 = none
    int      fixed_w;    // 0 = auto; fixed_w > 0 ignores min_w/max_w
    ScHAlign  halign;      // content placement when col_w > content_w
    int      valign_set; // 1 = override ScColumnsOpts.valign for this column
    ScVAlign valign;     // per-column vertical alignment
    ScColor  bg;         // background for padding spaces and empty slots; zero-init = no color
} ScColItem;
```

Width priority: `fixed_w` > `min_w`/`max_w` clamping > natural content width. Flex columns (fixed_w == 0) participate in `total_width` distribution.

**Per-column valign:** Set `valign_set = 1` and `valign` to override the global `ScColumnsOpts.valign` for a single column. Zero-initialized `ScColItem` inherits the global setting (no override).

**Per-column background (`bg`):** Applied to padding spaces (lp/rp alignment padding) and empty slots when the column has fewer lines than the tallest column. Uses the zero-init sentinel (`{0,0,0,0}` = not set), so `(ScColItem){ 0 }` works naturally with no background. Does not affect the captured widget content itself (which already has its own embedded ANSI codes).

```c
sc_columns_add_text(cols, t2, (ScColItem){ .valign_set = 1, .valign = SC_VALIGN_TOP    });
sc_columns_add_text(cols, t3, (ScColItem){ .valign_set = 1, .valign = SC_VALIGN_MIDDLE });
sc_columns_add_text(cols, t4, (ScColItem){ .valign_set = 1, .valign = SC_VALIGN_BOTTOM });
```

---

## List

Bulleted and numbered lists with word-wrap, hanging indent, and arbitrary nesting.

```c
ScList     *sc_list_new     (ScListOpts opts);
ScListItem *sc_list_add_str (ScList *l, const char *str, ScTextStyle opts);
ScListItem *sc_list_add_text(ScList *l, const ScText *t);
ScList     *sc_list_add_sub (ScListItem *parent, ScListOpts opts);
void        sc_list_print   (const ScList *l);
void        sc_list_free    (ScList *l);
```

`sc_list_add_sub` attaches a sub-list to an item; the sub-list is owned by the item and freed when the parent list is freed.

### ScListMarker

| Constant | Style |
|----------|-------|
| `SC_LIST_BULLET` | Fixed character (default `•`) |
| `SC_LIST_NUMBER` | `1.` `2.` `3.` … |
| `SC_LIST_ALPHA_LC` | `a.` `b.` `c.` … |
| `SC_LIST_ALPHA_UC` | `A.` `B.` `C.` … |
| `SC_LIST_ROMAN_LC` | `i.` `ii.` `iii.` … |
| `SC_LIST_ROMAN_UC` | `I.` `II.` `III.` … |

### ScListOpts

| Field | Description |
|-------|-------------|
| `marker` | Marker type (see above) |
| `bullet` | `SC_LIST_BULLET` only; `NULL` = default `•` |
| `marker_prefix` | Text before the value, e.g. `"("` → `(1`; default `""` |
| `marker_suffix` | Text after the value, e.g. `"."` → `1.`; default `"."` |
| `marker_style` | Style/color for the marker; **zero-init = no formatting** (see below) |
| `indent` | Left indent relative to parent, in columns |
| `item_gap` | Blank lines between items |
| `width` | 0 = terminal width |
| `margin` | Symmetric left+right outer margin |
| `ansi` | `ScAnsiMode` – ANSI passthrough for item strings; zero-init inherits the `sc_set_allow_ansi` global |

**Zero-init of `marker_style`:** Unlike other `ScTextStyle` fields, a zero-initialized `marker_style` in `ScListOpts` is explicitly treated as "no formatting" by the renderer. You can safely write `(ScListOpts){ .marker = SC_LIST_NUMBER }` without specifying `marker_style` and no color escape codes will be emitted for the marker.

**Alignment:** Numbered/alpha/roman markers are right-aligned within a field sized to the widest marker value in the list (e.g. `VIII.` sets the field width for all items).

**Hanging indent:** Continuation lines of a word-wrapped item are indented to the same column as the start of the text (i.e., aligned under the first word, not the marker).

**Nesting:** Sub-lists start at `text_start` of the parent item. Each level's `indent` adds further indentation relative to that base.

**Integration with ScColumns:** use `sc_columns_add_list(cl, list, item)`.

---

## Progress Bar

Animated in-place progress bar using `\r` overwrite. Stateful: allocate once, call `_draw` in a loop, call `_finish` to end.

```c
ScProgressBar *sc_progressbar_new      (ScProgressBarOpts opts);
void           sc_progressbar_set_label(ScProgressBar *b, const char *label);
void           sc_progressbar_draw     (ScProgressBar *b, double value, double max);
void           sc_progressbar_finish   (ScProgressBar *b, double value, double max);
void           sc_progressbar_free     (ScProgressBar *b);
```

`value`/`max` convention: if `max > 0`, ratio = value/max; if `max == 0`, value is already a 0.0–1.0 ratio. `show_value` only takes effect when `max > 0`.

### ScProgressType

| Constant | Appearance |
|----------|-----------|
| `SC_PROGRESS_BLOCK` | `█` / `░` |
| `SC_PROGRESS_ASCII` | `=` + `>` edge / ` ` |
| `SC_PROGRESS_LINE` | `━` / `╌` |
| `SC_PROGRESS_SHADED` | `▓` / `▒` edge / `░` |

### ScProgressBarOpts

| Field | Description |
|-------|-------------|
| `type` | Fill style (see above) |
| `left_cap` / `right_cap` | Border strings; `NULL` = no bracket; pass `"["` / `"]"` for defaults |
| `fill_color` / `empty_color` | Colors; zero-init = no color (same sentinel as `marker_style`) |
| `thresholds` | `ScProgressThresholds` – grouped threshold settings (see below) |
| `thresholds.enabled` | `bool` – switch fill color based on ratio |
| `thresholds.mid` / `thresholds.high` | Ratio thresholds (default 0.5 / 0.75) |
| `thresholds.color_low` / `.color_mid` / `.color_high` | Fill color per range |
| `show_percent` | `bool` – append ` XX%` (default true) |
| `show_value` | `bool` – append `(value/max)` after percent |
| `bar_width` | Inner bar char count; 0 = auto from `width` |
| `width` | Total line width; 0 = terminal width |
| `label_width` | Fixed label column width; 0 = natural width |
| `label_style` | Style for label text |
| `ansi` | `ScAnsiMode` – ANSI passthrough for the label; zero-init inherits the `sc_set_allow_ansi` global |

**Zero-init of `fill_color`/`empty_color`:** Same as every other `ScColor` field – zero-init equals `SC_ANSI_COLOR_NONE` and emits no escape codes. Use `SC_ANSI_COLOR_BLACK` for explicit black.

**Animation pattern:**
```c
ScProgressBar *b = sc_progressbar_new(opts);
sc_progressbar_set_label(b, "Installing");
for (int v = 0; v <= 100; v++) {
    sc_progressbar_draw(b, (double)v, 100.0);
    usleep(50000);
}
sc_progressbar_finish(b, 100.0, 100.0);
sc_progressbar_free(b);
```

---

## Spinner

Animated in-place spinner using `\r` overwrite. Label is updateable mid-animation.

```c
ScSpinner *sc_spinner_new      (const char *label, ScSpinnerOpts opts);
void       sc_spinner_set_label(ScSpinner *s, const char *label);
void       sc_spinner_tick     (ScSpinner *s);
void       sc_spinner_finish   (ScSpinner *s, bool success, const char *label);
void       sc_spinner_free     (ScSpinner *s);
```

`sc_spinner_tick` advances to the next frame, prints `frame label\r`, and calls `fflush`. `sc_spinner_finish` clears the line, then prints `✔ label\n` (success=true) or `✖ label\n` (success=false) in green/red.

### ScSpinnerType

| Constant | Frames |
|----------|--------|
| `SC_SPINNER_BRAILLE` | `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏` (10 frames) |
| `SC_SPINNER_PIPE` | `\|/-\` (4 frames) |
| `SC_SPINNER_DOTS` | `⣾⣽⣻⢿⡿⣟⣯⣷` (8 frames) |
| `SC_SPINNER_ARROW` | `←↖↑↗→↘↓↙` (8 frames) |

### ScSpinnerOpts

| Field | Description |
|-------|-------------|
| `type` | Frame style (see above) |
| `color` | Spinner character color; zero-init = no color |
| `label_style` | Style for label text; zero-init = no formatting |
| `ansi` | `ScAnsiMode` – ANSI passthrough for the label; zero-init inherits the `sc_set_allow_ansi` global |

**Animation pattern:**
```c
ScSpinner *s = sc_spinner_new("Loading...", opts);
for (int i = 0; i < 30; i++) { sc_spinner_tick(s); usleep(80000); }
sc_spinner_set_label(s, "Fetching...");
for (int i = 0; i < 20; i++) { sc_spinner_tick(s); usleep(80000); }
sc_spinner_finish(s, 1, "Done");
sc_spinner_free(s);
```

---

## Input Widgets

Interactive prompts. **Tty-oriented**, not stream-oriented: they open `/dev/tty` (fallback stdin/stdout), enter raw mode, read decoded keys, and redraw in place – they do **not** go through `sc_output_stream()`. Every widget returns `ScInputStatus` (`SC_INPUT_OK` / `SC_INPUT_CANCELLED` / `SC_INPUT_ERROR`); Esc and Ctrl-C always cancel; non-TTY contexts return `SC_INPUT_ERROR` (so callers can fall back to a default). On `SC_INPUT_OK` the interactive region is erased and a one-line summary is printed in its place. Header: `input/sparcli_input.h` (in the `sparcli.h` umbrella).

### Architecture

All widgets are thin state machines over one engine, `sc_prompt_run` (`src/input/prompt.c`), which owns the raw-mode + draw/read/dispatch loop. Each widget supplies an `ScPromptVTable { render, on_key }`; `render` builds a frame as an `ScText` and `sc_capture_text`s it – so input reuses the entire output renderer stack as its view layer. Shared pieces: `ScLineEditor` (`line_editor.c`, UTF-8 cursor/insert/delete/word-kill) backs text/password inputs and the fuzzy query; the TTY layer (`src/tty/`) provides raw mode + signal-safe restore (`term.c`), the escape-sequence decoder (`key.c`), and the multi-line in-place redraw (`screen.c`).

### Key decoding

`sc_key_decode(buf, len, &key)` is a **pure, unit-tested** decoder (`ScKey { type, codepoint, bytes }`) handling control bytes, CSI/SS3 sequences (arrows, Home/End, Delete, PageUp/Down, Shift+PageUp/Down via the `;2` modifier, Shift-Tab) and multi-byte UTF-8. It returns 0 bytes consumed for incomplete prefixes (lone ESC, partial UTF-8/CSI); the buffered reader `sc_tty_read_key` resolves a lone ESC to `SC_KEY_ESC` via a 25 ms `select()` timeout (not `poll()` – `poll()` on `/dev/tty` is broken on macOS and would swallow a lone Esc until the next key). Unrecognized escape sequences (e.g. pasted ANSI color codes) decode to `SC_KEY_NONE` with all bytes consumed; `sc_tty_read_key` **skips them silently** – they neither cancel the prompt nor appear as text, and `SC_KEY_NONE` from the reader always means EOF/read error. The decoder also recognizes the bracketed-paste markers (`ESC[200~`/`ESC[201~` → `SC_KEY_PASTE_START`/`_END`); between them the reader switches to a literal-text decode (`decode_paste_content`) that flags every key with `SC_MOD_PASTED`.

### Terminal safety / robustness

The input side manipulates global terminal state, so `src/tty/term.c` is defensive about always handing back a usable terminal:

- **Restore-on-signal:** raw mode + cursor are restored on `SIGINT`/`SIGTERM`/ `SIGHUP`/`SIGQUIT` (then the signal is re-raised with the default handler) and via `atexit`. Crash signals (SIGSEGV/ABRT/…) are intentionally **not** trapped, so debuggers/sanitizers keep their handlers; `atexit` covers clean exits.
- **Resize:** `SIGWINCH` sets a flag; `sc_tty_read_key` returns `SC_KEY_RESIZE` (the blocking `read` is interrupted, `EINTR`), and `sc_prompt_run` clears and repaints so content reflows. Other `EINTR` retries transparently.
- **Height clamp:** `sc_screen_draw` never draws more lines than the terminal has rows, so a frame taller than the window can't scroll the screen and break the cursor-up arithmetic (widgets also bound their own height).
- **One prompt at a time:** `sc_prompt_run` atomically claims a single global TTY session (`atomic_bool`) and returns `SC_INPUT_ERROR` if one is already running – so a nested or cross-thread call fails safely instead of corrupting state. The key buffer is reset at `sc_tty_begin` so stale bytes never leak between prompts.
- **`SPARCLI_NO_TTY` override:** setting this env var to a non-empty value other than `0` forces the no-TTY behavior everywhere – `sc_input_available()` returns `false`, prompts return `SC_INPUT_ERROR` without opening `/dev/tty`, the pager is a no-op, and the live display buffers (helper: `sc_no_tty_override()` in `src/internal.h`, honored by `term.c`, `pager.c`, `live.c`). Needed because test runners (`cargo test`, `pytest`) only redirect stdin/stdout: the controlling terminal stays reachable, so without the override an interactively started binding test suite would let prompts grab the real keyboard and the pager spawn a blocking `less`. `make rust-test` / `make python-test` (+ the Python `conftest.py`) set it; logic test: `tests/input/logic/test_no_tty.c`.
- **Bracketed paste:** `sc_tty_begin` enables bracketed-paste mode (`ESC[?2004h`; disabled again on every restore path incl. signals). Pasted text arrives between `ESC[200~`/`ESC[201~` markers and is treated as **literal, sanitized text**: the reader delivers it as `SC_KEY_CHAR`+`SC_MOD_PASTED` (pasted `\r`/`\n` = `SC_KEY_ENTER`+PASTED, other control bytes/ESC sequences dropped). The prompt engine never matches pasted keys against shortcuts/editor keys; each widget declares a `ScPasteMode` in its vtable (`SC_PASTE_IGNORE` default – select/confirm/datepicker drop pastes entirely, `SC_PASTE_TEXT` – text/password/number/fuzzy insert chars but ignore pasted Enter, `SC_PASTE_MULTILINE` – textarea also keeps newlines). So a pasted `\r` can never submit and pasted "y" can never answer a confirm. Terminals without bracketed-paste support degrade to the previous behavior.
- **Coverage:** `make test-input-pty` drives every interactive widget over a PTY under ASan/UBSan (headless), complementing the pure-logic and snapshot suites.

### Widgets

```c
ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts);
ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts);     /* *out heap; free */
ScInputStatus sc_password_input(const char *prompt, char **out, ScPasswordOpts opts);  /* *out heap; free */
ScInputStatus sc_number_input(const char *prompt, double *out, ScNumberOpts opts);     /* min/max/step, ↑/↓; opts.out_text = exact text out */
ScInputStatus sc_textarea(const char *prompt, char **out, ScTextareaOpts opts);        /* multi-line; Ctrl-D submits */
ScInputStatus sc_datepicker(struct tm *io, ScDatePickerOpts opts);                     /* io in/out */

/* Opaque-handle widgets (variable items / per-run config) */
ScSelect *sc_select_new(ScSelectOpts opts);            /* opts.multi = true → checkboxes */
void      sc_select_add(ScSelect *s, const char *label);
ScInputStatus sc_select_run(ScSelect *s, size_t *indices, size_t *count_io); /* count_io in:cap out:written */
void      sc_select_free(ScSelect *s);

ScFuzzy  *sc_fuzzy_new(ScFuzzyOpts opts);              /* opts.table + headers/n_cols → table view */
void      sc_fuzzy_add(ScFuzzy *f, const char *label);
void      sc_fuzzy_add_row(ScFuzzy *f, const char *const *fields, size_t n); /* query searches all cols (opts.search_columns) */
ScInputStatus sc_fuzzy_run(ScFuzzy *f, size_t *out_index);  /* out_index = original add order */
void      sc_fuzzy_free(ScFuzzy *f);
bool      sc_fuzzy_match(const char *pattern, const char *str, int *score);  /* pure, testable */

/* Input history (storage handle; the recall navigation lives in text_input) */
ScHistory *sc_history_new(ScHistoryOpts opts);   /* .app → XDG state file, .file = explicit path; loads it */
void       sc_history_add(ScHistory *h, const char *line);  /* skips empty + consecutive duplicates */
size_t     sc_history_count(const ScHistory *h);
const char *sc_history_get(const ScHistory *h, size_t i);   /* 0 = oldest */
bool       sc_history_save(const ScHistory *h);  /* also automatic on free */
bool       sc_history_load(ScHistory *h);        /* replaces current entries */
void       sc_history_free(ScHistory *h);        /* saves + frees */
```

- **TextInput/Password** share `sc_text_entry` (`text_input.c`, configured via the internal `ScTextEntryCfg`); password is the masked variant (`opts.mask`, default `"*"`). Both accept an optional `validate` callback that keeps the prompt open and shows an error line. A character counter is shown under the field by default – `count` (no limit) or `count/max` when `max_chars > 0`, which also caps input. Hide it with `hide_char_count`; style it via `count_style` (default dim). Counts UTF-8 codepoints, not bytes. Set `boxed = true` to render inside a panel (prompt = top title, counter = bottom-right border via the panel `subtitle`); `border` sets the box style (zero-init type = rounded) and `width` the panel width (0 = full terminal width). A validation error stacks below the box via `sc_vstack`. Long values scroll horizontally within `width` with `‹`/`›` edge markers (line editor). Optional input constraints: `char_filter` (built-ins `sc_filter_digits`, `sc_filter_decimal`, `sc_filter_alpha`, `sc_filter_alnum`, `sc_filter_no_space`) rejects disallowed keystrokes; `suggestions`/`n_suggestions` show a dim autocomplete ghost that Tab accepts. The `suggest` field (`ScSuggestOpts`, text input only) switches the autocomplete presentation to a **dropdown** (`mode = SC_SUGGEST_DROPDOWN`): a list of matches below the field – ↑/↓ move the highlight (↑ past the first row returns to the field), Tab/Enter accept the highlighted row, Enter without highlight submits. Prefix or fuzzy matching (`match`, fuzzy ranks by `sc_fuzzy_match` score), `max_visible` rows (default 5) + dim "… +N more" overflow line, stylable rows (`item_style`/`selected_style` incl. background), optional `border` frame, custom `cursor_marker`/`marker`. Works inline and boxed (the dropdown stacks beneath the panel and matches the box width when framed); exact matches are excluded, so accepting a suggestion closes the list. Logic tests: `tests/input/logic/test_suggest.c`; style snapshots in `test_style_text.c`; PTY cases `suggest-dropdown-*`.
- **NumberInput** (`number_input.c`) reuses the line editor with a decimal filter; ↑/↓ step by `step`, value clamped to `[min, max]` when `max > min`, formatted to `decimals` places. `decimal_sep` (`0`/`'.'` = period, `','` = comma) controls the separator shown and accepted while editing; `out_text` (a `char **`) optionally receives the submitted value as an exact heap string – always `'.'`-normalized and clamp-consistent with `*out` – so callers can feed arbitrary-precision decimal types without float round-trips. `start_empty = true` seeds the editor empty instead of with the formatted `initial` (type the amount directly, no pre-fill to delete). Enter on an empty field is ignored, so an empty prompt never submits `0`. `boxed = true` renders inside a panel (range shown on the bottom-right border); `border`/`width` as for text input. `sc_filter_decimal` accepts both `.` and `,`. **Calculator mode** (`opts.calculator`): `=` as first char starts an expression (`=1,5+2*3`); dim live preview (`= 7,50` / `= ?`), Enter accepts the result into the field (rounded display, full-precision value pending), second Enter submits; invalid expressions raise a red `error_style` line and stay open; editing after accept discards the pending value. Default submits **full precision** (`%.17g` out_text) while displaying rounded – a documented exception to display==submit; `calc_store_rounded` submits the displayed value, `calc_show_precise` displays full precision. While a pending result differs from the rounded display, a dim ` = <exact>` **indicator** is shown next to the field; an edit that discards it (and thereby loses precision) raises a yellow **warning line** ("exact result discarded …", persists until submit/new accept) – text/style via `calc_warn_text`/`calc_warn_style`, never shown when nothing is lost or with `calc_store_rounded`. The evaluator is the pure public `sc_calc_eval` (`src/input/calc.c`: recursive descent, `+ - * /` + parens + single unary minus, `.`/`,` separators, div-by-zero/overflow/depth-capped = false; fuzzed by `fuzz_calc.c`). Tests: `tests/input/logic/test_calc.c`, PTY `calc-*` cases, style snapshots.
- **Textarea** (`textarea.c`, self-contained multi-line buffer): Enter inserts a newline, Ctrl-D submits, arrows move across lines/cols, Home/End within a line. Long logical lines **soft-wrap** to the field width (char-level; cursor stays correct; navigation remains logical-line based). `boxed = true` renders the editor inside a panel (prompt = top title, footer stacked below); `border`/ `width` as for text input.
- **Select** scrolls a viewport (`max_visible`, default 10); `j/k` + arrows move, Space toggles in multi-select. The cursor **cycles around the ends by default** (Up on the first row → last, Down on the last → first); `opts.no_cycle` stops at the ends instead. Pre-seed with `sc_select_set_cursor` / `sc_select_set_checked`. A dim `↑ first–last/total ↓` indicator shows when the list scrolls beyond the viewport.
- **Fuzzy** ranks by `sc_fuzzy_match` on each keystroke; matched characters are highlighted (bold+underline, accent fg) in the list and in the matched table cells (`render_table` builds those cells as highlighted `ScText` via `append_highlighted`, borrowed by the table and freed after capture); the query field scrolls horizontally (`‹`/`›`) when long; table view builds an `ScTableData` of the visible rows each frame (cursor row via row-bg) and `sc_vstack`s query + body + scroll-indicator + footer. `refilter` matches each row via `row_matches` across the columns selected by `opts.search_columns` (bitmask; `0` = all, the default), ranking by the best-scoring column – so a table query can hit (and highlight) any column, not just the first. The cursor cycles around the result-list ends by default; `opts.no_cycle` disables it.
- **DatePicker** renders a month grid; arrows move day/week, PageUp/Down or `<`/`>` change month, Shift+PageUp/Down change year; zeroed `struct tm` seeds today. Month/year jumps keep the selected day, clamped to the target month's last valid day (e.g. Jan 31 → Feb 28). `week_start` is `ScWeekStart`.
- **History** (`history.c` + `include/input/sparcli_history.h`): REPL-style input history for the text input. `ScHistory` is pure storage (handle pattern like `ScSelect`; opts strings copied); the ↑/↓ recall navigation lives in `text_input.c`. Attach via `ScTextInputOpts.history` (**borrowed** for the run, like `shortcuts`): ↑ recalls older entries (the in-progress line is preserved and restored by walking back down past the newest entry), typing/editing leaves the recall, the **dropdown keeps priority** over history on ↑/↓ while it shows matches. Submitted lines are **auto-added** unless `ScTextInputOpts.no_history_add` (per call) or `ScHistoryOpts.no_auto_add` (per handle) is set; empty lines and consecutive duplicates are always skipped (`keep_duplicates` opts them back in), the cap (`max_entries`, default 500) evicts oldest. Persistence: `.app` → `sc_path_file(SC_PATH_STATE, app, "history")`, `.file` = explicit path; plain text, one entry per line, sanitized on load (trust boundary); auto-load on new, auto-save on free. Tests: `tests/input/logic/test_history.c` (storage + sandboxed `$HOME` persistence), PTY cases `history-*` (recall/edit/restore/no-auto-add). Bindings: C++ `sparcli::History` (RAII, `apply(opts)`), Rust `sparcli::History` (Drop = save), Python `sc.History` (context manager, `len()`/`[i]`/`entries()`).
- **Key-hint footer:** every widget shows a dim footer (e.g. `↑/↓ move · enter select · esc cancel`) by default; override the text with `hint`, restyle with `hint_style`, and choose its layout with `hint_layout` (`ScHintLayout`: `SC_HINT_INLINE` one `·`-separated line / `SC_HINT_STACKED` one hint per line / `SC_HINT_HIDDEN` none; zero-init `SC_HINT_LAYOUT_DEFAULT` inherits the theme, then inline). Stacked is rendered by splitting the hint on ` · `. Its placement is `hint_pos` (`ScHintPosition`: `SC_HINT_POS_TOP`/`_BOTTOM`(default)/ `_LEFT`/`_RIGHT`; left/right sit beside the widget, top-aligned), orthogonal to layout. `sc_compose_hint` (input_internal.h) builds the hint block and places it: vstack for top/bottom, a 2-column `ScColumns` for left/right.
- **Theme:** `sc_input_set_theme(&(ScInputTheme){…})` sets process-wide defaults (accent, styles, markers, `box` framing, `hint_layout`) that every widget inherits for any zero-init option. The theme's `box` (`ScBoxStyle`) is merged **sub-field by sub-field** (`m_box` in `theme.c`), so a caller can override just the border and inherit the rest. Precedence: per-call opts > theme > built-in default. Applied via `sc_theme_apply_*` at each widget's entry (`theme.c`). The theme struct **and its glyph strings are copied** by `sc_input_set_theme` (caller buffers may be temporary); the copies are owned by the library and released on the next set/reset.
- **Custom shortcuts** (`input/sparcli_shortcut.h`): every widget's `Sc*Opts` carries `shortcuts` / `n_shortcuts` (borrowed `ScShortcut[]`) and an optional `out_shortcut_id`. The **prompt engine** (`prompt.c`) matches them before `on_key` – so one implementation covers all widgets – after the reserved cancel keys (Esc / Ctrl-C can't be bound). Build chords with `sc_key_ctrl('e')` / `sc_key_fn(2)` / `sc_key_alt('e')`. `SC_SHORTCUT_RETURN` ends the prompt and writes the fired `id` to `*out_shortcut_id` (`-1` = normal submit); the widget still returns its value. `SC_SHORTCUT_CALLBACK` runs `on_fire(id, user)` in place and stays open unless it returns `false`; `sc_select_cursor` / `sc_fuzzy_cursor_index` expose the live selection to such callbacks, and `sc_select_remove` / `sc_fuzzy_remove` delete the highlighted item live (the emptied-list run tails are guarded). A callback can't open a second prompt (single-session), so the "edit an item" pattern is a RETURN shortcut + `sc_select_label` / `sc_select_set_label` + `sc_select_set_cursor` in a caller re-run loop (see `examples/c/input/shortcuts_theme.c`). A shortcut with a `hint_label` is shown in a dim footer the **engine** stacks under every frame (key name via `sc_key_chord_name`, e.g. `^X delete`), discoverable on any widget with no per-widget code. The key decoder now also yields `SC_KEY_F1..F12`, Alt via an `ESC`-prefix (`ScKey.mods & SC_MOD_ALT`), and generic Ctrl-letters as `SC_KEY_CHAR` + `SC_MOD_CTRL`. The C++ wrapper adds `sparcli::Shortcuts` (a builder with a `std::function` callback arena) + `key_ctrl/key_fn/key_alt`; `sc.apply(opts)` wires it and `sc.fired()` reads the result.

### Styling (all opts fields are zero-init-friendly)

Every visual element is configurable through the widget's `Sc*Opts`; a zero-init field selects the built-in default, so existing zero-init callers are unaffected. Helper `sc_style_set()` (`input_internal.h`) decides "caller-supplied vs default" for each `ScTextStyle`; glyph fields fall back when `NULL`.

- **Prompt/label:** every widget has a `prompt_style` for its prompt/heading (confirm's styles the question text and the `? ` prefix). Defaults: bold in `accent` (fuzzy), bold (datepicker), unstyled elsewhere.
- **Rich prompt:** for *partial* styling (e.g. `Rename `*`Apple`*` to`) every widget's opts also take `prompt_text` (a borrowed `ScText *`, overrides the string prompt) and `prompt_markup` (parse the string prompt as markup). Precedence `prompt_text > prompt_markup > plain+prompt_style`. Resolved by the shared `sc_prompt_append`/`sc_prompt_build`/`sc_prompt_width` (`input_internal.h`); works inline and boxed. Boxed mode routes it through `ScTitle.rich_text` (a new `ScText *` on the shared title struct that panels honor; rules/tables ignore it), so the box width is computed from the visible width, not the escape bytes.
- **External editor** (`text_input`/`textarea` only): `external_editor` + optional `editor` command + `editor_key` (zero-init = Ctrl-G). The engine suspends raw mode (`sc_tty_end`), runs `sc_run_editor` (`editor.c`: `mkstemp` 0600 → `fork`/`execvp` **no shell** on `/dev/tty` → `waitpid` → read back → `unlink`), then `sc_tty_begin` + fresh repaint. Widgets expose `edit_get`/ `edit_set` vtable hooks; the engine owns suspend/resume. Non-zero exit keeps the old value; text_input collapses newlines to spaces (single-line). **Password excluded** (plaintext temp file). `ScPromptEditor` carries the config to `sc_prompt_run`; the editor key is matched before custom shortcuts (same chord → editor wins).
- **Text styles:** `selected_style`/`unselected_style` (confirm), per-widget `cursor_style` (text/password/fuzzy editor cell; default black-on-white), `error_style` (text/password; default red), `count_style` (text/password character counter; default dim), `selected_style` (select/fuzzy cursor row), `counter_style` (fuzzy), `header_style`/`weekday_style`/`selected_style` (datepicker), and a `summary_style` on every widget.
- **Glyphs:** `cursor_marker`/`marker` and `checkbox_on`/`checkbox_off` (select), `cursor_marker`/`marker` (fuzzy list), `header_prev`/`header_next` (datepicker).
- **Box framing (`box`, every widget):** `ScBoxStyle box` frames the widget in a panel (border, content `bg`, inner `padding`, outer `margin`, `width`). See the `ScBoxStyle` Core Type. Inherits theme box defaults; the hint footer stays outside the frame.
- **Summary line:** printed via `sc_println` over `sc_output_stream` (styled by `summary_style`); set `hide_summary = true` to suppress it entirely.
- **Fuzzy table view:** `ScFuzzyOpts.table_opts` is passed through to the table renderer (border, header, padding, …); the cursor row is highlighted with `accent` by default, overridable via `selected_style.bg` (and `.fg`). Zero-init `table_opts` → single border + bold header.
- **Week start:** `ScDatePickerOpts.week_start` is an `ScWeekStart` enum (`SC_WEEK_START_DEFAULT`=Monday, `…_MONDAY`, `…_SUNDAY`) – a plain `0` cannot mean both "unset" and "Sunday", so Sunday is an explicit value.

`accent` itself defaults per widget when zero-init (`index == 0`): green (confirm), cyan (select/fuzzy/datepicker).

### Style snapshot tests

Each widget exposes an **internal** frame builder (`sc_confirm_frame`, `sc_text_entry_frame`, `sc_select_frame`, `sc_fuzzy_frame`, `sc_datepicker_frame`; decls in `input_internal.h`) that runs the normal render path over a constructed state and returns the captured `ScRendered`. The `tests/input/style/` suite uses these to print every widget in many styles **without a TTY** (`make test-input-style`); `sc_pad_print` writes the frame to `sc_output_stream`.

## Internal Helpers (`src/internal.h`)

Not part of the public API. Used by all source files that include `internal.h`:

| Helper | Description |
|--------|-------------|
| `sc_apply_colors(fg, bg)` | Emits ANSI fg/bg escapes; skips if `index == 0` (zero-init / `SC_ANSI_COLOR_NONE`) |
| `sc_terminal_width()` | Terminal width via `ioctl(TIOCGWINSZ)`, fallback 80 |
| `sc_utf8_string_length(string, byte_length)` | Visible column count of a UTF-8 byte sequence |
| `sc_utf8_trim_to_cols(string, max_columns)` | Byte count that fits within `max_columns` columns |

---

## Key Invariants

- **Concurrency:** the output target is **thread-local** (`_Thread_local current_output` in `core/output.c`), so multiple threads may render/capture concurrently to independent streams (TSan-clean; see `test_threads`). Each widget object is per-instance, so independent objects are independent. The **interactive input session is process-wide** (one terminal, process-global signal handlers): only one prompt runs at a time, enforced by an atomic claim in `sc_prompt_run` – a concurrent/nested attempt returns `SC_INPUT_ERROR`. The global theme (`sc_input_set_theme`) is shared, set-once config. The **global logger** follows the same pattern: configuration (level/sinks/opts) is set-once before threads start; the log calls themselves are thread-safe (atomic level, one `fputs` per record).
- **`SC_ANSI_COLOR_NONE` sentinel is `index = 0`**, identical to a zero-initialized `ScColor`. Any unset color field renders as "no color" automatically; no explicit assignment needed. Named colors use `index = 1..8` (BLACK..WHITE); RGB uses `index = -1`.
- `sc_print()` always appends `\033[0m` (reset), even when opts are all-none. This is intentional to isolate styling.
- The `h` horizontal-line character from `ScBorderType` is used by both panel titles, table titles, rules, and column separators – all from the same logical table in each file.
- `ScText` / `ScTableData` / `ScColumns` all heap-allocate; always call the corresponding `_free()` function.
- `ScColumns` captures widget output at `sc_columns_add_*` time. Modifying a table after adding it to a columns layout has no effect.
- Word-wrap in tables (`ScColOpts.wrap = 1`) breaks on spaces only. If no space fits in the column width, the line is truncated.
- **Zero-init `ScTextStyle` sentinel** (used by `ScKVOpts.key_style`, `ScKVOpts.val_style`, `ScListOpts.marker_style`, `ScBadgeOpts.text_style`): zero-init = no formatting. Renderers use `opts_has_format()` to detect this and skip `sc_print()`.
- **Calculator display/submit exception:** number input's calculator mode (default) displays the result rounded to `decimals` but submits full precision - the one deliberate exception to "displayed text == submitted value". `*out == *out_text` still always holds. `calc_store_rounded` restores display == submit. The exception is made visible in the UI: a dim ` = <exact>` indicator marks a pending full-precision value, and discarding it by editing (when precision is actually lost) raises a yellow warning line that the displayed value is what gets submitted from then on.
- **Opts-string lifetime:** every handle-based `*_new()` (table columns, list, kv, tree, spinner, progressbar, select, fuzzy, theme, history) **copies** the string fields of its opts/arguments, so caller buffers (e.g. FFI temporaries) only need to live until the call returns. Documented exceptions stay **borrowed**: table cell strings / `ScText` content (until print), `shortcuts`, `prompt_text` and `history` (until the run). Run-once widgets (`sc_confirm`, `sc_text_input`, …) only read their opts during the call. Tests: `tests/input/logic/test_opts_copy.c`, output tests "Opts strings are copied", PTY cases `fuzzy-heap-prompt`/`theme-heap-strings`.
- **ANSI sanitization / trust boundary:** every user string is sanitized **exactly once, where it crosses into the library** (`sc_text_append`, `sc_print`, widget add/set/render entry points, markup chunks): control bytes (< 0x20 except `\n`/`\t`, plus DEL) and ANSI escape sequences are removed. The opt-out is `sc_set_allow_ansi(bool)` (process-wide, atomic, default `false`) plus a per-widget `ScAnsiMode ansi` opts field (`SC_ANSI_MODE_DEFAULT`=inherit global / `_ALLOW` / `_SANITIZE`); when allowed, well-formed escape sequences pass through, stray control bytes are still dropped, and **all width functions skip escape sequences** (`sc_utf8_string_length`, `sc_utf8_trim_to_cols`, `sc_text_visible_width`), so frames stay aligned. Library-rendered content (`ScRendered`, captured columns output, CLI capture path) is inside the boundary and is never re-sanitized – internal code uses `sc_text_append_raw`/`sc_print_raw`. External-editor read-back and keyboard input are always filtered (never allow ANSI). **OSC-8 hyperlinks** (`sc_text_append_link`, markup `[link=…]`) are generated on the trusted side: the URL is scrubbed to printable ASCII (`sc_osc8_scrub_url`) so it cannot inject sequences, then the wrapper is built internally (`sc_osc8_wrap`) and appended raw; injected OSC-8 in ordinary user strings is still stripped. Sanitizer: `src/core/sanitize.c` (+ `sanitize_internal.h`). Tests: `tests/input/logic/test_sanitize.c`, `tests/fuzz/fuzz_sanitize.c`, CLI golden "sanitize:" sections.

---

## Key-Value List

```c
ScKV *sc_kv_new  (ScKVOpts opts);
void  sc_kv_add  (ScKV *kv, const char *key, const char *value);
void  sc_kv_print(const ScKV *kv);
void  sc_kv_free (ScKV *kv);
```

### ScKVOpts

| Field | Description |
|-------|-------------|
| `sep` | Separator string after the padded key; `NULL` = `"  "` (2 spaces) |
| `key_width` | `0` = auto (widest key); `>0` = fixed column width |
| `width` | Total line width; `0` = terminal width |
| `margin` | Symmetric left+right outer margin in characters |
| `item_gap` | Blank lines between items; default 0 |
| `wrap_val` | `1` = word-wrap long values with hanging indent; `0` = truncate |
| `key_style` | Style for key text; **zero-init = no formatting** |
| `val_style` | Style for value text; **zero-init = no formatting** |
| `ansi` | `ScAnsiMode` – ANSI passthrough for key/value strings; zero-init inherits the `sc_set_allow_ansi` global |

**Layout:** `margin + key (padded to key_w) + sep + value + margin`

Continuation lines of a wrapped value are indented by `margin + key_w + sep_w` columns.

---

## Alert Presets

Thin wrappers over `sc_panel_str`/`sc_panel_text` with preset icon, color, and border.

```c
void sc_alert_str    (ScAlertType type, const char *content);
void sc_alert_text   (ScAlertType type, const ScText *content);
void sc_alert_info   (const char *content);
void sc_alert_debug  (const char *content);
void sc_alert_warning(const char *content);
void sc_alert_error  (const char *content);
void sc_alert_success(const char *content);
```

| Type | Icon | Color |
|------|------|-------|
| `SC_ALERT_INFO` | ℹ | Blue |
| `SC_ALERT_DEBUG` | ⚙ | Magenta |
| `SC_ALERT_WARNING` | ⚠ | Yellow |
| `SC_ALERT_ERROR` | ✖ | Red |
| `SC_ALERT_SUCCESS` | ✔ | Green |

All alerts render `full_width = 1` with `SC_BORDER_SINGLE` and a colored left-aligned title. Content may contain `\n` for multi-line bodies.

---

## Badge

Inline styled text token. `sc_print_badge` writes to stdout (no trailing newline). `sc_text_append_badge` appends the composed badge string as a single span to an `ScText`.

```c
void sc_print_badge      (const char *text, ScBadgeOpts opts);
void sc_text_append_badge(ScText *t, const char *text, ScBadgeOpts opts);
```

### ScBadgeOpts

| Field | Description |
|-------|-------------|
| `left_cap` | `NULL` = `"["` |
| `right_cap` | `NULL` = `"]"` |
| `text_style` | Style for the full badge (caps + padding + text); **zero-init = no formatting** |
| `pad` | Spaces inside each cap; default 0 |
| `ansi` | `ScAnsiMode` – ANSI passthrough for the badge text; zero-init inherits the `sc_set_allow_ansi` global |

Badge string: `left_cap + pad×' ' + text + pad×' ' + right_cap`

---

## Utilities

```c
char *sc_strip_ansi(const char *str);
char *sc_truncate  (const char *str, int max_cols, const char *ellipsis);
void  sc_clear_line(void);
```

- `sc_strip_ansi`: returns a heap-allocated copy of `str` with **all** ANSI escape sequences removed (CSI, OSC, DCS/SOS/PM/APC, two-char ESC, lone ESC). Other bytes are copied verbatim. Caller must `free()` the result.
- `sc_truncate`: if the visible width of `str` exceeds `max_cols`, returns a heap-allocated truncated copy with `ellipsis` appended (may be `NULL`). If it fits, returns `strdup(str)`. Caller must `free()` the result.
- `sc_clear_line`: writes `\r` + spaces (terminal width) + `\r` + `fflush` to overwrite the current terminal line in place.

---

## Capture API

Renders any widget into a heap-allocated `ScRendered`. Caller must free with `sc_rendered_free()`. Use the result with `sc_pad_print`, `sc_align_print`, or `sc_columns_add_rendered`.

```c
ScRendered *sc_capture_str        (const char *s);
ScRendered *sc_capture_text       (const ScText *t);
ScRendered *sc_capture_table      (const ScTableData *t, ScTableOpts opts);
ScRendered *sc_capture_list       (const ScList *l);
ScRendered *sc_capture_tree       (const ScTree *t);
ScRendered *sc_capture_kv         (const ScKV *kv);
ScRendered *sc_capture_columns    (const ScColumns *cl);
ScRendered *sc_capture_panel_str  (const char *content, ScPanelOpts opts);
ScRendered *sc_capture_panel_text (const ScText *content, ScPanelOpts opts);
ScRendered *sc_capture_panel_rendered(const ScRendered *content, ScPanelOpts opts);
ScRendered *sc_capture_rule_str   (const char *title, ScRuleOpts opts);
ScRendered *sc_capture_rule_text  (const ScText *title, ScRuleOpts opts);
```

The same `ScRendered *` can be passed to multiple print functions (e.g. first `sc_pad_print`, then `sc_align_print`).

`sc_capture_panel_rendered` frames an **already-captured** `ScRendered` inside a panel: the rendered lines (with their own ANSI styling) become the panel content verbatim - they are trusted, library-generated text and are **not** re-sanitized, so embedded colors survive. It is the primitive behind input-widget box framing (`sc_box_wrap`) and is reusable to put a border / background / padding around any captured widget. **Background + embedded resets:** when `opts.bg` is set, each captured line is split at its `\033[0m` resets into one raw span per segment, because the panel re-applies its content bg only **after each span** (`print_line_spans`); without the split the embedded resets inside one big raw span would clear the bg and leave the text on the terminal default (only the padding filled). With no bg the line is appended whole.

### sc_vstack – stack widgets vertically in one column

```c
ScRendered *sc_vstack(const ScRendered *const *parts, size_t n, int gap);
```

Concatenates `n` captured renderings top-to-bottom into a single `ScRendered`, with `gap` blank lines between adjacent parts. This is how you place **two (or more) widgets one above the other inside a single column** – capture each widget, `sc_vstack` them, then `sc_columns_add_rendered` the result.

Inputs are **not** consumed: the caller still owns every `parts[i]` and frees them (and the returned value) with `sc_rendered_free`. Returns `NULL` when `n == 0`; `gap` is clamped to `>= 0`. `max_column_width` of the result is the widest line across all parts.

```c
ScRendered *r_list  = sc_capture_list(list);
ScRendered *r_rule  = sc_capture_rule_str("More", rule_opts);
ScRendered *parts[] = { r_list, r_rule };
ScRendered *col     = sc_vstack((const ScRendered *const *)parts, 2, 1);

sc_columns_add_rendered(cols, col, (ScColItem){ 0 });  /* list above the rule */
/* free col, r_list, r_rule individually */
```

---

## Padding

```c
typedef struct { int top; int right; int bottom; int left; } ScPadOpts;

void sc_pad_print(const ScRendered *r, ScPadOpts opts);
void sc_pad_str  (const char *s,       ScPadOpts opts);   /* capture + print */
void sc_pad_text (const ScText *t,     ScPadOpts opts);   /* capture + print */
```

`sc_pad_print` prints `top` blank lines, then each content line with `left` spaces prepended and `right` spaces appended, then `bottom` blank lines.

`right` padding (trailing spaces per line) is mostly useful in composed contexts (e.g. coloured backgrounds); it has no visible effect on a plain terminal.

**Usage:**
```c
ScRendered *r = sc_capture_table(t, opts);
sc_pad_print(r, (ScPadOpts){ .top = 1, .bottom = 1, .left = 4 });
sc_rendered_free(r);

/* convenience (one step): */
sc_pad_str("Hello", (ScPadOpts){ .left = 8 });
```

---

## Align

```c
/* width = 0 → sc_terminal_width(); width > 0 → fixed column count */
void sc_align_print(const ScRendered *r, ScHAlign halign, int width);
void sc_align_str  (const char *s,       ScHAlign halign, int width);
void sc_align_text (const ScText *t,     ScHAlign halign, int width);
```

Aligns every line of the rendered output within `width` columns. `SC_ALIGN_LEFT` is a no-op (prints as-is).

**Usage:**
```c
ScRendered *r = sc_capture_table(t, opts);
sc_align_print(r, SC_ALIGN_CENTER, 0);   /* center in terminal */
sc_rendered_free(r);

/* convenience (one step): */
sc_align_str("Centered heading", SC_ALIGN_CENTER, 0);
```

### sc_columns_add_rendered

```c
void sc_columns_add_rendered(ScColumns *cl, const ScRendered *r, ScColItem item);
```

Inserts an already-captured `ScRendered` into a `ScColumns` layout. The columns layout makes a deep copy, so the caller may free `r` immediately after.

```c
ScRendered *r = sc_capture_table(t, opts);
sc_columns_add_rendered(cl, r, (ScColItem){ 0 });
sc_rendered_free(r);   /* safe: columns owns its own copy */
```

---

## Markup

Rich-compatible inline markup. Parse a string into an `ScText *` or print directly.

### Syntax

| Tag | Effect |
|-----|--------|
| `[bold]` | `SC_TEXT_ATTR_BOLD` |
| `[italic]` | `SC_TEXT_ATTR_ITALIC` |
| `[underline]` / `[u]` | `SC_TEXT_ATTR_UNDER` |
| `[dim]` | `SC_TEXT_ATTR_DIM` |
| `[red]` … `[white]` `[black]` | foreground named color |
| `[on red]` … `[on white]` | background named color |
| `[rgb(r,g,b)]` | foreground RGB |
| `[on rgb(r,g,b)]` | background RGB |
| `[bold red on white]` | combined (all in one tag) |
| `[/]` | close most-recent style frame |
| `[/bold]`, `[/red]`, … | named close (same effect as `[/]`) |
| `[link=URL]text[/link]` | OSC-8 hyperlink; body is literal (nested tags not parsed), also closable with `[/]`; empty URL = unknown tag |
| `` `inline code` `` | code span: magenta fg by default (`ScMarkupOpts.code_style`), backticks removed; body is literal (tags/`[[` not parsed); unclosed backtick stays verbatim |
| `` \` `` | literal backtick character (also inside a code span) |
| `[[` | literal `[` character |
| `[blink]` (any unrecognized) | emitted verbatim including brackets |

Tags stack: `[bold][red]text[/] still bold[/]` – closing pops the top frame.

### Functions

```c
/* default: strip_unknown = 0 (verbatim) */
ScText *sc_markup_parse       (const char *s);
ScText *sc_markup_parse_opts  (const char *s,      ScMarkupOpts opts);
void    sc_markup_append      (ScText *t, const char *markup);
void    sc_markup_append_opts (ScText *t, const char *markup, ScMarkupOpts opts);
void    sc_markup_print       (const char *markup);
void    sc_markup_print_opts  (const char *markup,  ScMarkupOpts opts);
void    sc_markup_println     (const char *markup);
void    sc_markup_println_opts(const char *markup,  ScMarkupOpts opts);
```

**Usage:**
```c
/* Print directly */
sc_markup_println("[bold red]Error:[/] something went wrong");

/* Use with any widget that accepts ScText */
ScText *t = sc_markup_parse("[bold]sparcli[/] supports [italic]markup[/]");
sc_panel_text(t, opts);
sc_text_free(t);

/* Append markup into an existing ScText */
ScText *t = sc_text_new();
sc_text_append(t, "prefix – ", (ScTextStyle){0});
sc_markup_append(t, "[green]green suffix[/]");
sc_print_text(t);
sc_text_free(t);
```

### ScMarkupOpts – parser options

```c
typedef struct {
    int strip_unknown;      /* 1 = silently remove unrecognized tags; 0 = verbatim (default) */
    ScAnsiMode ansi;        /* raw-ESC passthrough; zero-init = inherit sc_set_allow_ansi */
    ScTextStyle code_style; /* style for `inline code` spans; zero-init = magenta fg.
                               Non-zero fields override the surrounding frame; fg always
                               replaced (magenta when unset), attr/bg inherited when unset */
} ScMarkupOpts;
```

Controls what happens with unrecognized tags like `[blink]`, `[link=...]`, `[strike]`:

| `strip_unknown` | `[blink]hello[/blink]` becomes |
|-----------------|-------------------------------|
| `0` (default)   | `[blink]hello[/blink]` – brackets printed as literal text |
| `1`             | `hello` – tag brackets silently removed, content kept |

```c
/* strip unknown tags – only content is kept */
sc_markup_println_opts("[blink]text[/blink]", (ScMarkupOpts){ .strip_unknown = 1 });

/* mixed: known tags styled, unknown tags stripped */
ScText *t = sc_markup_parse_opts(
    "[bold]important[/] [blink]ignored-tag[/blink] suffix",
    (ScMarkupOpts){ .strip_unknown = 1 }
);
```

### sc_cell_m – markup in tables

```c
static inline ScCell sc_cell_m(const char *s);  /* parses s as inline markup */
```

The cell **owns** the parsed `ScText`; `sc_table_free` frees it automatically. No separate free needed. (FFI variant: `sc_cell_from_markup(s)`.)

```c
sc_table_add_row(t, (ScCell[]){
    sc_cell_m("[green]✔ OK[/]"),
    sc_cell_m("Build [bold]passed[/]"),
}, 2);
sc_table_free(t);  /* frees markup ScText automatically */
```

**Unknown tags:** Any tag with an unrecognized token is emitted verbatim by default (including brackets). Use `ScMarkupOpts{ .strip_unknown = 1 }` to silently discard them.

**`[[` escape:** Two consecutive opening brackets produce a single literal `[`.

**Backtick inline code:** `` `code` `` renders the body in magenta (or `ScMarkupOpts.code_style`) with the backticks removed. The body is literal (like the `[link=…]` body): tags and `[[` are not parsed inside. `` \` `` escapes a literal backtick (outside and inside code spans); an unclosed backtick is kept verbatim. The code span inherits the surrounding frame's attr/bg; only the fg is replaced. Bindings: C++ uses `ScMarkupOpts` directly, Rust `MarkupOpts::code_style`, Python `code_style=` kwarg on `sc.markup.*` / `sc.Text.from_markup/append_markup`.

---

## Pager

Routes long output through `$PAGER` / `less -R`. Stream-oriented: redirects the **thread-local** `sc_output_stream()` into a pipe to the pager child (forked + `execvp`, **no shell**); other threads are unaffected. Header: `output/sparcli_pager.h`, source: `src/output/pager.c`.

```c
ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });  /* .command, .always */
sc_table_print(table, opts);                          /* paged */
int status = sc_pager_end(pager);                     /* flush, waitpid, restore */
```

| Behavior | Detail |
|----------|--------|
| Non-TTY output stream | No-op (unless `.always`); same code works in scripts/CI |
| Pager quits early (`q`) | `SIGPIPE` ignored for the session; disposition restored on end |
| Command missing | Falls back to `cat` (output never lost) |
| `command` | Split on whitespace, `execvp` without shell; `NULL` = `$PAGER`, then `less -R` |
| `sc_pager_end` | Always required (also after no-op begin); `NULL`-safe; returns pager exit status |

Tests: `tests/app/test_pager.c` (`make test-app`) – uses `cat`/`false` as deterministic pagers with stdout redirected to a temp file.

---

## Live Display (`src/output/live.c`)

Re-renders a composed `ScRendered` frame **in place** (dashboard like Rich `Live`). Stream-oriented: writes escape codes via `fprintf` to the **thread-local** `sc_output_stream()` captured at begin – so it composes with capture/pager (degrades, see below) and never touches the tty layer. Header: `output/sparcli_live.h`.

```c
ScLive *live = sc_live_begin((ScLiveOpts){ 0 });  /* .alt_screen, .show_cursor, .transient, .always, .prompt_rows */
sc_live_update(live, frame);                      /* ScRendered: capture + vstack/columns */
sc_live_update_str/text/table(live, ...);         /* capture + update + free in one call */
sc_live_end(live);                                /* restore + final flush + free handle */
```

| Behavior | Detail |
|----------|--------|
| Non-TTY stream | Updates buffered; only the **final frame** printed on end (clean CI/script output). `always` forces escape emission |
| Redraw | Cursor-up to frame top, overwrite line by line + `\033[K`, then `\033[J` for leftover lines; height-clamped via `sc_terminal_height()` (internal.h) |
| `alt_screen` | `\033[?1049h/l` fullscreen; final frame re-printed on the normal screen on end (unless `transient`) |
| `prompt_rows` | Rows reserved **below** the frame for an interactive prompt (REPL dashboards): the cursor parks at column 0 of the first reserved row after every update, and `prev_lines` spans frame + reserve so the next rewind still lands on the frame top. Reserve as many rows as the prompt draws; the height clamp shrinks the frame, never the reserve. The prompt must run with `hide_summary = true` (its self-erase via `sc_screen_clear` then returns the cursor exactly to the park row). Zero-init = 0 = classic behavior |
| Cursor | Hidden during the session (zero-init); `show_cursor` keeps it visible |
| Cleanup | `atexit` + SIGINT/TERM/HUP/QUIT handlers restore cursor/alt-screen (re-raise after); crash signals not trapped |
| Ownership | `sc_live_end` frees the handle (pager-style begin/end pair); frames stay caller-owned |
| Sessions | One per terminal at a time (soft); no background thread (thread-local stream) |

Tests: `tests/output/test_live.c` (`make test-output`; non-TTY path is golden-tested, escape emission + `prompt_rows` park/rewind verified via memstream + `always`); animated dashboard demo flagged `--no-animated`-skippable. Examples: `examples/c/output/live.c`, `examples/c/apps/repl_dashboard.c` (live + prompt + shortcut bar).

---

## XDG Paths (`src/app/`)

Per-application directories per the XDG Base Directory Specification; created (mode 0700) on first use. Header: `app/sparcli_paths.h` (sub-umbrella `app/sparcli_app.h`).

```c
char *dir = sc_path_config("myapp");      /* ~/.config/myapp (heap; free()) */
char *log = sc_path_file(SC_PATH_STATE, "myapp", "logs/run.log");
```

| Function | Result |
|----------|--------|
| `sc_path(kind, app)` / `sc_path_config/data/cache/state(app)` | App directory for the kind (`$XDG_*` if absolute, else `$HOME` fallback) |
| `sc_path_file(kind, app, relative)` | File path inside the app dir; parents created, file not |

All results are heap strings (caller frees); `NULL` on failure. **Validation:** `appname` must be a single safe path component; `relative` may contain subdirectories but no `..`/absolute/control bytes (path-traversal guard). Relative `$XDG_*` values are ignored per spec. Tests: `tests/app/test_paths.c` (sandboxed `$HOME`, never touches real user dirs).

Bindings: C++ `sparcli::paths::*` (`std::optional<std::string>`) + `sparcli::Pager` (RAII); Rust `sparcli::paths::*` (`Option<PathBuf>`) + `Pager` (Drop); Python `sc.config_dir(...)` etc. (`pathlib.Path`, raises `SparcliError`) + `sc.Pager` (context manager).

---

## Pretty Errors (`src/app/error.c`)

Structured error reporting: message + cause chain + hint + exit code, rendered as a red alert panel (reuses `sc_alert_text(SC_ALERT_ERROR, …)` - zero new rendering code). Header: `app/sparcli_error.h`. **No signal handling / crash trapping.**

```c
ScError *e = sc_error_new("Config could not be loaded");   /* strings copied + sanitized */
sc_error_add_cause(e, "file not found: ~/.config/app.toml");
sc_error_set_hint(e, "Run 'app init' to create one");
sc_error_set_code(e, 2);                                    /* default 1 */
sc_die(e);                       /* render to stderr + free + exit(2) */

sc_die_msg(2, "message", "hint");        /* one-shot convenience */
sc_error_print(e);                       /* render to sc_output_stream(); no exit */
sc_error_print_stderr(e);                /* render to stderr; stream restored; no exit */
```

| Invariant | Detail |
|-----------|--------|
| Trust boundary | Message/causes/hint are sanitized at the builder entry points; rendering uses raw appends |
| Output target | `sc_error_print` → thread-local stream (capture-able); `sc_die`/`_print_stderr` → always stderr |
| Ownership | Builder copies all strings; `sc_die` consumes (frees) the error; `sc_error_code(NULL)` = 1 |
| Rendering | Bold message, dim indented `caused by:` lines, blank line + bold `Hint:` block |

Tests: `tests/app/test_errors.c` (logic: copies, NULL safety, stderr routing) + `tests/output/test_errors.c` (golden rendering). Bindings: C++ `sparcli::ErrorReport` (+ `sparcli::die()`), Rust `sparcli::ErrorReport` (`die() -> !`), Python `sc.ErrorReport` (`die()` raises `SystemExit`, never calls C `exit()`).

---

## Logging (`src/log/`)

Leveled, colored terminal logging + plain-text file sinks. Header: `log/sparcli_log.h`. **Global logger** (`sc_log_*`: process-wide, built-in stderr sink at INFO) and **handle-based loggers** (`sc_logger_*`: own sinks/options).

```c
sc_log_set_level(SC_LOG_DEBUG);              /* stderr sink level; default INFO */
sc_log_add_file("app.log", SC_LOG_DEBUG);    /* plain-text file sink (appends) */
sc_log_info("started on port %d", port);     /* macros pass __FILE__/__LINE__ */

ScLogger *lg = sc_logger_new((ScLoggerOpts){ .hide_timestamps = true });
sc_logger_add_terminal(lg, stderr, SC_LOG_INFO);   /* colored; stream borrowed */
sc_logger_add_file(lg, "debug.log", SC_LOG_DEBUG); /* plain; owned + closed on free */
sc_logger_info(lg, "subsystem ready");
sc_logger_free(lg);
```

`ScLoggerOpts`: `level` (zero-init = DEBUG = everything), `hide_timestamps`, `show_source` (file:line suffix), `markup` (parse tags in messages), `ansi`.

| Invariant | Detail |
|-----------|--------|
| Render once | Record rendered once (colored); terminal sinks get colored bytes, file sinks `sc_strip_ansi`'d plain text |
| Output independence | Sinks own their `FILE *`; logging NEVER touches `sc_output_stream()` (no interference with capture/pager) |
| Trust boundary | Formatted message sanitized (or markup-parsed) exactly once; format strings are developer-controlled (`__attribute__((format))` checked) |
| Thread safety | Global config (level atomic, sinks set-once: configure before spawning threads); emits are race-free, one `fputs` per record per sink = no torn lines. Handle loggers: not internally synchronized |
| Global stderr sink | Colors only when stderr `isatty`; level via `sc_log_set_level` (default INFO); `sc_log_reset()` restores defaults (tests) |

Tests: `tests/app/test_log.c` (logic; `make test-app`) + concurrent-logging case in `tests/input/logic/test_threads.c` (runs under `make tsan` - the critical gate). Bindings pass messages as **data** (`"%s"` + string, never a format string): C++ `sparcli::Logger` + `sparcli::logging::*`, Rust `sparcli::Logger` + `sparcli::log::*`, Python `sc.Logger` + `sc.log_info(...)` etc.

---

## Argument Parser (`src/args/`)

Declarative, builder-style parser ("clap for C"): describe the command tree once, `sc_args_parse` handles `--help`/`--version`, validation, pretty-error reporting with did-you-mean and typed value conversion. Header: `args/sparcli_args.h`. Full tutorial + reference: `docs/api-framework.md`. **C + C++ bindings only** (Rust has clap, Python has argparse; the C sources still compile into those libs).

```c
ScArgs *args = sc_args_new((ScArgsOpts){ .prog = "tool", .version = "1.0", .about = "..." });
ScArgsCmd *root = sc_args_root(args);
sc_args_flag(root, "verbose", 'v', "Verbose output");
ScArgsCmd *build = sc_args_subcommand(root, "build", "Build the project");
sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");      /* + _default/_choices/_required */
sc_args_positional(build, "TARGET", SC_ARG_STR, "Build target", true, false);

ScArgsStatus status;                       /* MATCHED / HANDLED (help/version) / ERROR */
const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
long jobs = sc_args_get_int(args, "jobs"); /* getters search matched cmd + ancestors */
sc_args_free(args);

/* REPL loop: tokenize a line, parse the same tree once per line */
char **line_argv = sc_args_split("tool", line, &line_argc, err, sizeof err); /* quotes/escapes; NULL = error in err */
sc_args_parse(args, line_argc, line_argv, &status);  /* implicitly resets previous results */
sc_args_split_free(line_argv);
sc_args_reset(args);                       /* explicit clear without reparsing */
```

| Aspect | Detail |
|--------|--------|
| Source layout | `args.c` (builder/getters/free/reset), `args_parse.c` (parse loop), `args_value.c` (int/double/color/choices), `args_suggest.c` (Levenshtein), `args_help.c` (widget-rendered help), `args_complete.c` (zsh completion), `args_split.c` (REPL line tokenizer); internals in `args_internal.h` |
| Syntax | `--opt value`, `--opt=value`, `-j8`, `-vq` (combined flags), `--` terminator, negative numbers as values, nested subcommands, variadic positionals |
| Types | `SC_ARG_STR/INT/DOUBLE/COLOR`; "enums" = STR + `sc_args_opt_choices` (`sc_args_get_enum` returns the index) |
| Reserved | `--help`/`-h` (every command), `--version`/`-V` (root, when version set); user options with the same short letter win |
| Trust boundary | Every argv token is ANSI-sanitized before storage/echoing; errors render as Pretty Errors (stderr, exit code 2). `sc_args_split` itself does **not** sanitize (parse does, per token) |
| Help rendering | Borderless 2-col tables + bold/cyan headings via the normal widget stack; cell strings are kept in a `StringArena` until print (table cells borrow) |
| Ownership | Builder strings copied; getter results borrowed until `sc_args_free`; `ScArgsCmd*` nodes are borrowed from the tree; `sc_args_split` returns a NULL-terminated heap argv (free with `sc_args_split_free`) |
| REPL reuse | `sc_args_parse` calls `sc_args_reset` at its start, so one tree parses once per REPL line with no stale values; `sc_args_split` rules: whitespace splits, `'…'` literal, `"…"` with `\` escapes, quoted+bare runs concatenate, `argv[0]` = the passed prog name (parse starts at index 1) |
| Tests | `tests/args/` (`make test-args`: parse/errors/help/completion/split/reset) + `tests/fuzz/fuzz_args.c` + `tests/fuzz/fuzz_split.c` (random input under ASan/UBSan, part of `make fuzz`) |
| Examples | `examples/c/app/args.c`, `examples/c/apps/repl_demo.c` (REPL: split + implicit reset + history), `examples/c/apps/repl_minimal.c`, `examples/c/apps/repl_dashboard.c` |

The fuzzer found (and led to the fix of) a pre-existing out-of-bounds read in `sc_utf8_trim_to_cols`/`sc_utf8_string_length` (`src/internal.h`): truncated UTF-8 sequences at the end of a string are now advanced bounds-safely.
