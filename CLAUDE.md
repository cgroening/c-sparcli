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
ScColor sc_rgb(uint8_t r, uint8_t g, uint8_t b);  // index = -1
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
