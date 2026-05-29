# sparcli — Developer Reference

A C11 library for styled terminal output: colored text, bordered panels,
feature-rich tables, horizontal rules, and multi-column side-by-side layouts.

## Build

```sh
make            # builds libsparcli.a + shared lib + pkg-config
make test       # compiles and runs the OUTPUT suite (tests/output/test_main)
make test-input # compiles and runs the INPUT suite (tests/input/) — interactive,
                # needs a real TTY. `make test-input ARGS=--logic` runs only the
                # non-interactive logic tests (key decoder, line editor) for CI.
make clean      # removes build trees, .a, shared libs, test binaries
```

Compiler: `cc -std=c11 -Wall -Wextra -Iinclude -Isrc`

### Directory layout

Sources are grouped by concern. The **output/input boundary is physical**:
`src/output` (and `core`) is stream-oriented and writes through the
redirectable `sc_output_stream()`; `src/tty` + `src/input` are tty-oriented
and drive a real terminal in raw mode (never `sc_output_stream`).

```
src/core/     color, text, print, output(stream), render_wrap, version
src/output/   panel, rule, list, tree, columns, kv, alert, badge,
              progressbar, spinner, markup, pad, util, + table/
src/tty/      term (raw mode + signal restore), key (decoder), screen (redraw)
src/input/    prompt (loop engine), line_editor, confirm, text_input,
              password_input, select, fuzzy, datepicker
include/core/    include/output/    include/input/      (sparcli.h stays at root)
tests/output/    tests/input/
```

Public headers live in `include/{core,output,input}/`; cross-includes use
**root-relative paths** (`#include "core/sparcli_core.h"`), resolved via
`-Iinclude`. `sparcli.h` is the full umbrella; `input/sparcli_input.h` is the
input-only sub-umbrella. `#include <sparcli.h>` is unchanged for users; only
direct single-header includes moved.

When adding a source file, append its path (e.g. `src/output/foo.c`) to `SRC`
in the Makefile. The build tree mirrors `src/` automatically.

---

## Core Types

### ScColor

```c
typedef struct { int index; uint8_t r, g, b; } ScColor;
```

| `index` | Meaning |
|---------|---------|
| `0`     | **Not set** — `SC_ANSI_COLOR_NONE`; no escape code emitted. **Zero-init `ScColor` lands here**, so any unset color field is "no color" by default. |
| `-1`    | 24-bit RGB mode; uses `r`, `g`, `b` fields |
| `1`–`8` | Named ANSI color (`SC_ANSI_COLOR_BLACK` … `SC_ANSI_COLOR_WHITE`) |

Zero-init friendly: `(ScColor){0}` ≡ `SC_ANSI_COLOR_NONE`, so leaving any
color field unset emits no escape codes. Construct explicit black with
`SC_ANSI_COLOR_BLACK`; RGB with `sc_ansi_color_from_rgb(...)`.

```c
ScColor sc_ansi_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);  // index = -1
```

Named macros: `SC_ANSI_COLOR_NONE`, `SC_ANSI_COLOR_BLACK`, `SC_ANSI_COLOR_RED`, `SC_ANSI_COLOR_GREEN`,
`SC_ANSI_COLOR_YELLOW`, `SC_ANSI_COLOR_BLUE`, `SC_ANSI_COLOR_MAGENTA`, `SC_ANSI_COLOR_CYAN`, `SC_ANSI_COLOR_WHITE`.

### ScTextAttribute

Bitmask — combine with `|`:

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

`sc_text_visible_width(t)` — returns the max visible width across lines (ANSI-aware,
UTF-8-aware, counts codepoints not bytes).

### ScBorderType

```c
SC_BORDER_NONE | SC_BORDER_ASCII | SC_BORDER_SINGLE |
SC_BORDER_DOUBLE | SC_BORDER_ROUNDED | SC_BORDER_THICK
```

Used by panels, tables, rules, and column separators.

### ScTitle

```c
typedef struct {
    const char      *text;  /* NULL = no title */
    ScTextStyle      style; /* text rendering (bold, color, …) */
    ScHAlign         align; /* LEFT / CENTER / RIGHT */
    int              pad;   /* spaces on each side of the title text */
    ScTitlePosition  pos;   /* SC_POSITION_TOP / SC_POSITION_BOTTOM; unused for rules */
} ScTitle;
```

