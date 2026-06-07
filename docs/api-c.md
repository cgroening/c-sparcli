# sparcli – C API Reference

Full reference for every public type, function, option struct, and macro of the **C API**. For the header-only C++ wrapper see [`api-cpp.md`](api-cpp.md); for installation, linking and a quick-start example, see the [main README](../README.md).

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

### Named RGB palette (`SC_COLOR_*`)

`include/core/sparcli_palette.h` (included via `sparcli.h`) defines a curated 24-bit **RGB** palette of `SC_COLOR_*` compound-literal macros (`index == -1`) — additional to, and distinct from, the terminal-palette `SC_ANSI_COLOR_*`. Zero-init remains `SC_ANSI_COLOR_NONE`; the palette is opt-in.

```c
sc_panel_str("hi", (ScPanelOpts){
    .border = { SC_BORDER_ROUNDED, SC_COLOR_ACCENT, SC_ANSI_COLOR_NONE },
    .bg     = SC_COLOR_BG_DARKEN_1,
});
sc_alert_text(SC_ALERT_ERROR, sc_markup_parse("[error]disk full[/]"));
```

The 53 colors, by group:

| Group | Macros |
|-------|--------|
| Standard hues | `SC_COLOR_RED` `_ORANGE` `_YELLOW` `_GREEN` `_CYAN` `_BLUE` `_PURPLE` `_MAGENTA` `_BLACK` `_WHITE` |
| Vivid | `SC_COLOR_<HUE>_VIVID` (each of the 8 hues) |
| Dark | `SC_COLOR_<HUE>_DARK` (each of the 8 hues) |
| Backgrounds | `SC_COLOR_BG` `_BG_DARKEN_1` `_BG_DARKEN_2` `_BG_LIGHTEN_1` `_BG_LIGHTEN_2` `_BG_LIGHTEN_3` `_BG_SELECTED` |
| Foregrounds | `SC_COLOR_FG` `_FG_DARKEN_1` `_FG_DARKEN_2` `_FG_DARKEN_3` `_FG_LIGHTEN_1` `_FG_LIGHTEN_2` |
| Accents | `SC_COLOR_ACCENT` `_ACCENT_DIM` `_ACCENT_DARKER` `_ACCENT_DARK` `_ACCENT_IMPORTANT` |
| Status | `SC_COLOR_ENABLED` `_DISABLED` `_DISABLED_DIM` |
| Diagnostics | `SC_COLOR_ERROR` `_WARNING` `_SUCCESS` `_INFO` `_HINT` `_UNUSED` |

Each macro's source `#rrggbb` is in a comment in the header.

**Resolve a color by name:**

```c
bool sc_color_by_name  (const char *name, ScColor *out);
bool sc_color_by_name_n(const char *name, size_t length, ScColor *out);  // span
```

This is the single canonical name→`ScColor` resolver, shared by markup (`[accent]`, `[error]`, `[orange]`, …), the CLI (`--color accent`) and the args parser (`SC_ARG_COLOR`). It knows the eight ANSI names *and* the palette. The eight plain hue names (`"red"`, `"green"`, … `"white"`) always resolve to the **ANSI** colors; the palette's soft hues are not reachable by bare name — use `#rrggbb` or the `SC_COLOR_*` macro. Every other palette name (`"orange"`, `"purple"`, the `*_vivid`/`*_dark` variants, `bg*`, `fg*`, `accent*`, `enabled`, `disabled*`, `error`, `warning`, `success`, `info`, `hint`, `unused`) is string-addressable.

### ScTextAttribute

Bitmask – combine with `|`:

```c
SC_TEXT_ATTR_NONE | SC_TEXT_ATTR_BOLD | SC_TEXT_ATTR_DIM |
SC_TEXT_ATTR_ITALIC | SC_TEXT_ATTR_UNDER
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
sc_text_append(t, "hello ",
    (ScTextStyle){ SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE });
sc_text_append(t, "world",
    (ScTextStyle){ SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_GREEN, SC_ANSI_COLOR_NONE });
sc_print_text(t);
sc_text_free(t);
```

`sc_text_visible_width(t)` – returns the max visible width across lines (ANSI-aware and **display-width-aware**: East-Asian Wide / Fullwidth characters and emoji count as 2 columns, combining marks as 0; ambiguous symbols stay 1).

`sc_text_append_link(t, "text", "https://…", style)` – appends a span wrapped in an OSC-8 terminal hyperlink; see [Hyperlinks (OSC-8)](#hyperlinks-osc-8).

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
    const ScText    *rich_text; /* optional rich title; overrides text+style (panels only) */
    ScTextStyle      style; /* text rendering (bold, color, …) */
    ScHAlign         halign; /* LEFT / CENTER / RIGHT */
    int              pad;   /* spaces on each side of the title text */
    ScPosition  pos;   /* SC_POSITION_TOP / SC_POSITION_BOTTOM; unused for rules */
} ScTitle;
```

Used directly everywhere a title is needed. `pos` is ignored by rules (no top/bottom distinction). `rich_text` (when non-`NULL`) renders a multi-style title and is honored by panels (incl. boxed input prompts); rules and tables ignore it.

Access paths:
- `rule_opts.title.text` / `.style` / `.halign` / `.pad`
- `panel_opts.title.text` / `.style` / `.halign` / `.pad` / `.pos`

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

Table sources live in `src/table/` and are chained via `#include`: `table.c` → `table_print.c` → `table_print_init.c` → `table_print_render.c` → `table_print_render_cell.c`, `table_print_render_border.c`, `table_print_render_row.c`. Internal types are declared in `src/table/table_internal.h`.

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

**Per-column text style (`style`):** a default `ScTextStyle` (attributes + foreground) applied to every cell span in the column that carries no styling of its own. Priority: per-cell/markup style > `header.style`/`footer.style` (section) > column `style`. The fallback is per-field – e.g. a bold-only header style still picks up the column's foreground color. The style's `bg` member is ignored; use the `bg` field for column backgrounds.

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

The opts (including the `bullet`/`marker_prefix`/`marker_suffix` strings) are **copied** by `sc_list_new`/`sc_list_add_sub`, and item strings are copied by `sc_list_add_str` – the caller's buffers only need to live until the respective call returns. Rich-text items (`sc_list_add_text`) stay borrowed and must outlive printing.

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

## Tree

Hierarchical tree view with box-drawing connectors and vertical continuation guides. Nodes are added under an optional parent; a `NULL` parent makes a root.

```c
ScTree     *sc_tree_new (ScTreeOpts opts);
ScTreeNode *sc_tree_add_str (ScTree *tree, ScTreeNode *parent,
                             const char *str, ScTextStyle style,
                             const char *prefix, ScTextStyle prefix_style);
ScTreeNode *sc_tree_add_text(ScTree *tree, ScTreeNode *parent,
                             const ScText *text,
                             const char *prefix, ScTextStyle prefix_style);
void        sc_tree_print(const ScTree *tree);
void        sc_tree_free (ScTree *tree);
```

`ScTree` and `ScTreeNode` are opaque; nodes are owned by the tree (freed by `sc_tree_free`). `sc_tree_add_str` copies `str` and `prefix`; `sc_tree_add_text` **borrows** the `ScText` (not owned – keep it alive until print/free). `prefix` may be `NULL`.

### ScTreeOpts

| Field | Description |
|-------|-------------|
| `type` | `ScBorderType` – connector box-drawing set (branches and guides) |
| `connector_color` | Connector color; `SC_ANSI_COLOR_NONE` = no escape codes |
| `indent` | Spaces between the connector and node text; default `1` |
| `no_guide` | `bool` – suppress vertical continuation guides under finished branches |
| `ansi` | `ScAnsiMode` – ANSI passthrough for node strings; zero-init inherits the `sc_set_allow_ansi` global |

**Usage:**

```c
ScTree *tree = sc_tree_new((ScTreeOpts){ .type = SC_BORDER_SINGLE });
ScTreeNode *root = sc_tree_add_str(tree, NULL, "project",
                                   (ScTextStyle){0}, NULL, (ScTextStyle){0});
sc_tree_add_str(tree, root, "src",     (ScTextStyle){0}, NULL, (ScTextStyle){0});
sc_tree_add_str(tree, root, "include", (ScTextStyle){0}, NULL, (ScTextStyle){0});
sc_tree_print(tree);
sc_tree_free(tree);
```

**Integration with ScColumns / Capture:** use `sc_columns_add_tree(cl, tree, item)` or `sc_capture_tree(tree)`.

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

The opts (including the `left_cap`/`right_cap` strings) are **copied** by `sc_progressbar_new`, so the caller's buffers only need to live until the call returns.

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

## ANSI Sanitization (escape-injection protection)

Every user-supplied string that enters the library (panel/rule content, table cells, list items, key/value entries, tree nodes, spinner/progress-bar labels, badge text, markup text, titles, `sc_print`, `sc_text_append`) is **sanitized at the API boundary**: control bytes (everything below `0x20` except `\n`/`\t`, plus DEL) and ANSI escape sequences are removed. Untrusted data can therefore never inject terminal escape codes.

