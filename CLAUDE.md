# sparcli — Developer Reference

A C11 library for styled terminal output: colored text, bordered panels,
feature-rich tables, horizontal rules, and multi-column side-by-side layouts.

## Build

```sh
make          # builds libsparcli.a
make test     # compiles and runs tests/test_main
make clean    # removes .o, .a, test binary
```

Compiler: `cc -std=c11 -Wall -Wextra -Iinclude`

Source files in `SRC` (Makefile):
```
src/style.c  src/panel.c  src/color.c  src/text.c
src/table.c  src/columns.c  src/rule.c  src/tree.c  src/list.c
src/progressbar.c  src/spinner.c
src/kv.c  src/alert.c  src/badge.c  src/util.c  src/pad.c  src/markup.c
```

When adding a new source file, append it to `SRC` in the Makefile.

---

## Core Types

### ScColor

```c
typedef struct { int index; uint8_t r, g, b; } ScColor;
```

| `index` | Meaning |
|---------|---------|
| `-2`    | **Not set** — `SC_COLOR_NONE` — no escape code emitted |
| `-1`    | 24-bit RGB mode; uses `r`, `g`, `b` fields |
| `0`–`7` | Named ANSI color (`SC_COLOR_BLACK` … `SC_COLOR_WHITE`) |

**Critical:** Zero-initializing a `ScColor` struct gives `index=0` = `SC_COLOR_BLACK`,
NOT "no color". Always set `.color = SC_COLOR_NONE` explicitly when you don't want
a color, especially in `ScColumnsOpts.sep_color`, `ScRuleOpts.color`, etc.

```c
ScColor sc_color_from_rgb(uint8_t r, uint8_t g, uint8_t b);  // index = -1
```

Named macros: `SC_COLOR_NONE`, `SC_COLOR_BLACK`, `SC_COLOR_RED`, `SC_COLOR_GREEN`,
`SC_COLOR_YELLOW`, `SC_COLOR_BLUE`, `SC_COLOR_MAGENTA`, `SC_COLOR_CYAN`, `SC_COLOR_WHITE`.

### ScStyle

Bitmask — combine with `|`:

```c
SC_STYLE_NONE | SC_STYLE_BOLD | SC_STYLE_DIM | SC_STYLE_ITALIC | SC_STYLE_UNDER
```

### ScOptions

```c
typedef struct { ScStyle style; ScColor fg; ScColor bg; } ScOptions;
```

Used everywhere a styled text span is needed (cell headers, titles, rule labels, …).

### ScText / ScSpan

Multi-span rich text. Each span has its own `ScOptions`.

```c
ScText *t = sc_text_new();
sc_text_append(t, "hello ",  (ScOptions){ SC_STYLE_NONE, SC_COLOR_NONE, SC_COLOR_NONE });
sc_text_append(t, "world",   (ScOptions){ SC_STYLE_BOLD, SC_COLOR_GREEN, SC_COLOR_NONE });
sc_print_text(t);
sc_text_free(t);
```

`sc_text_visible_width(t)` — returns the max visible width across lines (ANSI-aware,
UTF-8-aware, counts codepoints not bytes).

### ScBorderStyle

```c
SC_BORDER_NONE | SC_BORDER_ASCII | SC_BORDER_SINGLE |
SC_BORDER_DOUBLE | SC_BORDER_ROUNDED | SC_BORDER_THICK
```

Used by panels, tables, rules, and column separators.

---

## Panels

```c
void sc_panel_str (const char *content, ScPanelOpts opts);
void sc_panel_text(const ScText *content, ScPanelOpts opts);
```

### ScPanelOpts

| Field | Type | Description |
|-------|------|-------------|
| `border` | `ScBorderStyle` | Frame style |
| `border_color` | `ScColor` | Frame color |
| `title` | `const char *` | NULL = no title |
| `title_opts` | `ScOptions` | Style for title text |
| `title_pos` | `SC_TITLE_TOP` / `SC_TITLE_BOTTOM` | |
| `title_align` | `ScAlign` | LEFT / CENTER / RIGHT |
| `title_pad` | `int` | Spaces on each side of title text, default 1 |
| `pad_x` | `int` | Horizontal content padding |
| `pad_y` | `int` | Vertical content padding (empty lines) |
| `width` | `int` | 0 = auto-fit content |
| `content_align` | `ScAlign` | Horizontal alignment of content lines |
| `full_width` | `int` | 1 = stretch to terminal width (overrides `width`) |