Used directly everywhere a title is needed. `pos` is ignored by rules (no top/bottom distinction).
Access paths:
- `rule_opts.title.text` / `.style` / `.align` / `.pad`
- `panel_opts.title.text` / `.style` / `.align` / `.pad` / `.pos`

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
| `title.align` | `ScHAlign` | LEFT / CENTER / RIGHT |
| `title.pad` | `int` | Spaces on each side of the title text |
| `title.pos` | `ScTitlePosition` | `SC_POSITION_TOP` / `SC_POSITION_BOTTOM` |
| `padding` | `ScEdges` | Inner content padding (top/right/bottom/left) |
| `margin` | `ScEdges` | Outer margin |
| `width` | `int` | 0 = auto-fit content |
| `content_align` | `ScHAlign` | Horizontal alignment of content lines |
| `full_width` | `bool` | Stretch to terminal width (overrides `width`) |

`full_width` takes precedence over `width`. Width is calculated as `terminal_width - 2`.

**Background color sentinel:** `border_bg` and `bg` use `{0,0,0,0}` (zero-init) as "not set" — the same sentinel as `ScProgressBarOpts.fill_color`. A zero-initialized `ScPanelOpts` field means no background. Use `SC_ANSI_COLOR_NONE` or leave unset for no color; use `sc_ansi_color_from_rgb(...)` for a specific color.

---

## Tables

Table sources live in `src/table/` and are chained via `#include`:
`table.c` → `table_print.c` → `table_print_init.c` → `table_print_render.c`
→ `table_print_render_cell.c`, `table_print_render_border.c`, `table_print_render_row.c`.
Internal types are declared in `src/table/table_internal.h`.
Only `src/table/table.c` appears in the Makefile `SRC`; the rest are included implicitly.

```c
ScTableData *sc_table_new(void);
void         sc_table_add_column(ScTableData *table, const char *header, ScColOpts col);
void         sc_table_add_row(ScTableData *table, ScCell *cells, size_t n);
void         sc_table_add_row_bg(ScTableData *table, ScCell *cells, size_t n, ScColor bg);
void         sc_table_add_footer_row(ScTableData *table, ScCell *cells, size_t n);
void         sc_table_print(const ScTableData *table, ScTableOpts opts);
void         sc_table_free(ScTableData *table);
```

`ScTableOpts` is passed at **print time**, not at construction — `sc_table_new()` takes
no arguments. The same `ScTableData *` can be printed multiple times with different opts.

### ScTableOpts