```c
void sc_set_allow_ansi(bool allow);  /* process-wide default; false = strip */
bool sc_allow_ansi(void);            /* current setting */
```

### ScAnsiMode – per-widget override

```c
typedef enum ScAnsiMode {
    SC_ANSI_MODE_DEFAULT  = 0,  /* inherit sc_set_allow_ansi() (the default) */
    SC_ANSI_MODE_ALLOW    = 1,  /* pass well-formed escape sequences through */
    SC_ANSI_MODE_SANITIZE = 2,  /* always strip escape sequences */
} ScAnsiMode;
```

Every output widget opts struct (`ScPanelOpts`, `ScRuleOpts`, `ScTableOpts`, `ScListOpts`, `ScKVOpts`, `ScTreeOpts`, `ScSpinnerOpts`, `ScProgressBarOpts`, `ScBadgeOpts`, `ScMarkupOpts`) carries an `ScAnsiMode ansi` field. Zero-init inherits the global setting; the explicit values override it for that widget only.

**When ANSI is allowed** (globally or per widget):

- Well-formed escape sequences (CSI `ESC[…m`, OSC `ESC]…BEL`, DCS, two-char `ESC c`, …) pass through verbatim.
- Stray control bytes (`\a`, `\b`, lone ESC, …) are **still** removed.
- All width calculations skip escape sequences, so borders, tables and frames stay aligned.

```c
/* Strings with raw ANSI render correctly inside frames: */
sc_set_allow_ansi(true);
sc_panel_str("\033[31mred\033[0m text", opts);          /* borders aligned */

/* Or per widget, with the global default staying strict: */
sc_panel_str(user_data, (ScPanelOpts){ .ansi = SC_ANSI_MODE_ALLOW });
```

**Always filtered regardless of any setting:** content read back from the external editor (`sc_text_input`/`sc_textarea` with `external_editor`), and keyboard input (escape sequences are decoded as keys, never inserted as text).

---

## Utilities

```c
char *sc_strip_ansi(const char *str);
char *sc_truncate  (const char *str, int max_cols, const char *ellipsis);
void  sc_clear_line(void);
```

- `sc_strip_ansi`: returns a heap-allocated copy of `str` with **all** ANSI escape sequences removed – CSI (`ESC[`…), OSC (`ESC]`…`BEL`/`ESC\`), DCS/SOS/PM/APC string sequences, two-character ESC sequences and lone ESC bytes. Other bytes (including `\t`) are copied verbatim. Caller must `free()` the result.
- `sc_truncate`: if the visible width of `str` exceeds `max_cols`, returns a heap-allocated truncated copy with `ellipsis` appended (may be `NULL`). If it fits, returns `strdup(str)`. Caller must `free()` the result.
- `sc_clear_line`: writes `\r` + spaces (terminal width) + `\r` + `fflush` to overwrite the current terminal line in place.

---

## Human-readable formatting

Header `core/sparcli_humanize.h` (in the `sparcli.h` umbrella). Each call returns a fresh heap string (caller `free`s) or `NULL` on OOM; output is plain text.

```c
char *sc_humanize_bytes(uint64_t bytes, ScByteUnit unit);
char *sc_humanize_bytes_opts(uint64_t bytes, ScByteUnit unit, ScHumanizeOpts opts);
char *sc_humanize_number(double value, ScHumanizeOpts opts);
char *sc_humanize_compact(double value, ScHumanizeOpts opts);
char *sc_humanize_percent(double ratio, ScHumanizeOpts opts);
char *sc_humanize_duration(double seconds);
char *sc_humanize_duration_clock(double seconds);
char *sc_humanize_relative(time_t when, time_t now);
```

| Function | Example |
|----------|---------|
| `sc_humanize_bytes(1536, SC_BYTES_SI)` | `"1.5 KB"` |
| `sc_humanize_bytes(1536, SC_BYTES_IEC)` | `"1.5 KiB"` |
| `sc_humanize_number(1234567, {0})` | `"1,234,567"` |
| `sc_humanize_compact(12400, {0})` | `"12.4k"` (suffixes `k M B T`) |
| `sc_humanize_percent(0.42, {0})` | `"42%"` |
| `sc_humanize_duration(8054)` | `"2h 14m"` (two units; `<60s` → `"Ns"`) |
| `sc_humanize_duration_clock(3725)` | `"01:02:05"` |
| `sc_humanize_relative(now-10800, now)` | `"3 hours ago"` (English) |

`ScByteUnit`: `SC_BYTES_SI` (1000-based, default), `SC_BYTES_IEC` (1024-based `KiB…`), `SC_BYTES_IEC_SHORT` (1024-based `K/M/G…`).

`ScHumanizeOpts` (zero-init friendly): `int decimals` (`0` = per-function default), `char decimal_sep` (`0` = `'.'`; `','` for de_DE), `char group_sep` (`0` = `','` thousands separator for `sc_humanize_number`; `'.'` for de_DE), `bool no_space` (drop the space before size units).

---

## Multi Progress

Header `output/sparcli_multiprogress.h` (umbrella). Several progress bars updated together in place; built on `ScProgressBar` + the live display, so off a terminal only the final stack prints.

```c
ScMultiProgress *sc_multiprogress_begin(ScMultiProgressOpts opts); /* .transient/.always */
int  sc_multiprogress_add(ScMultiProgress *mp, const char *label, ScProgressBarOpts opts);
void sc_multiprogress_update(ScMultiProgress *mp, int index, double value, double max);
void sc_multiprogress_set_label(ScMultiProgress *mp, int index, const char *label);
void sc_multiprogress_end(ScMultiProgress *mp);
```

`add` returns the bar index (`-1` on failure); each `update`/`set_label`/`add` redraws the whole stack. Per-bar `ScProgressBarOpts` are copied (like `sc_progressbar_new`); `max == 0` treats `value` as a 0..1 ratio.

---

## Diff

Header `output/sparcli_diff.h` (umbrella). Colored line-based unified diff (LCS).

```c
ScText     *sc_diff_text   (const char *old, const char *new, ScDiffOpts opts);
void        sc_diff_print  (const char *old, const char *new, ScDiffOpts opts);
ScRendered *sc_capture_diff(const char *old, const char *new, ScDiffOpts opts);
```

`ScDiffOpts` (zero-init = 3 context lines, `old`/`new` labels, red/green/cyan): `int context` (`0` = default 3, negative = full file), `bool no_header`, `const char *old_label`/`new_label`, `ScColor add_color`/`del_color`/`hunk_color`. Identical inputs produce an empty `ScText`. The diffed text is ANSI-sanitized at the boundary.

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
ScRendered *sc_capture_rule_str   (const char *title, ScRuleOpts opts);
ScRendered *sc_capture_rule_text  (const ScText *title, ScRuleOpts opts);
```

The same `ScRendered *` can be passed to multiple print functions (e.g. first `sc_pad_print`, then `sc_align_print`).

### sc_vstack – stack widgets vertically in one column

```c
ScRendered *sc_vstack(const ScRendered *const *parts, size_t n, int gap);
```

Concatenates `n` captured renderings top-to-bottom into a single `ScRendered`, inserting `gap` blank lines between adjacent parts. This is how you place **two (or more) widgets one above the other inside a single column**: capture each widget, `sc_vstack` them, then pass the result to `sc_columns_add_rendered`.

Inputs are **not** consumed – the caller still owns every `parts[i]` and frees them (and the returned value) with `sc_rendered_free`. Returns `NULL` when `n == 0`; `gap` is clamped to `>= 0`; the result's `max_column_width` is the widest line across all parts.

**Usage:**

```c
/* Right column = a list with a rule stacked beneath it. */
ScRendered *r_list  = sc_capture_list(list);
ScRendered *r_rule  = sc_capture_rule_str("More", rule_opts);
ScRendered *parts[] = { r_list, r_rule };
ScRendered *col     = sc_vstack((const ScRendered *const *)parts, 2, 1);

sc_columns_add_rendered(cl, col, (ScColItem){ 0 });

sc_rendered_free(col);
sc_rendered_free(r_list);
sc_rendered_free(r_rule);
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
| `[strike]` / `[s]` | `SC_TEXT_ATTR_STRIKE` |
| `[dim]` | `SC_TEXT_ATTR_DIM` |
| `[red]` … `[white]` `[black]` | foreground named color |
| `[on red]` … `[on white]` | background named color |
| `[rgb(r,g,b)]` | foreground RGB |
| `[on rgb(r,g,b)]` | background RGB |
| `[bold red on white]` | combined (all in one tag) |
| `[/]` | close most-recent style frame |
| `[/bold]`, `[/red]`, … | named close (same effect as `[/]`) |
| `[link=URL]text[/link]` | OSC-8 terminal hyperlink (see [Hyperlinks](#hyperlinks-osc-8)) |
| `` `inline code` `` | code span: magenta foreground (configurable via `code_style`), backticks removed; body is literal (tags not parsed) |
| `` \` `` | literal backtick character |
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
    ScTextStyle code_style; /* style for `inline code` spans; zero-init = magenta fg */
} ScMarkupOpts;
```

Controls what happens with unrecognized tags like `[blink]`, `[strike]`:

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

### Backtick inline code

Text between backticks renders as a code span: the backticks are removed and the content gets a magenta foreground by default. The body is **literal** – markup tags and `[[` inside backticks are not parsed (so `` `[bold]` `` shows the tag as code). The code span inherits the surrounding frame's attributes and background; only the foreground is replaced.

```c
sc_markup_println("run `make qa` before committing");          /* magenta code */
sc_markup_println("[bold]bold and `code` mixed[/]");           /* bold + magenta */
sc_markup_println("a literal \\` backtick");                   /* \` escape */
```

| Input | Result |
|-------|--------|
| `` `code` `` | `code` in magenta, backticks removed |
| `` \` `` | literal backtick, no code span |
| `` `dangling `` (unclosed) | backtick kept verbatim, no styling |
| `` `a \` b` `` | code span containing ``a ` b`` |

The style is configurable via `ScMarkupOpts.code_style` (zero-init = magenta foreground). Non-zero fields override the surrounding frame; an unset `code_style.fg` always falls back to magenta:

```c
/* bold cyan instead of magenta */
sc_markup_println_opts("see `code`", (ScMarkupOpts){
    .code_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE },
});
```

---

## Hyperlinks (OSC-8)

Clickable terminal hyperlinks: the visible text is wrapped in OSC-8 escape sequences, so supporting terminals (iTerm2, Kitty, WezTerm, Ghostty, Windows Terminal, GNOME Terminal, …) open the URL on Cmd/Ctrl+click. Terminals without OSC-8 support ignore the sequences and show only the text – output degrades cleanly.

```c
/* API: append a linked span to an ScText */
ScText *t = sc_text_new();
sc_text_append(t, "See the ", (ScTextStyle){ 0 });
sc_text_append_link(t, "documentation", "https://example.com/docs",
    (ScTextStyle){ SC_TEXT_ATTR_UNDER, SC_ANSI_COLOR_CYAN, SC_ANSI_COLOR_NONE });