`full_width` takes precedence over `width`. Width is calculated as `terminal_width - 2`.

---

## Tables

```c
ScTable *sc_table_new(ScTableOpts opts);
void     sc_table_add_col(ScTable *t, const char *header, ScColOpts col);
void     sc_table_add_row(ScTable *t, ScCell *cells, size_t n);
void     sc_table_add_row_bg(ScTable *t, ScCell *cells, size_t n, ScColor bg);
void     sc_table_add_footer_row(ScTable *t, ScCell *cells, size_t n);
void     sc_table_print(const ScTable *t);
void     sc_table_free(ScTable *t);
```

### ScTableOpts

| Field | Description |
|-------|-------------|
| `borders` | `ScTableBorders` — style + color per border segment |
| `header_row` | 1 = first added row is the header |
| `header_col` | 1 = first column is a header column |
| `header_row_bg` / `header_col_bg` | Background for header areas |
| `header_opts` | `ScOptions` applied to all header cells |
| `striped` | 1 = alternating row backgrounds |
| `stripe_bg` | Background for odd data rows (0-indexed) |
| `footer_row_bg` / `footer_col_bg` | Background for footer rows/column |
| `footer_opts` | `ScOptions` for footer cells |
| `title` | Table title string |
| `title_opts` / `title_pos` / `title_align` / `title_pad` | Title appearance |
| `cell_pad_x` / `cell_pad_y` | Cell padding |
| `total_width` | 0 = auto; >0 = distribute width across flex columns |
| `max_rows` | 0 = unlimited; >0 = truncate with indicator |
| `rtl` | 1 = right-to-left column order |

### ScTableBorders

```c
typedef struct {
    ScBorderStyle style;
    ScColor       outer_color;
    ScColor       inner_color;
    ScColor       header_row_sep_color;
    ScColor       header_col_sep_color;
    int           no_outer;      // suppress outer frame
    int           no_inner_h;    // suppress inner row separators
    int           no_inner_v;    // suppress inner col separators (except header col)
} ScTableBorders;
```

### ScColOpts

```c
typedef struct {
    int      min_w;
    int      max_w;
    int      fixed_w;
    ScAlign  align;
    ScValign valign;
    int      wrap;    // 1 = word-wrap, 0 = truncate
    ScColor  bg;      // per-column background
} ScColOpts;
```

**Background priority:** `header/footer bg > per-row bg > stripe bg > col bg`.
Per-column bg is the lowest-priority fallback: only applied when no row-level
background is active (`row_bg.index == -2`).

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
| `style` | `ScBorderStyle` — which `h` character to use |
| `color` | Line color; `SC_COLOR_NONE` = no escape codes |
| `title_opts` | `ScOptions` for the title text |
| `title_align` | LEFT / CENTER / RIGHT (default CENTER) |
| `title_pad` | Spaces on each side of title, default 1 |
| `width` | 0 = full terminal width; >0 = fixed width |
| `align` | Placement of the rule when `width > 0` (LEFT/CENTER/RIGHT) |
| `margin` | Symmetric left+right indent in chars |
| `pad_y` | Blank lines printed above and below the rule |

**Zero-init pitfall:** `color` defaults to `SC_COLOR_BLACK` if not set. Always
specify `.color = SC_COLOR_NONE` for an uncolored rule.

---

## Columns

Renders multiple widgets side by side using a capture-and-replay approach:
each widget is rendered into a `tmpfile()` via `dup2`, then lines are zipped
horizontally.