| Field | Description |
|-------|-------------|
| `border` | `ScTableBorder` — style + color per border segment |
| `header` | `ScTableHeader` — grouped header settings (see below) |
| `header.row` | `bool` — first added row is the header |
| `header.col` | `bool` — first column is a header column |
| `header.row_bg` / `header.col_bg` | Background for header areas |
| `header.style` | `ScTextStyle` applied to all header cells |
| `striped` | `bool` — alternating row backgrounds |
| `stripe_bg` | Background for odd data rows (0-indexed) |
| `footer` | `ScTableFooter` — grouped footer settings (see below) |
| `footer.row_bg` / `footer.col_bg` | Background for footer rows/column |
| `footer.style` | `ScTextStyle` for footer cells |
| `title` | `ScTitle` — table title |
| `title.text` | Title string; `NULL` = no title |
| `title.style` / `title.align` / `title.pad` / `title.pos` | Title text appearance and position |
| `cell_pad` | `ScEdges` — inner cell padding |
| `margin` | `ScEdges` — outer table margin (top/right/bottom/left) |
| `total_width` | 0 = auto; >0 = distribute width across flex columns |
| `max_rows` | 0 = unlimited; >0 = truncate with indicator |
| `rtl` | `bool` — right-to-left column order |

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
} ScColOpts;
```

**Background priority:** `header/footer bg > per-row bg > stripe bg > col bg`.
Per-column bg is the lowest-priority fallback: only applied when no row-level
background is active (`row_bg.index == 0`).

### ScCell macros

| Macro | Description |
|-------|-------------|
| `SC_CELL(s)` | Plain string cell |
| `SC_CELL_A(s, ha, va)` | String + horizontal + vertical alignment |
| `SC_CELL_T(t)` | ScText cell |
| `SC_CELL_TA(t, ha, va)` | ScText + alignment |
| `SC_CELL_CS(s, cs)` | String + colspan |
| `SC_CELL_CSA(s, cs, ha)` | String + colspan + horizontal alignment |
| `SC_CELL_TCS(t, cs)` | ScText + colspan |
| `SC_CELL_TCSA(t, cs, ha)` | ScText + colspan + horizontal alignment |
| `SC_CELL_SKIP` | Placeholder cell covered by a colspan |
| `SC_CELL_RS(s, rs)` | String + rowspan |
| `SC_CELL_TRS(t, rs)` | ScText + rowspan |
| `SC_ROW_SKIP` | Placeholder row covered by a rowspan |

Colspan: set `colspan > 1` on the spanning cell; fill the remaining positions
with `SC_CELL_SKIP`. Rowspan: set `rowspan > 1` on the spanning cell; fill the
covered rows with `SC_ROW_SKIP` at the corresponding column position.

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
| `type` | `ScBorderType` — which `h` character to use |
| `color` | Line color; `SC_ANSI_COLOR_NONE` = no escape codes |
| `title` | `ScTitle` — title label (text, style, align, pad; pos ignored) |
| `title.text` | Title string; `NULL` = no title |
| `title.style` | `ScTextStyle` for the title text |
| `title.align` | LEFT / CENTER / RIGHT (default CENTER) |
| `title.pad` | Spaces on each side of title, default 1 |
| `width` | 0 = full terminal width; >0 = fixed width |
| `align` | Placement of the rule when `width > 0` (LEFT/CENTER/RIGHT) |
| `margin` | `ScEdges` — top/bottom = blank lines; left/right = indent |

---

## Columns

Renders multiple widgets side by side using a capture-and-replay approach:
each widget is rendered into a `tmpfile()` via `dup2`, then lines are zipped
horizontally.

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
| `sep` | `ScBorderStyle` — vertical separator bundle (`type`, `color`, `bg`); `sep.type = SC_BORDER_NONE` = no separator |
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
    ScHAlign  align;      // content placement when col_w > content_w
    int      valign_set; // 1 = override ScColumnsOpts.valign for this column
    ScVAlign valign;     // per-column vertical alignment
    ScColor  bg;         // background for padding spaces and empty slots; zero-init = no color
} ScColItem;
```

Width priority: `fixed_w` > `min_w`/`max_w` clamping > natural content width.
Flex columns (fixed_w == 0) participate in `total_width` distribution.

**Per-column valign:** Set `valign_set = 1` and `valign` to override the global
`ScColumnsOpts.valign` for a single column. Zero-initialized `ScColItem` inherits
the global setting (no override).

**Per-column background (`bg`):** Applied to padding spaces (lp/rp alignment padding)
and empty slots when the column has fewer lines than the tallest column. Uses the
zero-init sentinel (`{0,0,0,0}` = not set), so `(ScColItem){ 0 }` works naturally
with no background. Does not affect the captured widget content itself (which already
has its own embedded ANSI codes).

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

`sc_list_add_sub` attaches a sub-list to an item; the sub-list is owned by the item
and freed when the parent list is freed.

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

**Zero-init of `marker_style`:** Unlike other `ScTextStyle` fields, a zero-initialized
`marker_style` in `ScListOpts` is explicitly treated as "no formatting" by the renderer.
You can safely write `(ScListOpts){ .marker = SC_LIST_NUMBER }` without specifying
`marker_style` and no color escape codes will be emitted for the marker.

**Alignment:** Numbered/alpha/roman markers are right-aligned within a field sized to
the widest marker value in the list (e.g. `VIII.` sets the field width for all items).

**Hanging indent:** Continuation lines of a word-wrapped item are indented to the same
column as the start of the text (i.e., aligned under the first word, not the marker).

**Nesting:** Sub-lists start at `text_start` of the parent item. Each level's
`indent` adds further indentation relative to that base.

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

`value`/`max` convention: if `max > 0`, ratio = value/max; if `max == 0`, value is already a 0.0–1.0 ratio.
`show_value` only takes effect when `max > 0`.

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
| `thresholds` | `ScProgressThresholds` — grouped threshold settings (see below) |
| `thresholds.enabled` | `bool` — switch fill color based on ratio |
| `thresholds.mid` / `thresholds.high` | Ratio thresholds (default 0.5 / 0.75) |
| `thresholds.color_low` / `.color_mid` / `.color_high` | Fill color per range |
| `show_percent` | `bool` — append ` XX%` (default true) |
| `show_value` | `bool` — append `(value/max)` after percent |
| `bar_width` | Inner bar char count; 0 = auto from `width` |
| `width` | Total line width; 0 = terminal width |
| `label_width` | Fixed label column width; 0 = natural width |
| `label_style` | Style for label text |