sc_print_text(t);
sc_text_free(t);

/* Markup: [link=URL]text[/link] (also closable with [/]) */
sc_markup_println("Open [link=https://example.com/docs]the docs[/link] now");
```

Works inside every widget that accepts `ScText` or markup (panels, table cells, lists, …): the escape bytes occupy **zero visible columns**, so width calculations and frame alignment are unaffected.

**Security / sanitization:**

- The **URL** is reduced to printable ASCII (0x20–0x7E): control bytes, ESC and BEL are removed, so a URL can never terminate the sequence early or inject a nested escape sequence.
- The **visible text** crosses the normal trust boundary (sanitized like `sc_text_append`).
- A `NULL` or empty URL appends a plain span without any link bytes.
- Injected OSC-8 sequences in ordinary user strings are still stripped by default – only `sc_text_append_link` and the `[link=…]` tag produce links.

**Markup specifics:**

- The link body is **literal text**: nested tags between `[link=…]` and `[/link]` are not parsed (they appear verbatim). Style the whole link from outside: `[bold][link=…]text[/link][/]`.
- URLs containing `]` cannot be expressed in markup (the tag ends at the first `]`); use `sc_text_append_link` for those.
- `[link=]` (empty URL) is treated as an unknown tag.

---

## Pager

Routes long output through a pager (`$PAGER` / `less -R`) so the user can scroll instead of losing it off-screen. Header: `output/sparcli_pager.h`.

```c
ScPager *sc_pager_begin(ScPagerOpts opts);
int      sc_pager_end(ScPager *pager);
```

### ScPagerOpts

| Field | Description |
|-------|-------------|
| `command` | Pager command, split on whitespace and executed **without a shell**; `NULL`/empty = `$PAGER`, then `less -R` |
| `always` | `true` = page even when the output stream is not a terminal |

Between `sc_pager_begin` and `sc_pager_end`, everything written through `sc_output_stream()` on the calling thread (all widgets and prints) is piped into the pager process. `sc_pager_end` flushes, waits for the pager to exit, restores the previous output stream, and returns the pager's exit status.

```c
ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });
sc_table_print(table, opts);          /* paged */
sc_markup_println("[dim]end of report[/]");
int status = sc_pager_end(pager);     /* user quit less */
```

**Behavior:**

- **Non-TTY output** (pipe, file, capture, CI): paging is skipped, output goes through unchanged, `sc_pager_end` returns `0` – the same code works in scripts.
- **`SIGPIPE`-safe:** quitting the pager early (`q` in less) never kills the program; the previous `SIGPIPE` disposition is restored on end.
- **Missing pager command:** falls back to `cat` (output passes through unpaged but is never lost).
- **Thread scope:** the redirect uses the thread-local output stream, so other threads' output is unaffected.
- The handle must always be passed to `sc_pager_end` (also for no-op sessions); `sc_pager_end(NULL)` is safe.

---

## Live Display

Re-renders a composed frame in place, so multiple widgets (tables, panels, progress bars, columns, ...) form a continuously updating dashboard - like Rich's `Live`. Frames are built with the capture API (`sc_capture_*`, `sc_vstack`, `ScColumns`); the live session only handles the in-place redraw. Header: `output/sparcli_live.h`.

```c
ScLive *sc_live_begin(ScLiveOpts opts);
void    sc_live_update(ScLive *live, const ScRendered *frame);
void    sc_live_update_str(ScLive *live, const char *str);
void    sc_live_update_text(ScLive *live, const ScText *text);
void    sc_live_update_table(ScLive *live, const ScTableData *table, ScTableOpts opts);
void    sc_live_end(ScLive *live);   /* restore terminal + free the handle */
```

### ScLiveOpts

| Field | Description |
|-------|-------------|
| `alt_screen` | Fullscreen on the alternate screen buffer (htop-style); the previous terminal content is restored on end. The final frame is then re-printed on the normal screen unless `transient` |
| `show_cursor` | Keep the cursor visible; **zero-init hides it** during the session (the natural default for live displays) and restores it on end |
| `transient` | Erase the live region on end instead of leaving the final frame; off-terminal it suppresses the final-frame output |
| `always` | Emit the redraw escape codes even when the output stream is not a terminal |
| `prompt_rows` | Rows reserved **below** the frame for an interactive prompt (REPL dashboards): after every update the cursor parks at column 0 of the first reserved row, where e.g. `sc_text_input` (with `hide_summary = true`) runs and erases itself; the next update rewinds over frame + reserve together. Reserve as many rows as the prompt draws. Zero-init = 0 = classic behavior. See `examples/c/apps/repl_dashboard.c` |
| `valign` | Vertical alignment of the whole block on the **alt screen** (`SC_VALIGN_TOP` default / `MIDDLE` / `BOTTOM`): pads with leading blank rows so the frame (+ reserve) sits top/middle/bottom. Ignored off the alt screen |
| `valign_fixed_header` | With `valign`, pin the frame at the top and align only the reserved region beneath it (a gap between header and prompt) instead of the whole block |

```c
ScLive *live = sc_live_begin((ScLiveOpts){ 0 });
for (int i = 0; i <= 100; i++) {
    /* compose the dashboard: any widgets, captured + stacked */
    ScRendered *table_part = sc_capture_table(status_table, table_opts);
    ScRendered *bar_part   = sc_capture_str(progress_line);
    const ScRendered *parts[2] = { table_part, bar_part };
    ScRendered *frame = sc_vstack(parts, 2, 1);

    sc_live_update(live, frame);   /* overwrites the previous frame in place */

    sc_rendered_free(frame);
    sc_rendered_free(table_part);
    sc_rendered_free(bar_part);
    usleep(100000);
}
sc_live_end(live);   /* leaves the final frame on screen */
```

**Behavior:**

- **Non-TTY output** (pipe, file, capture, CI): updates are buffered and only the **final frame** is printed by `sc_live_end` - the same code produces clean output in scripts. A live session inside a capture or pager degrades the same way.
- **In-place redraw:** each update rewinds to the top of the previous frame, overwrites line by line (erasing stale content), and erases leftover lines from a previously taller frame. Frames taller than the terminal are clamped to the terminal height.
- **Frames are caller-built:** rebuild the frame against `sc_terminal_width()` each iteration; the session never re-renders on its own (no background thread - the output stream is thread-local).
- **Cleanup safety:** cursor visibility and the alternate screen are restored via `atexit` on clean exits and on SIGINT/SIGTERM/SIGHUP/SIGQUIT (the signal is then re-raised). Crash signals (SIGSEGV/SIGABRT) are deliberately not trapped - after a crash in `alt_screen` mode, run `reset`.
- **One session per terminal** at a time; `sc_live_end` frees the handle (no separate `_free`). `sc_live_end(NULL)` is safe.
- **Composing with a prompt** (`prompt_rows`): the live display and an input widget can share the terminal - the dashboard redraws in place above, the prompt runs in the reserved rows below. The prompt must hide its summary line; its self-erase returns the cursor exactly to where the live display parked it, keeping both regions' arithmetic stable across the loop.

### Alt-screen session

```c
void sc_altscreen_begin(void);
void sc_altscreen_end(void);
```

A thin standalone alternate-screen scope (enter the alt screen + home and hide the cursor / leave and restore), separate from `sc_live`. It does **not** draw — it just **owns the screen** for an app loop that runs **full-screen widgets** (`ScFuzzyOpts.fullscreen` / `ScFormOpts.fullscreen`): open it once, run the widgets, then end it. Switching widgets never flickers because the screen stays entered, and each widget composes its own pinned header + `valign` + body each frame. A **no-op off a terminal** (CI/pipes). Cursor and screen are restored on `atexit` / SIGINT/TERM/HUP/QUIT, like `sc_live`. One session at a time. See `examples/c/apps/fullscreen_app.c`.

```c
sc_altscreen_begin();
while (running) {
    ScRendered *header = build_header(state);      /* a captured panel */
    /* run a fuzzy finder or a form with .fullscreen = true, .header = header,
       .valign = SC_VALIGN_MIDDLE - each fills the screen, grows then scrolls */
    sc_rendered_free(header);
}
sc_altscreen_end();
```

Run the demos with `make run-example EX=live_demo` and `make run-example EX=repl_dashboard`.

---

## Input Widgets

Interactive prompts: confirm, text, password, number, textarea, single/multi select, fuzzy finder, and a date picker. Unlike the output side (which writes to the redirectable `sc_output_stream()`), input widgets are **tty-oriented**: they open `/dev/tty` (falling back to stdin/stdout), enter raw mode, read decoded keys, and redraw in place. Header: `input/sparcli_input.h` (included by the `sparcli.h` umbrella).

Every widget returns an `ScInputStatus`. Esc and Ctrl-C always cancel; a non-TTY context (output piped, CI) returns `SC_INPUT_ERROR` so callers can fall back to a default. On `SC_INPUT_OK` the interactive region is erased and a one-line summary is printed in its place (suppress with `hide_summary`).

Setting the environment variable `SPARCLI_NO_TTY` to a non-empty value other than `0` forces the non-TTY behavior even when a terminal is attached: `sc_input_available()` returns `false`, every prompt returns `SC_INPUT_ERROR` without touching `/dev/tty`, the pager becomes a no-op, and the live display buffers its frames. Test suites use this so widgets never grab the developer's real terminal (test runners like `cargo test` or `pytest` only redirect stdin/stdout – the controlling terminal stays reachable).

**Paste safety:** prompts enable bracketed-paste mode, so pasted text is treated as literal, sanitized content – pasted escape sequences and control bytes are dropped, a pasted `Enter` never submits a single-line field (the textarea keeps pasted newlines), and pasted characters can never answer a confirmation, navigate a list or trigger a custom shortcut. Terminals without bracketed-paste support fall back to interpreting pastes as keystrokes.

```c
typedef enum ScInputStatus {
    SC_INPUT_OK = 0,    /* user confirmed a value (Enter / selection) */
    SC_INPUT_CANCELLED, /* user aborted (Esc or Ctrl-C)               */
    SC_INPUT_ERROR,     /* not a TTY, read error, or allocation failure */
} ScInputStatus;
```

```c
/* Value widgets – return ScInputStatus; out-params filled on SC_INPUT_OK. */
ScInputStatus sc_confirm       (const char *question, bool *out, ScConfirmOpts opts);
ScInputStatus sc_text_input    (const char *prompt, char **out, ScTextInputOpts opts);  /* *out heap; free */
ScInputStatus sc_password_input(const char *prompt, char **out, ScPasswordOpts opts);   /* *out heap; free */
ScInputStatus sc_number_input  (const char *prompt, double *out, ScNumberOpts opts);
ScInputStatus sc_textarea      (const char *prompt, char **out, ScTextareaOpts opts);   /* *out heap; free */
ScInputStatus sc_datepicker    (struct tm *io, ScDatePickerOpts opts);                  /* io in/out */