```c
ScColumns *sc_columns_new(ScColumnsOpts opts);
void sc_columns_add_table     (ScColumns *cl, const ScTable   *t,       ScColItem item);
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
| `sep_style` | `ScBorderStyle` for the vertical separator; `SC_BORDER_NONE` = no separator |
| `sep_color` | Separator color; **must be set to `SC_COLOR_NONE`** if no color desired |
| `valign` | `SC_VALIGN_TOP` / `SC_VALIGN_MIDDLE` / `SC_VALIGN_BOTTOM` |
| `total_width` | 0 = auto; >0 = distribute across flex columns |

**Zero-init pitfall:** `sep_color` defaults to `SC_COLOR_BLACK`. Always set
`.sep_color = SC_COLOR_NONE` when using a separator without a color.

### ScColItem

Per-column options passed with each `sc_columns_add_*` call:

```c
typedef struct {
    int     min_w;   // 0 = none
    int     max_w;   // 0 = none
    int     fixed_w; // 0 = auto; fixed_w > 0 ignores min_w/max_w
    ScAlign align;   // content placement when col_w > content_w
} ScColItem;
```

Width priority: `fixed_w` > `min_w`/`max_w` clamping > natural content width.
Flex columns (fixed_w == 0) participate in `total_width` distribution.

---

## List

Bulleted and numbered lists with word-wrap, hanging indent, and arbitrary nesting.

```c
ScList     *sc_list_new     (ScListOpts opts);
ScListItem *sc_list_add_str (ScList *l, const char *str, ScOptions opts);
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
| `marker_opts` | Style/color for the marker; **zero-init = no formatting** (see below) |
| `indent` | Left indent relative to parent, in columns |
| `item_gap` | Blank lines between items |
| `width` | 0 = terminal width |
| `margin` | Symmetric left+right outer margin |

**Zero-init of `marker_opts`:** Unlike other `ScOptions` fields, a zero-initialized
`marker_opts` in `ScListOpts` is explicitly treated as "no formatting" by the renderer.
You can safely write `(ScListOpts){ .marker = SC_LIST_NUMBER }` without specifying
`marker_opts` and no color escape codes will be emitted for the marker.

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

### ScProgressStyle

| Constant | Appearance |
|----------|-----------|
| `SC_PROGRESS_BLOCK` | `█` / `░` |
| `SC_PROGRESS_ASCII` | `=` + `>` edge / ` ` |
| `SC_PROGRESS_LINE` | `━` / `╌` |
| `SC_PROGRESS_SHADED` | `▓` / `▒` edge / `░` |

### ScProgressBarOpts

| Field | Description |
|-------|-------------|
| `style` | Fill style (see above) |
| `left_cap` / `right_cap` | Border strings; `NULL` = no bracket; pass `"["` / `"]"` for defaults |
| `fill_color` / `empty_color` | Colors; zero-init = no color (same sentinel as `marker_opts`) |
| `use_thresholds` | 1 = switch fill color based on ratio; 0 = use `fill_color` only |
| `threshold_mid` / `threshold_high` | Ratio thresholds (default 0.5 / 0.75) |
| `color_low` / `color_mid` / `color_high` | Fill color per range |
| `show_percent` | 1 = append ` XX%` (default 1) |
| `show_value` | 1 = append `(value/max)` after percent |
| `bar_width` | Inner bar char count; 0 = auto from `width` |
| `width` | Total line width; 0 = terminal width |
| `label_width` | Fixed label column width; 0 = natural width |
| `label_opts` | Style for label text |