**Zero-init of `fill_color`/`empty_color`:** Same as every other `ScColor` field — zero-init equals `SC_ANSI_COLOR_NONE` and emits no escape codes. Use `SC_ANSI_COLOR_BLACK` for explicit black.

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

`sc_spinner_tick` advances to the next frame, prints `frame label\r`, and calls `fflush`.
`sc_spinner_finish` clears the line, then prints `✔ label\n` (success=true) or `✖ label\n` (success=false) in green/red.

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

Interactive prompts. **Tty-oriented**, not stream-oriented: they open
`/dev/tty` (fallback stdin/stdout), enter raw mode, read decoded keys, and
redraw in place — they do **not** go through `sc_output_stream()`. Every
widget returns `ScInputStatus` (`SC_INPUT_OK` / `SC_INPUT_CANCELLED` /
`SC_INPUT_ERROR`); Esc and Ctrl-C always cancel; non-TTY contexts return
`SC_INPUT_ERROR` (so callers can fall back to a default). On `SC_INPUT_OK`
the interactive region is erased and a one-line summary is printed in its
place. Header: `input/sparcli_input.h` (in the `sparcli.h` umbrella).

### Architecture

All widgets are thin state machines over one engine, `sc_prompt_run`
(`src/input/prompt.c`), which owns the raw-mode + draw/read/dispatch loop.
Each widget supplies an `ScPromptVTable { render, on_key }`; `render` builds
a frame as an `ScText` and `sc_capture_text`s it — so input reuses the entire
output renderer stack as its view layer. Shared pieces: `ScLineEditor`
(`line_editor.c`, UTF-8 cursor/insert/delete/word-kill) backs text/password
inputs and the fuzzy query; the TTY layer (`src/tty/`) provides raw mode +
signal-safe restore (`term.c`), the escape-sequence decoder (`key.c`), and
the multi-line in-place redraw (`screen.c`).

### Key decoding

`sc_key_decode(buf, len, &key)` is a **pure, unit-tested** decoder
(`ScKey { type, codepoint, bytes }`) handling control bytes, CSI/SS3
sequences (arrows, Home/End, Delete, PageUp/Down, Shift-Tab) and multi-byte
UTF-8. It returns 0 bytes consumed for incomplete prefixes (lone ESC, partial
UTF-8/CSI); the buffered reader `sc_tty_read_key` resolves a lone ESC to
`SC_KEY_ESC` via a 25 ms poll timeout.

### Widgets

```c
ScInputStatus sc_confirm(const char *question, bool *out, ScConfirmOpts opts);
ScInputStatus sc_text_input(const char *prompt, char **out, ScTextInputOpts opts);     /* *out heap; free */
ScInputStatus sc_password_input(const char *prompt, char **out, ScPasswordOpts opts);  /* *out heap; free */
ScInputStatus sc_datepicker(struct tm *io, ScDatePickerOpts opts);                     /* io in/out */

/* Opaque-handle widgets (variable items / per-run config) */
ScSelect *sc_select_new(ScSelectOpts opts);            /* opts.multi = true → checkboxes */
void      sc_select_add(ScSelect *s, const char *label);
ScInputStatus sc_select_run(ScSelect *s, size_t *indices, size_t *count_io); /* count_io in:cap out:written */
void      sc_select_free(ScSelect *s);

ScFuzzy  *sc_fuzzy_new(ScFuzzyOpts opts);              /* opts.table + headers/n_cols → table view */
void      sc_fuzzy_add(ScFuzzy *f, const char *label);
void      sc_fuzzy_add_row(ScFuzzy *f, const char *const *fields, size_t n); /* fields[0] = match key */
ScInputStatus sc_fuzzy_run(ScFuzzy *f, size_t *out_index);  /* out_index = original add order */
void      sc_fuzzy_free(ScFuzzy *f);
bool      sc_fuzzy_match(const char *pattern, const char *str, int *score);  /* pure, testable */
```

- **TextInput/Password** share `sc_text_entry` (`text_input.c`); password is
  the masked variant (`opts.mask`, default `"*"`). Both accept an optional
  `validate` callback that keeps the prompt open and shows an error line.