/* Opaque-handle widgets – variable item count / per-run config. */
ScSelect *sc_select_new(ScSelectOpts opts);            /* opts.multi = true → checkboxes */
void      sc_select_add(ScSelect *s, const char *label);
void      sc_select_set_cursor (ScSelect *s, size_t index);       /* preselect */
void      sc_select_set_checked(ScSelect *s, size_t index, bool on);
size_t      sc_select_cursor (const ScSelect *s);                 /* live cursor (for callbacks) */
const char *sc_select_label  (const ScSelect *s, size_t index);   /* current label, or NULL */
void        sc_select_set_label(ScSelect *s, size_t index, const char *label); /* edit in place */
void        sc_select_remove (ScSelect *s, size_t index);         /* delete item (for callbacks) */
ScInputStatus sc_select_run(ScSelect *s, size_t *indices, size_t *count_io); /* count_io in:cap out:written */
void      sc_select_free(ScSelect *s);

ScFuzzy  *sc_fuzzy_new(ScFuzzyOpts opts);              /* opts.table + headers/n_cols → table view */
void      sc_fuzzy_add    (ScFuzzy *f, const char *label);
void      sc_fuzzy_add_row(ScFuzzy *f, const char *const *fields, size_t n); /* query searches all cols (opts.search_columns) */
void      sc_fuzzy_add_section(ScFuzzy *f, const char *title);              /* non-selectable header (e.g. a day) */
void      sc_fuzzy_add_styled (ScFuzzy *f, const char *label, ScTextStyle s); /* per-item base style */
void      sc_fuzzy_add_row_styled(ScFuzzy *f, const char *const *fields, const ScTextStyle *styles, size_t n); /* per-cell base style */
void      sc_fuzzy_add_row_rich(ScFuzzy *f, ScText *const *cells, size_t n); /* full ScText cells (deep-copied); match-key flattened */
void      sc_fuzzy_set_disabled(ScFuzzy *f, size_t i, bool on);            /* grey out, non-selectable (also unchecks) */
void      sc_fuzzy_set_id  (ScFuzzy *f, size_t i, uint64_t id);            /* stable caller id */
uint64_t  sc_fuzzy_id_at   (const ScFuzzy *f, size_t i);                   /* id by add-order index */
uint64_t  sc_fuzzy_cursor_id(const ScFuzzy *f);                           /* id of the highlighted row */
size_t    sc_fuzzy_cursor_index(const ScFuzzy *f);     /* live selection (for callbacks) */
bool      sc_fuzzy_has_selection(const ScFuzzy *f);    /* a row matches the query? (cursor_index 0 is ambiguous) */
void      sc_fuzzy_remove (ScFuzzy *f, size_t index);  /* delete row (for callbacks / re-run loops) */
ScInputStatus sc_fuzzy_run(ScFuzzy *f, size_t *out_index);  /* out_index = original add order */
ScInputStatus sc_fuzzy_run_multi(ScFuzzy *f, size_t *indices, size_t *count_io); /* opts.multi: Space toggles; checked add-order indices */
void      sc_fuzzy_set_checked(ScFuzzy *f, size_t i, bool on);            /* seed / inspect the checked set */
bool      sc_fuzzy_is_checked (const ScFuzzy *f, size_t i);
void      sc_fuzzy_check_all  (ScFuzzy *f, bool on);
size_t    sc_fuzzy_checked_count(const ScFuzzy *f);
void      sc_fuzzy_set_cursor (ScFuzzy *f, size_t i);                     /* pre-position the cursor (add order) */
const char *sc_fuzzy_label    (const ScFuzzy *f, size_t i);              /* first field */
void      sc_fuzzy_set_label  (ScFuzzy *f, size_t i, const char *s);     /* live relabel */
void      sc_fuzzy_set_row    (ScFuzzy *f, size_t i, const char *const *fields, size_t n);
void      sc_fuzzy_set_row_style(ScFuzzy *f, size_t i, size_t col, ScTextStyle s);
void      sc_fuzzy_free(ScFuzzy *f);
bool      sc_fuzzy_match(const char *pattern, const char *str, int *score);  /* pure, testable */
```

**Sections, multi-select, styling, ordering** — `ScFuzzyOpts` is zero-init-friendly, so existing callers are unchanged; the new capabilities are opt-in:

- **Sections** (`sc_fuzzy_add_section`) are non-selectable headers; they appear **only when their group has a match**, the cursor skips them, and `opts.section_counts` appends `" (k)"`. `opts.empty_text` shows a line when nothing matches.
- **Multi-select** (`opts.multi`): Space toggles, `sc_fuzzy_run_multi` returns the checked add-order indices, `opts.checkbox_column` gives the table its own checkbox column, and `opts.toggle_all_key` / `opts.toggle_section_key` (`ScKeyChord`, e.g. `sc_key_ctrl('a')`) flip all rows / the cursor's section.
- **Per-cell color**: `sc_fuzzy_add_row_styled` sets a whole-cell base `ScTextStyle` (the match highlight overlays it; the cursor row keeps the cell color); `sc_fuzzy_add_row_rich` takes full `ScText` cells (multi-color spans, no per-character highlight). `opts.disabled_style` greys disabled rows.
- **Ordering** (`opts.order` = `SC_FUZZY_ORDER_SCORE` / `_INSERTION` / `_COLUMN` with `order_column` / `order_desc`) sorts **within each section** when sections are present, else globally. Stable ids (`sc_fuzzy_set_id` / `_id_at` / `_cursor_id`) and the live mutators (`set_cursor` / `set_label` / `set_row` / `set_row_style` / `remove`, plus `opts.initial_query`) drive an apply-action-then-re-run loop (see `examples/c/apps/todo_fuzzy.c`).
- **Modal normal/insert mode** (`opts.modal`, off by default — vim-style): in **normal** mode bare-letter shortcuts fire (no modifier), `j`/`k`/`g`/`G` navigate, `opts.clear_key` (zero-init = disabled) empties the query, and `Esc` cancels; in **insert** mode keys edit the query and bare-letter shortcuts are suppressed (they type). Toggle with `opts.insert_key` (zero-init = `i`) / `opts.normal_key` (zero-init = `Esc`); `opts.start_in_insert` picks the initial mode (default normal). The query line shows the mode: a colored field tint + a `NORMAL`/`INSERT` badge (`opts.mode_normal_style` / `mode_insert_style`, labels `normal_label` / `insert_label`, `hide_mode_badge` drops the badge). `Ctrl-N` / `Ctrl-P` move the cursor in both modes (and the non-modal finder). A bare-character chord is built directly, e.g. `(ScKeyChord){ SC_KEY_CHAR, 'c', 0 }`. Two opt-in `ScPromptVTable` hooks make this generic: `capture_escape` forwards `Esc` to the widget instead of cancelling (`Ctrl-C`/EOF still cancel), and `suppress_char_shortcuts` skips bare-character shortcut chords while in insert mode.

**Opts lifetime (handle widgets):** `sc_select_new` / `sc_fuzzy_new` **copy** their opts string fields (prompt, markers, checkbox glyphs, hint, fuzzy headers, `empty_text`, `initial_query`, `normal_label` / `insert_label`) and item labels are copied by `sc_*_add` (rich `ScText` cells are deep-copied), so the caller's buffers only need to live until the respective call returns. Only `shortcuts` and `prompt_text` stay **borrowed** and must outlive the run.

### Form (grid layout)

A **form** lays out several fields as framed boxes in a row/column raster, navigated in 2D and edited one field at a time in a region below the grid. It is one interactive session (a single `sc_form_run`) — do **not** nest it inside another prompt.

```c
ScForm *sc_form_new(ScFormOpts opts);   /* opts strings copied */
void sc_form_row_begin(ScForm *f);      /* start a grid row */
int  sc_form_add_text  (ScForm *f, const char *label, const char *initial, ScFieldOpts o); /* → field index */
int  sc_form_add_number(ScForm *f, const char *label, double initial, ScFieldOpts o);
int  sc_form_add_bool  (ScForm *f, const char *label, bool initial, ScFieldOpts o);
int  sc_form_add_select(ScForm *f, const char *label, const char *const *choices, size_t n, size_t initial, ScFieldOpts o);
int  sc_form_add_multiselect(ScForm *f, const char *label, const char *const *choices, size_t n, const size_t *checked_idx, size_t n_checked, ScFieldOpts o);
int  sc_form_add_date  (ScForm *f, const char *label, struct tm initial, ScFieldOpts o); /* zeroed tm = today */
void sc_form_add_skip  (ScForm *f);     /* placeholder under a col/row-spanning field */
ScInputStatus sc_form_run(ScForm *f);   /* Ctrl-D submit (enforces required), Esc cancel */
const char *sc_form_get_string(const ScForm *f, int field);
double      sc_form_get_number(const ScForm *f, int field);
bool        sc_form_get_bool  (const ScForm *f, int field);
size_t      sc_form_get_choice(const ScForm *f, int field);                      /* single-select index */
size_t      sc_form_get_checked(const ScForm *f, int field, size_t *out, size_t cap); /* multiselect → count */
bool        sc_form_get_date  (const ScForm *f, int field, struct tm *out);
bool        sc_form_modified(const ScForm *f);   /* any field changed vs its initial value */
void        sc_form_free(ScForm *f);
```

**Layout** mirrors the table model — rows of cells with `ScFieldOpts.col_span` / `row_span` and `sc_form_add_skip` placeholders for spanned cells. Per-field width is `ScFieldWidthMode` (`SC_FWIDTH_AUTO` / `_PCT` / `_FIXED` with `width`); it is a **shared column grid**, so a column has one width, taken from the **last** single-column field anchored in it (a spanning field just sums its columns). `SC_FWIDTH_PCT` widths are resolved with **cumulative rounding**: consecutive PCT columns collectively hit `round(term_w · Σpct / 100)`, so e.g. five 20% columns fill the full width exactly with no gap (independent per-column flooring would drop up to one column each). Every column is at least `FORM_MIN_COL` (6) wide. `height` sets a field's content lines; a `row_span` field fills the spanned rows.

**Interaction:** arrows move the active box in 2D (it gets an `accent`-colored border); Tab/Shift-Tab cycle. **Enter** opens the field's editor in an `accent`-framed box **below** the grid (the box keeps showing the committed value); a second **Enter** validates + saves and the grid re-renders; **Esc** discards the edit (Esc in navigation cancels the form). Bool fields toggle in place with Space/Enter. select/multiselect open a scrollable list (Space toggles a checkbox); date opens a month grid. **Ctrl-D** submits and first enforces `ScFieldOpts.required` (empty text/number or zero-checked multiselect blocks submit and jumps to the field). **`ScFormOpts.autoedit`** opens the first field's editor immediately at start (skip navigation; no-op on a bool field) — e.g. type a new record's title right away. **Dirty tracking:** `sc_form_modified()` reports whether any field differs from the value it was added with (for an "unsaved changes?" prompt on Esc), and `ScFormOpts.modified_marker` (e.g. `"[*] "`) prefixes a changed field's box title, appearing/disappearing as the value diverges from / returns to its initial.

**Multiline text** (`ScFieldOpts.multiline`): the value may hold newlines (shown across the box) and is edited via the **external editor** — **Enter** or `ScFormOpts.editor_key` (zero-init = Ctrl-G) opens `ScFormOpts.editor` (NULL = `$VISUAL`/`$EDITOR`/nvim/vi). **`ScFormOpts.editor_suffix`** (borrowed, NULL/empty = none) gives the editor's temp file an extension (e.g. `".md"`) so the editor detects the filetype; the file is `sparcli-edit-XXXXXX<suffix>` (mode 0600, unlinked after read-back). Optional per-field `ScFieldOpts.validate` keeps the inline editor open with a red error line. **Full-screen mode** (`ScFormOpts.fullscreen` + a borrowed `header` `ScRendered *` + `valign`): composes `[valign-pad][header][grid]` filling the terminal for a consistent shell alongside a full-screen finder; run inside an `sc_altscreen_begin` session. By default the grid is fixed-size (leftover space becomes `valign` padding), but a field with **`ScFieldOpts.fill_height`** grows its row to consume the remaining terminal height (e.g. a multiline body that fills the screen) — `height` acts as the minimum, and if several fields set it the first wins; ignored outside full-screen and when the terminal is too short to leave free rows. **`ScFormOpts.valign_scope`** (`ScVAlignScope`, fullscreen only) selects what `valign` moves: `SC_VALIGN_SCOPE_ALL` (zero-init) aligns the whole header+grid+footer block as one unit, while `SC_VALIGN_SCOPE_CONTENT` pins the header to the top row and the edit/hint footer to the bottom row, aligning only the grid in the gap between them (`[header][pad][grid][pad][footer]`). Strings are copied; `shortcuts` and `header` stay borrowed. Bindings: C++ `sparcli::Form`, Rust `sparcli::Form`, Python `sc.Form`. Examples: `examples/c/apps/form_demo.c`, `examples/c/apps/fullscreen_app.c`.

```c
/* Custom shortcuts (any widget) – bind extra keys to actions. See "Custom shortcuts". */
ScKeyChord sc_key_ctrl(char letter);   /* ^letter (Ctrl-C/H/I/J/M not bindable; case-insensitive) */
ScKeyChord sc_key_fn  (int n);         /* F1..F12 */
ScKeyChord sc_key_alt (char letter);   /* Alt/Meta + letter */
ScKeyChord sc_key_special(ScKeyType k);            /* named key: arrows, Del, Home, … */
ScKeyChord sc_key_mod (ScKeyType k, uint8_t mods); /* named key + Shift/Alt/Ctrl (combos) */
void       sc_key_chord_name(ScKeyChord chord, char *buf, size_t cap); /* "F2","^E","M-e","←","Del","M-↑" */
bool       sc_key_chord_matches(ScKey key, ScKeyChord chord);
const ScShortcut *sc_shortcut_find(ScKey key, const ScShortcut *items, size_t n);