**Zero-init of `fill_color`/`empty_color`:** Same as other color fields — zero-initialized `ScColor` (index=0, rgb=0) is treated as "no color" by the renderer. Use `sc_color_from_rgb(0,0,0)` for explicit black.

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
void       sc_spinner_finish   (ScSpinner *s, int success, const char *label);
void       sc_spinner_free     (ScSpinner *s);
```

`sc_spinner_tick` advances to the next frame, prints `frame label\r`, and calls `fflush`.
`sc_spinner_finish` clears the line, then prints `✔ label\n` (success=1) or `✖ label\n` (success=0) in green/red.

### ScSpinnerStyle

| Constant | Frames |
|----------|--------|
| `SC_SPINNER_BRAILLE` | `⠋⠙⠹⠸⠼⠴⠦⠧⠇⠏` (10 frames) |
| `SC_SPINNER_PIPE` | `\|/-\` (4 frames) |
| `SC_SPINNER_DOTS` | `⣾⣽⣻⢿⡿⣟⣯⣷` (8 frames) |
| `SC_SPINNER_ARROW` | `←↖↑↗→↘↓↙` (8 frames) |

### ScSpinnerOpts

| Field | Description |
|-------|-------------|
| `style` | Frame style (see above) |
| `color` | Spinner character color; zero-init = no color |
| `label_opts` | Style for label text; zero-init = no formatting |

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

## Internal Helpers (`src/internal.h`)

Not part of the public API. Used by all source files that include `internal.h`:

| Helper | Description |
|--------|-------------|
| `sc_apply_colors(fg, bg)` | Emits ANSI fg/bg escapes; skips if `index == -2` |
| `sc_term_width()` | Terminal width via `ioctl(TIOCGWINSZ)`, fallback 80 |
| `sc_utf8_vis_w(s, byte_len)` | Visible column count of a UTF-8 byte sequence |
| `sc_utf8_trim_to_cols(s, max_cols)` | Byte count that fits within `max_cols` columns |

---

## Key Invariants

- **`SC_COLOR_NONE` sentinel is `index = -2`**, not 0. Zero-initialized `ScColor`
  is `SC_COLOR_BLACK`. This is the most common source of unexpected coloring.
- `sc_print()` always appends `\033[0m` (reset), even when opts are all-none.
  This is intentional to isolate styling.
- The `h` horizontal-line character from `ScBorderStyle` is used by both
  panel titles, table titles, rules, and column separators — all from the same
  logical table in each file.
- `ScText` / `ScTable` / `ScColumns` all heap-allocate; always call the
  corresponding `_free()` function.
- `ScColumns` captures widget output at `sc_columns_add_*` time. Modifying a
  table after adding it to a columns layout has no effect.
- Word-wrap in tables (`ScColOpts.wrap = 1`) breaks on spaces only. If no
  space fits in the column width, the line is truncated.
- **Zero-init `ScOptions` sentinel** (used by `ScKVOpts.key_opts`, `ScKVOpts.val_opts`,
  `ScListOpts.marker_opts`, `ScBadgeOpts.text_opts`): zero-init = no formatting.
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
| `key_opts` | Style for key text; **zero-init = no formatting** |
| `val_opts` | Style for value text; **zero-init = no formatting** |

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
| `text_opts` | Style for the full badge (caps + padding + text); **zero-init = no formatting** |
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
ScRendered *sc_capture_table      (const ScTable *t);
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
ScRendered *r = sc_capture_table(t);
sc_pad_print(r, (ScPadOpts){ .top = 1, .bottom = 1, .left = 4 });
sc_rendered_free(r);

/* convenience (one step): */
sc_pad_str("Hello", (ScPadOpts){ .left = 8 });
```

---

## Align

```c
/* width = 0 → sc_term_width(); width > 0 → fixed column count */
void sc_align_print(const ScRendered *r, ScAlign align, int width);
void sc_align_str  (const char *s,       ScAlign align, int width);
void sc_align_text (const ScText *t,     ScAlign align, int width);
```

Aligns every line of the rendered output within `width` columns.
`SC_ALIGN_LEFT` is a no-op (prints as-is).

**Usage:**
```c
ScRendered *r = sc_capture_table(t);
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
ScRendered *r = sc_capture_table(t);
sc_columns_add_rendered(cl, r, (ScColItem){ 0 });
sc_rendered_free(r);   /* safe: columns owns its own copy */
```

---

## Markup

Rich-compatible inline markup. Parse a string into an `ScText *` or print directly.

### Syntax

| Tag | Effect |
|-----|--------|
| `[bold]` | `SC_STYLE_BOLD` |
| `[italic]` | `SC_STYLE_ITALIC` |
| `[underline]` / `[u]` | `SC_STYLE_UNDER` |
| `[dim]` | `SC_STYLE_DIM` |
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
sc_text_append(t, "prefix — ", (ScOptions){0});
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