- **Select** scrolls a viewport (`max_visible`, default 10); `j/k` + arrows
  move, Space toggles in multi-select.
- **Fuzzy** ranks by `sc_fuzzy_match` on each keystroke; table view builds an
  `ScTableData` of the visible rows each frame (cursor row via row-bg) and
  `sc_vstack`s it under the query line.
- **DatePicker** renders a month grid; arrows move day/week, PageUp/Down or
  `<`/`>` change month; zeroed `struct tm` seeds today.

`ScConfirmOpts`/`ScSelectOpts`/`ScFuzzyOpts`/`ScDatePickerOpts` default their
`accent` color when zero-init (`index == 0`).

## Internal Helpers (`src/internal.h`)

Not part of the public API. Used by all source files that include `internal.h`:

| Helper | Description |
|--------|-------------|
| `sc_apply_colors(fg, bg)` | Emits ANSI fg/bg escapes; skips if `index == 0` (zero-init / `SC_ANSI_COLOR_NONE`) |
| `sc_term_width()` | Terminal width via `ioctl(TIOCGWINSZ)`, fallback 80 |
| `sc_utf8_vis_w(s, byte_len)` | Visible column count of a UTF-8 byte sequence |
| `sc_utf8_trim_to_cols(s, max_cols)` | Byte count that fits within `max_cols` columns |

---

## Key Invariants

- **`SC_ANSI_COLOR_NONE` sentinel is `index = 0`**, identical to a zero-initialized `ScColor`. Any unset color field renders as "no color" automatically; no explicit assignment needed. Named colors use `index = 1..8` (BLACK..WHITE); RGB uses `index = -1`.
- `sc_print()` always appends `\033[0m` (reset), even when opts are all-none.
  This is intentional to isolate styling.
- The `h` horizontal-line character from `ScBorderType` is used by both
  panel titles, table titles, rules, and column separators — all from the same
  logical table in each file.
- `ScText` / `ScTableData` / `ScColumns` all heap-allocate; always call the
  corresponding `_free()` function.
- `ScColumns` captures widget output at `sc_columns_add_*` time. Modifying a
  table after adding it to a columns layout has no effect.
- Word-wrap in tables (`ScColOpts.wrap = 1`) breaks on spaces only. If no
  space fits in the column width, the line is truncated.
- **Zero-init `ScTextStyle` sentinel** (used by `ScKVOpts.key_style`, `ScKVOpts.val_style`,
  `ScListOpts.marker_style`, `ScBadgeOpts.text_style`): zero-init = no formatting.
  Renderers use `opts_has_format()` to detect this and skip `sc_print()`.

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

**Layout:** `margin + key (padded to key_w) + sep + value + margin`

Continuation lines of a wrapped value are indented by `margin + key_w + sep_w` columns.

---

## Alert Presets

Thin wrappers over `sc_panel_str`/`sc_panel_text` with preset icon, color, and border.

```c
void sc_alert_str    (ScAlertType type, const char *content);
void sc_alert_text   (ScAlertType type, const ScText *content);
void sc_alert_info   (const char *content);
void sc_alert_warning(const char *content);
void sc_alert_error  (const char *content);
void sc_alert_success(const char *content);
```

| Type | Icon | Color |
|------|------|-------|
| `SC_ALERT_INFO` | ℹ | Blue |
| `SC_ALERT_WARNING` | ⚠ | Yellow |
| `SC_ALERT_ERROR` | ✖ | Red |
| `SC_ALERT_SUCCESS` | ✔ | Green |

All alerts render `full_width = 1` with `SC_BORDER_SINGLE` and a colored left-aligned title.
Content may contain `\n` for multi-line bodies.

---

## Badge

Inline styled text token. `sc_print_badge` writes to stdout (no trailing newline).
`sc_text_append_badge` appends the composed badge string as a single span to an `ScText`.

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

Badge string: `left_cap + pad×' ' + text + pad×' ' + right_cap`

---

## Utilities

```c
char *sc_strip_ansi(const char *str);
char *sc_truncate  (const char *str, int max_cols, const char *ellipsis);
void  sc_clear_line(void);
```

- `sc_strip_ansi`: returns a heap-allocated copy of `str` with all ANSI CSI escape
  sequences removed. Caller must `free()` the result.
- `sc_truncate`: if the visible width of `str` exceeds `max_cols`, returns a
  heap-allocated truncated copy with `ellipsis` appended (may be `NULL`). If it
  fits, returns `strdup(str)`. Caller must `free()` the result.