/* Optional input constraints for text/password (validate keeps the prompt open). */
typedef bool (*ScValidateFn)(const char *value, void *ctx, const char **err_out);
typedef bool (*ScCharFilter)(uint32_t codepoint, void *ctx);
bool sc_filter_digits  (uint32_t cp, void *ctx);   /* 0-9 */
bool sc_filter_decimal (uint32_t cp, void *ctx);   /* 0-9 . , - + */
bool sc_filter_alpha   (uint32_t cp, void *ctx);   /* letters */
bool sc_filter_alnum   (uint32_t cp, void *ctx);   /* letters + digits */
bool sc_filter_no_space(uint32_t cp, void *ctx);   /* rejects whitespace */

bool sc_input_available(void);   /* true when a prompt can run (a TTY is present
                                    and SPARCLI_NO_TTY is not set) */
```

Most styling options are zero-init-friendly: an unset `ScTextStyle`/`ScColor` selects the widget's built-in default. Every widget has a `prompt_style`, `summary_style`/`hide_summary`, and a key-hint footer (`hint` / `hint_layout` / `hint_pos` / `hint_style`).

`hint_layout` is an `ScHintLayout` that controls the footer on every widget: `SC_HINT_INLINE` (one `·`-separated line – the default), `SC_HINT_STACKED` (one hint per line), or `SC_HINT_HIDDEN` (no footer). The zero-init `SC_HINT_LAYOUT_DEFAULT` inherits the theme, then falls back to inline.

`hint_pos` is an `ScHintPosition` that places the footer relative to the widget – `SC_HINT_POS_TOP`, `SC_HINT_POS_BOTTOM` (default), `SC_HINT_POS_LEFT`, or `SC_HINT_POS_RIGHT` (left/right sit beside the widget, top-aligned). It is orthogonal to `hint_layout` (e.g. right + stacked, or right + inline). The zero-init `SC_HINT_POS_DEFAULT` inherits the theme, then falls back to bottom.

### Custom shortcuts

Every widget's opts carry `shortcuts` / `n_shortcuts` (a borrowed `ScShortcut[]`) and an optional `int *out_shortcut_id`. The prompt engine matches them before the widget's own keys (after the reserved Esc / Ctrl-C cancel), so one mechanism works on all widgets. Header: `input/sparcli_shortcut.h`.

```c
typedef enum { SC_SHORTCUT_RETURN = 0, SC_SHORTCUT_CALLBACK } ScShortcutMode;

typedef struct ScShortcut {
    ScKeyChord     chord;   /* sc_key_ctrl('e') / sc_key_fn(2) / sc_key_alt('e') */
    int            id;      /* reported via *out_shortcut_id (RETURN mode) */
    ScShortcutMode mode;
    bool         (*on_fire)(int id, void *user); /* CALLBACK: true = keep open */
    void          *user;
    const char    *hint_label; /* shown in a dim footer (e.g. "^X delete") */
} ScShortcut;
```

- **RETURN** ends the prompt; `*out_shortcut_id` receives the fired `id` (`-1` on a normal submit/cancel) and the widget still returns its value. Use it for "edit/help/new" actions that close the prompt.
- **CALLBACK** runs `on_fire(id, user)` in place and keeps the prompt open unless it returns `false`. It must not open another prompt (single session). For live list edits use `sc_select_cursor`/`sc_fuzzy_cursor_index` to read the selection and `sc_select_remove`/`sc_fuzzy_remove`/`sc_select_set_label` to mutate it.

Key decoding gained `SC_KEY_F1`…`SC_KEY_F12`, a `mods` bitmask on `ScKey` (`SC_MOD_CTRL` / `SC_MOD_ALT`), Alt via an `ESC` prefix, and generic Ctrl-letters as `SC_KEY_CHAR + SC_MOD_CTRL`. Esc / Ctrl-C stay reserved (not bindable).

### Rich prompts

For partial styling (e.g. `Rename `*`Apple`*` to`) every widget's opts also take `prompt_text` (a borrowed `ScText *`, overrides the string prompt) and `prompt_markup` (parse the string prompt as inline markup). Precedence: `prompt_text` > `prompt_markup` > plain `prompt` + `prompt_style`. Works inline and boxed (boxed routes through `ScTitle.rich_text`, so the box width is computed from the visible width, not escape bytes). `prompt_text` needs no escaping even when the value contains `[`.

```c
ScText *p = sc_text_new();
sc_text_append(p, "Rename ", (ScTextStyle){ 0 });
sc_text_append(p, name, (ScTextStyle){ SC_TEXT_ATTR_ITALIC, 0, 0 });
sc_text_append(p, " to", (ScTextStyle){ 0 });
sc_text_input(NULL, &out, (ScTextInputOpts){ .initial = name, .prompt_text = p });
sc_text_free(p);
```

### External editor

`sc_text_input` and `sc_textarea` can hand off to the user's `$EDITOR`. Opt in per call: `external_editor = true`, optional `editor` command override, and `editor_key` (an `ScKeyChord`; zero-init = **Ctrl-G**). Pressing the key suspends raw mode, opens the editor on a temp file seeded with the current value, and on a clean save+quit replaces the value with the file contents (text_input collapses newlines to spaces, since it is single-line). A non-zero editor exit (e.g. `:cq`) keeps the old value. The temp file is `sparcli-edit-XXXXXX` (mode 0600, unlinked after read-back); it can carry an extension for filetype detection — exposed to callers via `ScFormOpts.editor_suffix` for forms. The editor key is matched **before** custom shortcuts, so binding the same chord to both makes the editor win.

```c
sc_textarea("Commit message", &msg, (ScTextareaOpts){
    .external_editor = true,            /* Ctrl-G → editor */
    .editor          = "nvim",          /* NULL = $VISUAL → $EDITOR → nvim → vi */
});
```

Safety: the command runs via `execvp` with a **whitespace-tokenized argv (no shell)**; the temp file is `mkstemp` mode `0600` and unlinked before return; the terminal is restored on resume (and by the existing `atexit`/signal handlers if the process dies). **Password input does not support this** (a plaintext temp file + editor swap/undo files would leak the secret). Editor commands with quoted/space-containing arguments are not supported (tokenized on whitespace).

### sc_confirm

Yes/No question. Arrow keys / Tab / `h` / `l` move; `y`/`n` pick directly; Enter confirms.

| Field | Description |
|-------|-------------|
| `default_yes` | Initial selection (`false` = No) |
| `yes_label` / `no_label` | Button labels; `NULL` = "Yes" / "No" |
| `accent` | Highlight color of the selected option; zero-init = green |
| `prompt_style` | Style for the question text |
| `selected_style` / `unselected_style` | Option styles; zero-init = bold black-on-`accent` / dim |
| `box` (`ScBoxStyle`) | Frame the question in a panel; zero-init = inline |
| `summary_style` / `hide_summary` | Post-confirm summary line |
| `hint` / `hint_layout` / `hint_style` | Key-hint footer |

### sc_text_input / sc_password_input

Single-line entry over a shared line editor (UTF-8 cursor/insert/delete/word- kill). Password masks each character (`mask`, default `"*"`; `""` hides length).

| Field | Description |
|-------|-------------|
| `initial` | Pre-filled value (text only); `NULL` = empty |
| `placeholder` | Dim hint shown while empty |
| `mask` | Password only: glyph per char; `NULL` = `"*"` |
| `prompt_style` / `value_style` / `cursor_style` | Styles; cursor zero-init = black-on-white |
| `error_style` | Validation error line; zero-init = red |
| `max_chars` | Cap on input length; `0` = unlimited |
| `hide_char_count` / `count_style` | Character counter (`count` or `count/max`); default shown, dim |
| `box` (`ScBoxStyle`) | Frame the field in a panel: `box.enabled` on; `box.border`/`box.bg`/`box.padding`/`box.margin`/`box.width` (`0` = full terminal width). Prompt = top title, counter = bottom-right border |
| `char_filter` / `char_filter_ctx` | Per-keystroke filter (built-ins `sc_filter_*`) |
| `suggestions` / `n_suggestions` | Text only: autocomplete word list. Default presentation is a dim ghost (first prefix match; Tab accepts); see `suggest` for the dropdown |
| `suggest` | `ScSuggestOpts` – presentation of `suggestions`: ghost text (zero-init) or a navigable dropdown list (see below) |
| `validate` / `validate_ctx` | Validator; keeps the prompt open and shows an error line |
| `history` / `no_history_add` | Text only: attach an `ScHistory` for ↑/↓ recall of previous entries; submitted lines are recorded automatically unless `no_history_add`. Borrowed for the call; see [Input history](#input-history-sc_history) |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

`*out` is heap-allocated on `SC_INPUT_OK` – the caller must `free()` it.

#### Autocomplete dropdown (`ScSuggestOpts`)

Set `suggest.mode = SC_SUGGEST_DROPDOWN` to present the suggestions as a list below the field instead of inline ghost text. While at least one suggestion matches the typed value, the list is shown and the keys change: **↑/↓** move the highlight (↑ above the first row returns focus to the field), **Tab** accepts the highlighted row (or the best match when none is highlighted), **Enter** accepts the highlighted row – or submits the typed value when no row is highlighted. Works inline and in `boxed` mode (the dropdown stacks beneath the panel and, when framed, matches the box width).

| Field | Description |
|-------|-------------|
| `mode` | `SC_SUGGEST_GHOST` (zero-init) or `SC_SUGGEST_DROPDOWN` |
| `match` | `SC_SUGGEST_MATCH_PREFIX` (zero-init, case-insensitive) or `SC_SUGGEST_MATCH_FUZZY` (subsequence match via `sc_fuzzy_match`, best score first) |
| `max_visible` | Max rows shown at once; `0` = 5. More matches add a dim `… +N more` line |
| `border` | `ScBorderStyle` frame around the list; zero-init type = plain list without a border |
| `item_style` | Style of unselected rows; zero-init = no formatting |
| `selected_style` | Style (incl. background) of the highlighted row; zero-init = bold cyan |
| `more_style` | Style of the `… +N more` overflow line; zero-init = dim |
| `cursor_marker` / `marker` | Markers before the highlighted / unselected rows; `NULL` = `"‣ "` / two spaces |

An exact (case-insensitive) match is never listed – accepting a suggestion therefore closes the dropdown. An empty field shows no suggestions.

```c
static const char *const cmds[] = { "commit", "checkout", "cherry-pick" };
char *cmd = NULL;
sc_text_input("Git command", &cmd, (ScTextInputOpts){
    .suggestions = cmds, .n_suggestions = 3,
    .suggest = {
        .mode  = SC_SUGGEST_DROPDOWN,
        .match = SC_SUGGEST_MATCH_FUZZY,
        .border = { .type = SC_BORDER_ROUNDED },
        .selected_style = { SC_TEXT_ATTR_NONE, SC_ANSI_COLOR_BLACK,
                            SC_ANSI_COLOR_CYAN },   /* black on cyan */
    },
});
```

### sc_number_input

Numeric entry with a decimal filter; ↑/↓ adjust by `step`; value clamped to `[min, max]` when `max > min`; formatted to `decimals` places.

| Field | Description |
|-------|-------------|
| `initial` | Starting value |
| `start_empty` | Start with an empty field instead of the formatted `initial` value. Enter on an empty field is ignored, so the prompt never submits "nothing" as `0` |
| `min` / `max` | Bounds, applied when `max > min` |
| `step` | Up/Down increment; `0` = 1 |
| `decimals` | Fractional digits; `0` = integer |
| `decimal_sep` | Decimal separator for display and input; `0`/`'.'` = period, `','` = comma. Both `.` and `,` keystrokes are accepted and shown as the configured separator |
| `out_text` | Optional `char **`: on `SC_INPUT_OK` receives the submitted value as a heap string – exact, never round-tripped through `double`. Always `'.'`-separated and reflecting clamping, so it can feed an arbitrary-precision decimal type. Caller frees |
| `prompt_style` / `value_style` / `cursor_style` | Styles |
| `box` (`ScBoxStyle`) | Frame in a panel (range shown on the bottom-right border); see `ScBoxStyle` |
| `calculator` | Enable calculator mode: `=` starts an arithmetic expression (see below) |
| `calc_store_rounded` / `calc_show_precise` | Calculator precision flags (see below) |
| `error_style` | Style of the invalid-expression error line; zero-init = red |
| `calc_warn_text` / `calc_warn_style` | Text and style of the discarded-result warning (see below); `NULL` text = built-in English default, zero-init style = yellow |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

```c
/* Exact money amount, German-style comma input: */
double value = 0;
char *text = NULL;
sc_number_input("Amount", &value, (ScNumberOpts){
    .decimals = 2, .step = 0.5, .decimal_sep = ',', .out_text = &text });
/* user types "12,99" → value == 12.99, text == "12.99" (caller frees) */
```

Note: `*out` and `*out_text` always agree – when a typed value is clamped to `[min, max]`, the buffer is rewritten before both are produced. With `decimals = 0` a typed fraction is rounded the same way in both. sparcli assumes the C numeric locale (it never calls `setlocale`).

#### Calculator mode

With `calculator = true`, typing `=` as the **first character** switches the field to expression mode: the user types an arithmetic expression (e.g. `=1,5+2*3`), a dim live preview shows the result (`= 7,50`) or `= ?` while the expression is incomplete/invalid. The keys then are:

- **Enter** (on a valid expression): replaces the field content with the result – the "accept" step. The field shows the value rounded to `decimals`; the full-precision result is kept internally. On an invalid expression a red error line appears instead and the prompt stays open.
- **Enter** again: submits. By default `*out` / `*out_text` carry the **full-precision** result (a deliberate exception to "displayed text == submitted value"; `*out` and `*out_text` still agree, formatted as round-trip-exact `%.17g`).
- Editing the accepted value (typing, Backspace, ↑/↓) discards the pending exact result – the edited text becomes the value, as usual.
- Backspacing the leading `=` away exits calculator mode.

**Pending indicator and discard warning:** while the accepted full-precision result differs from the rounded display (e.g. `=1/3` → field shows `0,33`, exact value `0.333…` pending), a dim ` = 0,3333333333` indicator is shown next to the field – as long as it is visible, the exact result is what gets submitted. Editing the field then discards that pending result; when this actually loses precision, a warning line (yellow by default) appears below the field: *"exact result discarded - the displayed value will be submitted"*. The warning stays until submit or until a new expression is accepted; it never appears when nothing is lost (e.g. `=1/2` → `0,50` is exact) or with `calc_store_rounded` (the display is the value there anyway). Override the text with `calc_warn_text` (e.g. for localized UIs) and the style with `calc_warn_style`.

Expression syntax (see `sc_calc_eval`): `+ - * /`, parentheses, a single unary minus per operand (`2*-3` works, `1++2` and `--3` are typos and rejected), numbers with `.` or `,` as decimal separator, whitespace ignored. Division by zero and overflow are invalid.

Precision flags:

| Flags | Field shows | `*out` / `out_text` |
|-------|-------------|---------------------|
| (default) | rounded to `decimals` | full precision, clamped |
| `calc_store_rounded` | rounded to `decimals` | the rounded (displayed) value |
| `calc_show_precise` | full precision | full precision |

```c
/* Finance entry with calculator: "=12,99*3" → field shows 38,97,
   submitted text is the exact result. */
double value = 0;
char *exact = NULL;
sc_number_input("Amount", &value, (ScNumberOpts){
    .decimals = 2, .decimal_sep = ',', .start_empty = true,
    .calculator = true, .out_text = &exact });
```

The evaluator itself is public and TTY-free:

```c
double result;
if (sc_calc_eval("1,5+2*(3-1)", &result)) { /* result == 5.5 */ }
```

### sc_textarea

Multi-line entry: Enter inserts a newline, Ctrl-D submits, arrows move across lines/cols, Home/End within a line; long logical lines soft-wrap to the field width.

| Field | Description |
|-------|-------------|
| `initial` | Pre-filled multi-line value |
| `placeholder` | Dim hint shown while empty |
| `prompt_style` / `value_style` / `cursor_style` | Styles |
| `box` (`ScBoxStyle`) | Frame the editor in a panel; see `ScBoxStyle` |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

`*out` (with embedded newlines) is heap-allocated on `SC_INPUT_OK`; free it.

### sc_select (single / multi)

Opaque handle (variable item count). `j/k` + arrows move; Space toggles in multi-select; a viewport (`max_visible`, default 10) scrolls with a dim `↑ first–last/total ↓` indicator. Single-select writes one index; multi-select writes the checked indices in display order (`*count_io` in: capacity, out: written). `sc_select_cursor` / `sc_select_label` / `sc_select_set_label` / `sc_select_remove` let a shortcut callback read and edit items live (see [Custom shortcuts](#custom-shortcuts)).

| Field | Description |
|-------|-------------|
| `prompt` | Heading above the list |
| `multi` | `true` = checkbox multi-select |
| `no_cycle` | Stop at the list ends; the cursor cycles around by default (Up on first → last) |
| `max_visible` | Rows shown at once; `0` = 10 |
| `accent` | Cursor-row highlight; zero-init = cyan |
| `prompt_style` / `selected_style` | Heading + cursor-row styles |
| `cursor_marker` / `marker` | Cursor / other-row prefixes; `NULL` = "‣ " / "  " |
| `checkbox_on` / `checkbox_off` | Multi only; `NULL` = "[x] " / "[ ] " |
| `box` (`ScBoxStyle`) | Background / frame / width: `bg` fills behind rows (rows inherit it; a `bg` works borderless), `width_mode` (default `content`, + `min_width`/`max_width`) / `fixed`/`full`, `bg_extent` controls the fill reach. The cursor row becomes a full-width bar when `selected_style.bg` is set |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

### sc_fuzzy

Opaque handle. Ranks items by `sc_fuzzy_match` on each keystroke; matched characters are highlighted (in the list, and in the matched table cells). List view by default; set `table = true` with `headers`/`n_cols` and add rows via `sc_fuzzy_add_row` for a table view. `out_index` is the chosen item's original add order. `sc_fuzzy_cursor_index` / `sc_fuzzy_remove` expose and mutate the selection from a shortcut callback. Beyond plain picking it also does **section headers** (`sc_fuzzy_add_section`, shown only when their group matches), **multi-select** (`opts.multi` + `sc_fuzzy_run_multi`, with `set_checked`/`check_all` and a checkbox column / toggle keys), **per-cell colors** (`sc_fuzzy_add_row_styled` / `_add_row_rich`) and disabled rows, configurable **ordering** within sections (`opts.order`), **stable ids** (`sc_fuzzy_set_id` / `_id_at`), and **live mutation** (`set_label`/`set_row`/`set_row_style`) for an apply-action-then-re-run loop — see the "Sections, multi-select, styling, ordering" notes above and `examples/c/apps/todo_fuzzy.c`.

| Field | Description |
|-------|-------------|
| `prompt` | Search-field label; `NULL` = "Search" |
| `max_visible` | Result rows shown; `0` = 10 |
| `no_cycle` | Stop at the result-list ends (cycles around by default) |
| `accent` | Cursor-row highlight; zero-init = cyan |
| `table` / `headers` / `n_cols` | Table view configuration |
| `search_columns` | Bitmask of columns the query searches (bit `c` = column `c`); `0` = all (default). Table view only; a row matches when any selected column matches |
| `table_opts` | Passed through to the table renderer (border, header, …); the cursor-row background defaults to `accent`, overridable via `selected_style.bg` |
| `box` (`ScBoxStyle`) | Background / frame / width for the finder (query + results): `bg` fills behind rows (borderless ok), `width_mode` (default `content` + `min_width`/`max_width`) / `fixed`/`full`, `bg_extent`; the list cursor row becomes a full-width bar when `selected_style.bg` is set |
| `max_height` | Cap the finder height in rows so the result list scrolls within it; `0` auto-fits the terminal. Set it to a live dashboard's `prompt_rows` reserve |
| `no_scrollbar` | Suppress the right-edge scrollbar (drawn by default while the list scrolls); the `↑ x–y/total ↓` text indicator still shows |
| `stretch_columns` | Bitmask of table columns that absorb the slack when `box` resolves wider than the natural table (`full`/`fixed`/`content`+`max_width`); `0` = content-sized. Table view only. The table is re-rendered to the box interior whenever its natural width differs from it — so when the interior is **narrower** (e.g. an over-long cell), the stretch columns shrink and their cells truncate with `…` instead of overflowing the terminal |
| `fullscreen` | Full-screen mode: the finder fills the terminal, its list **grows with the items** up to the available height (terminal − `header` − chrome) then scrolls; the leftover space is placed by `valign`. Run inside an `sc_altscreen_begin` session |
| `valign` | Vertical alignment of the (header + finder) block (fullscreen only): `TOP` fills below the finder, `BOTTOM` above the header, `MIDDLE` both — recomputed each frame as the list grows |
| `header` | Borrowed `ScRendered *` pinned above the finder (fullscreen only); e.g. a captured panel. Must outlive the run |
| `prompt_style` / `selected_style` / `cursor_style` / `counter_style` | Styles |
| `cursor_marker` / `marker` | List cursor / other-row prefixes |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

### sc_datepicker

Month-grid calendar. Arrows move day/week; PageUp/PageDown (or `<`/`>`) change month; Shift+PageUp/PageDown change year; month/year jumps keep the selected day (clamped, e.g. Jan 31 → Feb 28). `io` is in/out: a zeroed `struct tm` seeds today; on `SC_INPUT_OK` it holds the picked date.

| Field | Description |
|-------|-------------|
| `prompt` | Heading above the calendar |
| `week_start` | `ScWeekStart`: `SC_WEEK_START_DEFAULT` (Monday) / `_MONDAY` / `_SUNDAY` |
| `accent` | Selected-day highlight; zero-init = cyan |
| `prompt_style` / `header_style` / `weekday_style` / `selected_style` | Styles |
| `header_prev` / `header_next` | Month-arrow glyphs; `NULL` = "‹" / "›" |
| `box` (`ScBoxStyle`) | Frame the calendar in a panel; zero-init = inline |
| `summary_style` / `hide_summary`, `hint` / `hint_layout` / `hint_style` | As above |

### Input history (sc_history)

REPL-style command history for the text input: ↑/↓ recall previous entries, submitted lines are recorded automatically, and the history optionally persists across runs. The handle is pure storage (the recall navigation lives in the text input); one handle naturally spans a whole REPL session. Header: `input/sparcli_history.h`.

```c
ScHistory  *sc_history_new(ScHistoryOpts opts);   /* loads the file, if configured */
void        sc_history_add(ScHistory *h, const char *line);
size_t      sc_history_count(const ScHistory *h);
const char *sc_history_get(const ScHistory *h, size_t index);  /* 0 = oldest */
bool        sc_history_save(const ScHistory *h);
bool        sc_history_load(ScHistory *h);        /* replaces current entries */
void        sc_history_free(ScHistory *h);        /* saves + frees */
```

| `ScHistoryOpts` field | Description |
|-------|-------------|
| `max_entries` | Retained entry cap; `0` = 500. Oldest entries are evicted past the cap |
| `app` | XDG persistence: entries live in `~/.local/state/<app>/history` (created on first use) |
| `file` | Explicit persistence file path; overrides `app`. `NULL` + no `app` = in-memory only |
| `no_auto_add` | When attached to a text input, do **not** record submitted lines automatically |
| `keep_duplicates` | Keep consecutive duplicate lines instead of collapsing them |

Attach it via `ScTextInputOpts.history` (borrowed for the call):

```c
ScHistory *hist = sc_history_new((ScHistoryOpts){ .app = "myapp" });

for (;;) {
    char *line = NULL;
    ScInputStatus st = sc_text_input("repl>", &line,
        (ScTextInputOpts){ .history = hist });
    if (st != SC_INPUT_OK) { break; }
    /* dispatch(line) - the line was already added to hist automatically */
    free(line);
}
sc_history_free(hist);   /* writes ~/.local/state/myapp/history */
```

**Behavior:** ↑ recalls older entries (newest first); the in-progress line is preserved and restored by walking back down past the newest entry; typing/editing leaves the recall. While the autocomplete dropdown shows matches it keeps priority over history on ↑/↓. Auto-add skips empty lines and consecutive duplicates and is disabled per call with `ScTextInputOpts.no_history_add`. Entries are sanitized when they enter the history (`sc_history_add` and file load); lines containing line breaks are rejected.

Examples: `examples/c/apps/repl_minimal.c` (just the loop + history), `examples/c/apps/repl_demo.c` (full REPL with the argument parser).

### Theme

Process-wide defaults inherited by every widget for any zero-init option. Precedence: per-call opts > theme > built-in default.

```c
void         sc_input_set_theme(const ScInputTheme *theme);  /* NULL resets */
ScInputTheme sc_input_theme(void);                           /* current theme */
```

`ScInputTheme` carries `accent`, `border`, the shared styles (`prompt_style`, `selected_style`, `cursor_style`, `count_style`, `summary_style`, `error_style`, `hint_style`), glyphs (`cursor_marker`, `marker`, `checkbox_on`, `checkbox_off`), and `hint_layout`.

The theme struct **and its glyph strings are copied** by `sc_input_set_theme`, so the caller's buffers only need to live until the call returns. The strings returned by `sc_input_theme()` are library-owned copies, valid until the next `sc_input_set_theme` call.

---

## Internal Helpers (`src/internal.h`)

Not part of the public API, but useful background for contributors and power users:

| Helper | Description |
|--------|-------------|
| `sc_apply_colors(fg, bg)` | Emits ANSI fg/bg escapes; skips if `index == 0` (zero-init / `SC_ANSI_COLOR_NONE`) |
| `sc_terminal_width()` | Terminal width via `ioctl(TIOCGWINSZ)`, fallback 80 |
| `sc_utf8_string_length(string, byte_length)` | Visible column count of a UTF-8 byte sequence (display-width-aware: CJK/Fullwidth/emoji = 2, combining = 0) |
| `sc_utf8_trim_to_cols(string, max_columns)` | Byte count that fits within `max_columns` columns |

---

## Key Invariants

- **`SC_ANSI_COLOR_NONE` sentinel is `index = 0`**, identical to a zero-initialized `ScColor`. Any unset color field renders as "no color" automatically; no explicit assignment needed. Named colors use `index = 1..8` (BLACK..WHITE); RGB uses `index = -1`.
- `sc_print()` always appends `\033[0m` (reset), even when opts are all-none. This is intentional to isolate styling.
- The `h` horizontal-line character from `ScBorderType` is used by both panel titles, table titles, rules, and column separators – all from the same logical table in each file.
- `ScText` / `ScTableData` / `ScColumns` all heap-allocate; always call the corresponding `_free()` function.
- `ScColumns` captures widget output at `sc_columns_add_*` time. Modifying a table after adding it to a columns layout has no effect.
- Word-wrap in tables (`ScColOpts.wrap = 1`) breaks on spaces only. If no space fits in the column width, the line is truncated. **Truncated (non-wrapped) cells end with an ellipsis (`…`)**: one column is reserved for the glyph and the content trimmed to fit (`remaining - 1` columns + `…`); a column only 1 cell wide shows just `…`. The ellipsis inherits the cell's style/bg. Trimming never splits a **double-width glyph** (CJK/emoji), so the cell is padded to the full column width when a wide-glyph boundary leaves it a column short — the border stays aligned.
- **Zero-init `ScTextStyle` sentinel** (used by `ScKVOpts.key_style`, `ScKVOpts.val_style`, `ScListOpts.marker_style`, `ScBadgeOpts.text_style`): zero-init = no formatting. Renderers use `opts_has_format()` to detect this and skip `sc_print()`.