- `sc_clear_line`: writes `\r` + spaces (terminal width) + `\r` + `fflush` to
  overwrite the current terminal line in place.

---

## Capture API

Renders any widget into a heap-allocated `ScRendered`. Caller must free with
`sc_rendered_free()`. Use the result with `sc_pad_print`, `sc_align_print`, or
`sc_columns_add_rendered`.

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
ScRendered *sc_capture_rule_str   (const char *title, ScRuleOpts opts);
ScRendered *sc_capture_rule_text  (const ScText *title, ScRuleOpts opts);
```

The same `ScRendered *` can be passed to multiple print functions (e.g. first
`sc_pad_print`, then `sc_align_print`).

### sc_vstack — stack widgets vertically in one column

```c
ScRendered *sc_vstack(const ScRendered *const *parts, size_t n, int gap);
```

Concatenates `n` captured renderings top-to-bottom into a single `ScRendered`,
with `gap` blank lines between adjacent parts. This is how you place **two (or
more) widgets one above the other inside a single column** — capture each
widget, `sc_vstack` them, then `sc_columns_add_rendered` the result.

Inputs are **not** consumed: the caller still owns every `parts[i]` and frees
them (and the returned value) with `sc_rendered_free`. Returns `NULL` when
`n == 0`; `gap` is clamped to `>= 0`. `max_column_width` of the result is the
widest line across all parts.

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

`sc_pad_print` prints `top` blank lines, then each content line with `left` spaces
prepended and `right` spaces appended, then `bottom` blank lines.

`right` padding (trailing spaces per line) is mostly useful in composed contexts
(e.g. coloured backgrounds); it has no visible effect on a plain terminal.

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
/* width = 0 → sc_term_width(); width > 0 → fixed column count */
void sc_align_print(const ScRendered *r, ScHAlign align, int width);
void sc_align_str  (const char *s,       ScHAlign align, int width);
void sc_align_text (const ScText *t,     ScHAlign align, int width);
```

Aligns every line of the rendered output within `width` columns.
`SC_ALIGN_LEFT` is a no-op (prints as-is).

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

Inserts an already-captured `ScRendered` into a `ScColumns` layout.
The columns layout makes a deep copy, so the caller may free `r` immediately after.

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
| `[[` | literal `[` character |
| `[blink]` (any unrecognized) | emitted verbatim including brackets |

Tags stack: `[bold][red]text[/] still bold[/]` — closing pops the top frame.

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
sc_text_append(t, "prefix — ", (ScTextStyle){0});
sc_markup_append(t, "[green]green suffix[/]");
sc_print_text(t);
sc_text_free(t);
```

### ScMarkupOpts — parser options

```c
typedef struct {
    int strip_unknown; /* 1 = silently remove unrecognized tags; 0 = verbatim (default) */
} ScMarkupOpts;
```

Controls what happens with unrecognized tags like `[blink]`, `[link=...]`, `[strike]`:

| `strip_unknown` | `[blink]hello[/blink]` becomes |
|-----------------|-------------------------------|
| `0` (default)   | `[blink]hello[/blink]` — brackets printed as literal text |
| `1`             | `hello` — tag brackets silently removed, content kept |

```c
/* strip unknown tags — only content is kept */
sc_markup_println_opts("[blink]text[/blink]", (ScMarkupOpts){ .strip_unknown = 1 });

/* mixed: known tags styled, unknown tags stripped */
ScText *t = sc_markup_parse_opts(
    "[bold]important[/] [blink]ignored-tag[/blink] suffix",
    (ScMarkupOpts){ .strip_unknown = 1 }
);
```

### SC_CELL_M — markup in tables

```c
#define SC_CELL_M(s)  /* creates an SC_CELL_MARKUP cell */
```

The cell **owns** the parsed `ScText`; `sc_table_free` frees it automatically.
No separate free needed.

```c
sc_table_add_row(t, (ScCell[]){
    SC_CELL_M("[green]✔ OK[/]"),
    SC_CELL_M("Build [bold]passed[/]"),
}, 2);
sc_table_free(t);  /* frees markup ScText automatically */
```

**Unknown tags:** Any tag with an unrecognized token is emitted verbatim by default
(including brackets). Use `ScMarkupOpts{ .strip_unknown = 1 }` to silently discard them.

**`[[` escape:** Two consecutive opening brackets produce a single literal `[`.
