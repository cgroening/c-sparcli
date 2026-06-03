//! Output widgets: panels, tables, rules, columns, lists, trees, key/value
//! blocks, alerts, badges, progress bars, spinners, capture/compose.

use crate::style::{
    cstring, Align, AnsiMode, Arena, BorderStyle, BorderType, Color, Edges,
    Position, Style, VAlign,
};
use crate::text::Text;
use sparcli_sys as ffi;

repr_enum!(
    /// Preset alert kinds.
    AlertType {
        Info = ffi::ScAlertType_SC_ALERT_INFO,
        Debug = ffi::ScAlertType_SC_ALERT_DEBUG,
        Warning = ffi::ScAlertType_SC_ALERT_WARNING,
        Error = ffi::ScAlertType_SC_ALERT_ERROR,
        Success = ffi::ScAlertType_SC_ALERT_SUCCESS,
    } default Info
);

repr_enum!(
    /// Progress-bar fill style.
    ProgressType {
        Block = ffi::ScProgressType_SC_PROGRESS_BLOCK,
        Ascii = ffi::ScProgressType_SC_PROGRESS_ASCII,
        Line = ffi::ScProgressType_SC_PROGRESS_LINE,
        Shaded = ffi::ScProgressType_SC_PROGRESS_SHADED,
    } default Block
);

repr_enum!(
    /// Spinner frame set.
    SpinnerType {
        Braille = ffi::ScSpinnerType_SC_SPINNER_BRAILLE,
        Pipe = ffi::ScSpinnerType_SC_SPINNER_PIPE,
        Dots = ffi::ScSpinnerType_SC_SPINNER_DOTS,
        Arrow = ffi::ScSpinnerType_SC_SPINNER_ARROW,
    } default Braille
);

repr_enum!(
    /// List marker style.
    ListMarker {
        Bullet = ffi::ScListMarker_SC_LIST_BULLET,
        Number = ffi::ScListMarker_SC_LIST_NUMBER,
        AlphaLower = ffi::ScListMarker_SC_LIST_ALPHA_LC,
        AlphaUpper = ffi::ScListMarker_SC_LIST_ALPHA_UC,
        RomanLower = ffi::ScListMarker_SC_LIST_ROMAN_LC,
        RomanUpper = ffi::ScListMarker_SC_LIST_ROMAN_UC,
    } default Bullet
);

/// A component title (panels, tables, rules).
#[derive(Clone, Debug, Default)]
pub struct Title {
    pub text: Option<String>,
    pub style: Style,
    pub align: Align,
    pub pad: i32,
    pub pos: Position,
}

impl Title {
    pub fn new(text: impl Into<String>) -> Self {
        Title {
            text: Some(text.into()),
            ..Default::default()
        }
    }
    pub fn style(mut self, s: Style) -> Self {
        self.style = s;
        self
    }
    pub fn align(mut self, a: Align) -> Self {
        self.align = a;
        self
    }
    pub fn pad(mut self, p: i32) -> Self {
        self.pad = p;
        self
    }
    pub fn pos(mut self, p: Position) -> Self {
        self.pos = p;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScTitle {
        ffi::ScTitle {
            text: a.opt(self.text.as_deref()),
            rich_text: std::ptr::null_mut(),
            style: self.style.raw(),
            halign: self.align.raw(),
            pad: self.pad,
            pos: self.pos.raw(),
        }
    }
}

/* ── Panels ──────────────────────────────────────────────────────────────── */

/// Options for [`panel`].
#[derive(Clone, Debug, Default)]
pub struct PanelOpts {
    pub border: BorderStyle,
    pub bg: Color,
    pub title: Title,
    pub subtitle: Title,
    pub full_width: bool,
    pub width: i32,
    pub content_align: Align,
    pub padding: Edges,
    pub margin: Edges,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl PanelOpts {
    pub fn new() -> Self {
        PanelOpts::default()
    }
    pub fn border(mut self, b: BorderStyle) -> Self {
        self.border = b;
        self
    }
    pub fn rounded(self) -> Self {
        self.border(BorderStyle::new(BorderType::Rounded))
    }
    pub fn single(self) -> Self {
        self.border(BorderStyle::new(BorderType::Single))
    }
    pub fn double(self) -> Self {
        self.border(BorderStyle::new(BorderType::Double))
    }
    pub fn thick(self) -> Self {
        self.border(BorderStyle::new(BorderType::Thick))
    }
    pub fn title(mut self, t: impl Into<String>) -> Self {
        self.title = Title::new(t);
        self
    }
    pub fn bg(mut self, c: Color) -> Self {
        self.bg = c;
        self
    }
    pub fn full_width(mut self) -> Self {
        self.full_width = true;
        self
    }
    pub fn width(mut self, w: i32) -> Self {
        self.width = w;
        self
    }
    pub fn content_align(mut self, a: Align) -> Self {
        self.content_align = a;
        self
    }
    pub fn padding(mut self, e: Edges) -> Self {
        self.padding = e;
        self
    }
    pub fn margin(mut self, e: Edges) -> Self {
        self.margin = e;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScPanelOpts {
        ffi::ScPanelOpts {
            border: self.border.raw(),
            bg: self.bg.raw(),
            title: self.title.raw(a),
            subtitle: self.subtitle.raw(a),
            full_width: self.full_width,
            width: self.width,
            content_align: self.content_align.raw(),
            padding: self.padding.raw(),
            margin: self.margin.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// Renders a bordered panel around `content`.
pub fn panel(content: &str, opts: PanelOpts) {
    let mut a = Arena::new();
    let o = opts.raw(&mut a);
    let c = a.cstr(content);
    unsafe { ffi::sc_panel_str(c, o) };
}

/// Panel from rich text.
pub fn panel_text(content: &Text, opts: PanelOpts) {
    let mut a = Arena::new();
    let o = opts.raw(&mut a);
    unsafe { ffi::sc_panel_text(content.as_ptr(), o) };
}

/* ── Rule ────────────────────────────────────────────────────────────────── */

/// Options for [`rule`].
#[derive(Clone, Debug, Default)]
pub struct RuleOpts {
    pub kind: BorderType,
    pub color: Color,
    pub title: Title,
    pub width: i32,
    pub align: Align,
    pub margin: Edges,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl RuleOpts {
    pub fn new() -> Self {
        RuleOpts::default()
    }
    pub fn kind(mut self, k: BorderType) -> Self {
        self.kind = k;
        self
    }
    pub fn color(mut self, c: Color) -> Self {
        self.color = c;
        self
    }
    pub fn title(mut self, t: impl Into<String>) -> Self {
        self.title = Title::new(t);
        self
    }
    pub fn width(mut self, w: i32) -> Self {
        self.width = w;
        self
    }
    pub fn align(mut self, a: Align) -> Self {
        self.align = a;
        self
    }
    pub fn margin(mut self, e: Edges) -> Self {
        self.margin = e;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScRuleOpts {
        ffi::ScRuleOpts {
            type_: self.kind.raw(),
            color: self.color.raw(),
            title: self.title.raw(a),
            width: self.width,
            halign: self.align.raw(),
            margin: self.margin.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// Draws a horizontal rule, optionally with a `title` set in the opts.
pub fn rule(opts: RuleOpts) {
    let mut a = Arena::new();
    let o = opts.raw(&mut a);
    unsafe { ffi::sc_rule_str(std::ptr::null(), o) };
}

/* ── Badge ───────────────────────────────────────────────────────────────── */

/// Options for [`badge`].
#[derive(Clone, Debug, Default)]
pub struct BadgeOpts {
    pub left_cap: Option<String>,
    pub right_cap: Option<String>,
    pub text_style: Style,
    pub pad: i32,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl BadgeOpts {
    pub fn new() -> Self {
        BadgeOpts::default()
    }
    pub fn caps(
        mut self,
        left: impl Into<String>,
        right: impl Into<String>,
    ) -> Self {
        self.left_cap = Some(left.into());
        self.right_cap = Some(right.into());
        self
    }
    pub fn style(mut self, s: Style) -> Self {
        self.text_style = s;
        self
    }
    pub fn pad(mut self, p: i32) -> Self {
        self.pad = p;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScBadgeOpts {
        ffi::ScBadgeOpts {
            left_cap: a.opt(self.left_cap.as_deref()),
            right_cap: a.opt(self.right_cap.as_deref()),
            text_style: self.text_style.raw(),
            pad: self.pad,
            ansi: self.ansi.raw(),
        }
    }
}

/// Prints an inline badge token (no trailing newline).
pub fn badge(text: &str, opts: BadgeOpts) {
    let mut a = Arena::new();
    let o = opts.raw(&mut a);
    let t = a.cstr(text);
    unsafe { ffi::sc_print_badge(t, o) };
}

/* ── Alerts ──────────────────────────────────────────────────────────────── */

/// Preset alert boxes.
pub mod alert {
    use super::*;

    pub fn show(kind: AlertType, content: &str) {
        unsafe { ffi::sc_alert_str(kind.raw(), cstring(content).as_ptr()) };
    }
    pub fn info(content: &str) {
        unsafe { ffi::sc_alert_info(cstring(content).as_ptr()) };
    }
    pub fn debug(content: &str) {
        unsafe { ffi::sc_alert_debug(cstring(content).as_ptr()) };
    }
    pub fn warning(content: &str) {
        unsafe { ffi::sc_alert_warning(cstring(content).as_ptr()) };
    }
    pub fn error(content: &str) {
        unsafe { ffi::sc_alert_error(cstring(content).as_ptr()) };
    }
    pub fn success(content: &str) {
        unsafe { ffi::sc_alert_success(cstring(content).as_ptr()) };
    }
}

/* ── Tables ──────────────────────────────────────────────────────────────── */

/// Per-column options.
#[derive(Clone, Debug, Default)]
pub struct ColOpts {
    pub min_width: i32,
    pub max_width: i32,
    pub fixed_width: i32,
    pub align: Align,
    pub valign: VAlign,
    pub word_wrap: bool,
    pub bg: Color,
    /// Default text style for unstyled cells in this column; lower priority
    /// than per-cell styling and the header/footer section styles.
    pub style: Style,
}

impl ColOpts {
    pub fn new() -> Self {
        ColOpts::default()
    }
    pub fn min_width(mut self, w: i32) -> Self {
        self.min_width = w;
        self
    }
    pub fn max_width(mut self, w: i32) -> Self {
        self.max_width = w;
        self
    }
    pub fn fixed_width(mut self, w: i32) -> Self {
        self.fixed_width = w;
        self
    }
    pub fn align(mut self, a: Align) -> Self {
        self.align = a;
        self
    }
    pub fn valign(mut self, v: VAlign) -> Self {
        self.valign = v;
        self
    }
    pub fn word_wrap(mut self, on: bool) -> Self {
        self.word_wrap = on;
        self
    }
    pub fn bg(mut self, c: Color) -> Self {
        self.bg = c;
        self
    }
    pub fn style(mut self, s: Style) -> Self {
        self.style = s;
        self
    }
    fn raw(&self) -> ffi::ScColOpts {
        ffi::ScColOpts {
            min_width: self.min_width,
            max_width: self.max_width,
            fixed_width: self.fixed_width,
            halign: self.align.raw(),
            valign: self.valign.raw(),
            word_wrap: self.word_wrap,
            bg: self.bg.raw(),
            style: self.style.raw(),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum CellKind {
    Str,
    Markup,
    Skip,
    RowSkip,
}

/// A table cell. Build from a string, or via the helpers for markup, rich
/// text and span/row skips.
#[derive(Clone, Debug)]
pub struct Cell {
    kind: CellKind,
    text: String,
    halign: Option<Align>,
    valign: Option<VAlign>,
    col_span: i32,
    row_span: i32,
}

impl Cell {
    pub fn new(s: impl Into<String>) -> Self {
        Cell {
            kind: CellKind::Str,
            text: s.into(),
            halign: None,
            valign: None,
            col_span: 0,
            row_span: 0,
        }
    }
    /// A cell whose content is parsed as inline markup (owned by the table).
    pub fn markup(s: impl Into<String>) -> Self {
        Cell {
            kind: CellKind::Markup,
            ..Cell::new(s)
        }
    }
    /// Placeholder covered by a colspan.
    pub fn skip() -> Self {
        Cell {
            kind: CellKind::Skip,
            ..Cell::new("")
        }
    }
    /// Placeholder row covered by a rowspan.
    pub fn row_skip() -> Self {
        Cell {
            kind: CellKind::RowSkip,
            ..Cell::new("")
        }
    }
    pub fn align(mut self, a: Align) -> Self {
        self.halign = Some(a);
        self
    }
    pub fn valign(mut self, v: VAlign) -> Self {
        self.valign = Some(v);
        self
    }
    pub fn colspan(mut self, n: i32) -> Self {
        self.col_span = n;
        self
    }
    pub fn rowspan(mut self, n: i32) -> Self {
        self.row_span = n;
        self
    }
}

impl From<&str> for Cell {
    fn from(s: &str) -> Self {
        Cell::new(s)
    }
}
impl From<String> for Cell {
    fn from(s: String) -> Self {
        Cell::new(s)
    }
}

/// Table border options.
#[derive(Clone, Debug, Default)]
pub struct TableBorder {
    pub kind: BorderType,
    pub outer_color: Color,
    pub inner_color: Color,
    pub no_outer: bool,
    pub no_inner_h: bool,
    pub no_inner_v: bool,
}

/// Header settings.
#[derive(Clone, Debug, Default)]
pub struct TableHeader {
    pub row: bool,
    pub col: bool,
    pub row_bg: Color,
    pub col_bg: Color,
    pub style: Style,
}

/// Options for printing a [`Table`].
#[derive(Clone, Debug, Default)]
pub struct TableOpts {
    pub border: TableBorder,
    pub header: TableHeader,
    pub striped: bool,
    pub stripe_bg: Color,
    pub title: Title,
    pub cell_pad: Edges,
    pub margin: Edges,
    pub total_width: i32,
    pub max_rows: i32,
    pub right_to_left: bool,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl TableOpts {
    pub fn new() -> Self {
        TableOpts::default()
    }
    pub fn border(mut self, kind: BorderType) -> Self {
        self.border.kind = kind;
        self
    }
    pub fn header_row(mut self, on: bool) -> Self {
        self.header.row = on;
        self
    }
    pub fn header_col(mut self, on: bool) -> Self {
        self.header.col = on;
        self
    }
    pub fn striped(mut self, on: bool) -> Self {
        self.striped = on;
        self
    }
    pub fn title(mut self, t: impl Into<String>) -> Self {
        self.title = Title::new(t);
        self
    }
    pub fn total_width(mut self, w: i32) -> Self {
        self.total_width = w;
        self
    }
    pub fn max_rows(mut self, n: i32) -> Self {
        self.max_rows = n;
        self
    }
    pub fn cell_pad(mut self, e: Edges) -> Self {
        self.cell_pad = e;
        self
    }
    pub fn margin(mut self, e: Edges) -> Self {
        self.margin = e;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScTableOpts {
        ffi::ScTableOpts {
            border: ffi::ScTableBorder {
                type_: self.border.kind.raw(),
                outer_color: self.border.outer_color.raw(),
                inner_color: self.border.inner_color.raw(),
                header_row_sep_color: Color::NONE.raw(),
                header_col_sep_color: Color::NONE.raw(),
                no_outer: self.border.no_outer,
                no_inner_h: self.border.no_inner_h,
                no_inner_v: self.border.no_inner_v,
            },
            header: ffi::ScTableHeader {
                row: self.header.row,
                col: self.header.col,
                row_bg: self.header.row_bg.raw(),
                col_bg: self.header.col_bg.raw(),
                style: self.header.style.raw(),
            },
            striped: self.striped,
            stripe_bg: self.stripe_bg.raw(),
            footer: ffi::ScTableFooter {
                row_bg: Color::NONE.raw(),
                col_bg: Color::NONE.raw(),
                style: Style::default().raw(),
            },
            title: self.title.raw(a),
            cell_pad: self.cell_pad.raw(),
            margin: self.margin.raw(),
            total_width: self.total_width,
            max_rows: self.max_rows,
            right_to_left: self.right_to_left,
            ansi: self.ansi.raw(),
        }
    }
}

/// An owning table. Cell strings are copied into an internal arena that lives
/// as long as the table, so passing temporaries to [`Table::row`] is safe.
pub struct Table {
    ptr: *mut ffi::ScTableData,
    arena: Vec<std::ffi::CString>,
}

impl Table {
    pub fn new() -> Table {
        let ptr = unsafe { ffi::sc_table_new() };
        assert!(!ptr.is_null(), "sc_table_new: out of memory");
        Table {
            ptr,
            arena: Vec::new(),
        }
    }

    /// Adds a column with a header label.
    pub fn column(&mut self, header: &str, opts: ColOpts) -> &mut Self {
        let h = self.intern(header);
        unsafe { ffi::sc_table_add_column(self.ptr, h, opts.raw()) };
        self
    }

    /// Adds a data row.
    pub fn row<I, C>(&mut self, cells: I) -> &mut Self
    where
        I: IntoIterator<Item = C>,
        C: Into<Cell>,
    {
        let mut v = self.build_cells(cells);
        unsafe { ffi::sc_table_add_row(self.ptr, v.as_mut_ptr(), v.len()) };
        self
    }

    /// Adds a data row with a background color.
    pub fn row_bg<I, C>(&mut self, cells: I, bg: Color) -> &mut Self
    where
        I: IntoIterator<Item = C>,
        C: Into<Cell>,
    {
        let mut v = self.build_cells(cells);
        unsafe {
            ffi::sc_table_add_row_bg(
                self.ptr,
                v.as_mut_ptr(),
                v.len(),
                bg.raw(),
            )
        };
        self
    }

    /// Adds a footer row.
    pub fn footer_row<I, C>(&mut self, cells: I) -> &mut Self
    where
        I: IntoIterator<Item = C>,
        C: Into<Cell>,
    {
        let mut v = self.build_cells(cells);
        unsafe {
            ffi::sc_table_add_footer_row(self.ptr, v.as_mut_ptr(), v.len())
        };
        self
    }

    /// Renders the table.
    pub fn print(&self, opts: TableOpts) {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        unsafe { ffi::sc_table_print(self.ptr, o) };
    }

    pub(crate) fn as_ptr(&self) -> *const ffi::ScTableData {
        self.ptr
    }

    fn intern(&mut self, s: &str) -> *const std::os::raw::c_char {
        self.arena.push(cstring(s));
        self.arena.last().unwrap().as_ptr()
    }

    fn build_cells<I, C>(&mut self, cells: I) -> Vec<ffi::ScCell>
    where
        I: IntoIterator<Item = C>,
        C: Into<Cell>,
    {
        cells
            .into_iter()
            .map(|c| {
                let c = c.into();
                let mut raw = unsafe {
                    match c.kind {
                        CellKind::Str => {
                            ffi::sc_cell_from_str(self.intern(&c.text))
                        }
                        CellKind::Markup => {
                            ffi::sc_cell_from_markup(cstring(&c.text).as_ptr())
                        }
                        CellKind::Skip => ffi::sc_cell_skip_placeholder(),
                        CellKind::RowSkip => ffi::sc_row_skip_placeholder(),
                    }
                };
                if let Some(h) = c.halign {
                    raw.halign_set = true;
                    raw.halign = h.raw();
                }
                if let Some(v) = c.valign {
                    raw.valign_set = true;
                    raw.valign = v.raw();
                }
                if c.col_span != 0 {
                    raw.col_span = c.col_span;
                }
                if c.row_span != 0 {
                    raw.row_span = c.row_span;
                }
                raw
            })
            .collect()
    }
}

impl Default for Table {
    fn default() -> Self {
        Table::new()
    }
}

impl Drop for Table {
    fn drop(&mut self) {
        unsafe { ffi::sc_table_free(self.ptr) };
    }
}

/* ── Lists ───────────────────────────────────────────────────────────────── */

/// Options for a [`List`].
#[derive(Clone, Debug, Default)]
pub struct ListOpts {
    pub marker: ListMarker,
    pub bullet: Option<String>,
    pub marker_prefix: Option<String>,
    pub marker_suffix: Option<String>,
    pub marker_style: Style,
    pub indent: i32,
    pub item_gap: i32,
    pub width: i32,
    pub margin: Edges,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl ListOpts {
    pub fn new() -> Self {
        ListOpts::default()
    }
    pub fn marker(mut self, m: ListMarker) -> Self {
        self.marker = m;
        self
    }
    pub fn bullet(mut self, b: impl Into<String>) -> Self {
        self.bullet = Some(b.into());
        self
    }
    pub fn marker_style(mut self, s: Style) -> Self {
        self.marker_style = s;
        self
    }
    pub fn indent(mut self, i: i32) -> Self {
        self.indent = i;
        self
    }
    pub fn item_gap(mut self, g: i32) -> Self {
        self.item_gap = g;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScListOpts {
        ffi::ScListOpts {
            marker: self.marker.raw(),
            bullet: a.opt(self.bullet.as_deref()),
            marker_prefix: a.opt(self.marker_prefix.as_deref()),
            marker_suffix: a.opt(self.marker_suffix.as_deref()),
            marker_style: self.marker_style.raw(),
            indent: self.indent,
            item_gap: self.item_gap,
            width: self.width,
            margin: self.margin.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// An owning bulleted/numbered list (with arbitrary nesting).
pub struct List {
    ptr: *mut ffi::ScList,
    owns: bool,
}

impl List {
    pub fn new(opts: ListOpts) -> List {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let ptr = unsafe { ffi::sc_list_new(o) };
        assert!(!ptr.is_null(), "sc_list_new: out of memory");
        List { ptr, owns: true }
    }

    /// Adds a string item; returns a handle for attaching a sub-list.
    pub fn add(&mut self, s: &str, style: Style) -> ListItem {
        let item = unsafe {
            ffi::sc_list_add_str(self.ptr, cstring(s).as_ptr(), style.raw())
        };
        ListItem { ptr: item }
    }

    /// Adds a rich-text item (the list takes ownership of `t`).
    pub fn add_text(&mut self, t: Text) -> ListItem {
        // The C list borrows the ScText; leak it so the borrow stays valid for
        // the list's lifetime (freed with the process - lists are short-lived).
        let raw = t.as_mut_ptr();
        std::mem::forget(t);
        let item = unsafe { ffi::sc_list_add_text(self.ptr, raw) };
        ListItem { ptr: item }
    }

    pub fn print(&self) {
        unsafe { ffi::sc_list_print(self.ptr) };
    }

    pub(crate) fn as_ptr(&self) -> *const ffi::ScList {
        self.ptr
    }
}

impl Drop for List {
    fn drop(&mut self) {
        if self.owns {
            unsafe { ffi::sc_list_free(self.ptr) };
        }
    }
}

/// Handle to a list item; use [`ListItem::sub`] to nest a list.
pub struct ListItem {
    ptr: *mut ffi::ScListItem,
}

impl ListItem {
    /// Attaches a sub-list (owned by the parent).
    pub fn sub(&self, opts: ListOpts) -> List {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let ptr = unsafe { ffi::sc_list_add_sub(self.ptr, o) };
        assert!(!ptr.is_null(), "sc_list_add_sub: out of memory");
        List { ptr, owns: false }
    }
}

/* ── Trees ───────────────────────────────────────────────────────────────── */

/// Options for a [`Tree`].
#[derive(Clone, Debug, Default)]
pub struct TreeOpts {
    pub kind: BorderType,
    pub connector_color: Color,
    pub indent: i32,
    pub no_guide: bool,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl TreeOpts {
    pub fn new() -> Self {
        TreeOpts::default()
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self) -> ffi::ScTreeOpts {
        ffi::ScTreeOpts {
            type_: self.kind.raw(),
            connector_color: self.connector_color.raw(),
            indent: self.indent,
            no_guide: self.no_guide,
            ansi: self.ansi.raw(),
        }
    }
}

/// Non-owning handle to a tree node.
#[derive(Clone, Copy)]
pub struct TreeNode {
    ptr: *mut ffi::ScTreeNode,
}

impl TreeNode {
    /// The root sentinel (pass as `parent` to add a top-level node).
    pub fn root() -> TreeNode {
        TreeNode {
            ptr: std::ptr::null_mut(),
        }
    }
}

/// An owning tree view.
pub struct Tree {
    ptr: *mut ffi::ScTree,
}

impl Tree {
    pub fn new(opts: TreeOpts) -> Tree {
        let ptr = unsafe { ffi::sc_tree_new(opts.raw()) };
        assert!(!ptr.is_null(), "sc_tree_new: out of memory");
        Tree { ptr }
    }

    /// Adds a string node under `parent` ([`TreeNode::root`] for a root node).
    pub fn add(&mut self, s: &str, parent: TreeNode, style: Style) -> TreeNode {
        let node = unsafe {
            ffi::sc_tree_add_str(
                self.ptr,
                parent.ptr,
                cstring(s).as_ptr(),
                style.raw(),
                std::ptr::null(),
                Style::default().raw(),
            )
        };
        TreeNode { ptr: node }
    }

    pub fn print(&self) {
        unsafe { ffi::sc_tree_print(self.ptr) };
    }

    pub(crate) fn as_ptr(&self) -> *const ffi::ScTree {
        self.ptr
    }
}

impl Drop for Tree {
    fn drop(&mut self) {
        unsafe { ffi::sc_tree_free(self.ptr) };
    }
}

/* ── Key/Value ───────────────────────────────────────────────────────────── */

/// Options for a [`Kv`] block.
#[derive(Clone, Debug, Default)]
pub struct KvOpts {
    pub sep: Option<String>,
    pub key_width: i32,
    pub width: i32,
    pub margin: Edges,
    pub item_gap: i32,
    pub wrap_val: bool,
    pub key_style: Style,
    pub val_style: Style,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl KvOpts {
    pub fn new() -> Self {
        KvOpts::default()
    }
    pub fn key_width(mut self, w: i32) -> Self {
        self.key_width = w;
        self
    }
    pub fn key_style(mut self, s: Style) -> Self {
        self.key_style = s;
        self
    }
    pub fn wrap_val(mut self, on: bool) -> Self {
        self.wrap_val = on;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScKVOpts {
        ffi::ScKVOpts {
            sep: a.opt(self.sep.as_deref()),
            key_width: self.key_width,
            width: self.width,
            margin: self.margin.raw(),
            item_gap: self.item_gap,
            wrap_val: self.wrap_val,
            key_style: self.key_style.raw(),
            val_style: self.val_style.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// An owning key/value block (both key and value copied by the C side).
pub struct Kv {
    ptr: *mut ffi::ScKV,
}

impl Kv {
    pub fn new(opts: KvOpts) -> Kv {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let ptr = unsafe { ffi::sc_kv_new(o) };
        assert!(!ptr.is_null(), "sc_kv_new: out of memory");
        Kv { ptr }
    }
    pub fn add(&mut self, key: &str, value: &str) -> &mut Self {
        unsafe {
            ffi::sc_kv_add(
                self.ptr,
                cstring(key).as_ptr(),
                cstring(value).as_ptr(),
            )
        };
        self
    }
    pub fn print(&self) {
        unsafe { ffi::sc_kv_print(self.ptr) };
    }
    pub(crate) fn as_ptr(&self) -> *const ffi::ScKV {
        self.ptr
    }
}

impl Drop for Kv {
    fn drop(&mut self) {
        unsafe { ffi::sc_kv_free(self.ptr) };
    }
}

/* ── Columns ─────────────────────────────────────────────────────────────── */

/// Per-column placement options for a [`Columns`] layout.
#[derive(Clone, Debug, Default)]
pub struct ColItem {
    pub min_w: i32,
    pub max_w: i32,
    pub fixed_w: i32,
    pub align: Align,
    pub valign: Option<VAlign>,
    pub bg: Color,
    pub stretch: bool,
}

impl ColItem {
    pub fn new() -> Self {
        ColItem::default()
    }
    pub fn fixed(mut self, w: i32) -> Self {
        self.fixed_w = w;
        self
    }
    pub fn align(mut self, a: Align) -> Self {
        self.align = a;
        self
    }
    pub fn valign(mut self, v: VAlign) -> Self {
        self.valign = Some(v);
        self
    }
    fn raw(&self) -> ffi::ScColItem {
        ffi::ScColItem {
            min_w: self.min_w,
            max_w: self.max_w,
            fixed_w: self.fixed_w,
            halign: self.align.raw(),
            valign_set: self.valign.is_some(),
            valign: self.valign.unwrap_or_default().raw(),
            bg: self.bg.raw(),
            stretch: self.stretch,
        }
    }
}

/// Options for a [`Columns`] layout.
#[derive(Clone, Debug, Default)]
pub struct ColumnsOpts {
    pub gap: i32,
    pub sep: BorderStyle,
    pub valign: VAlign,
    pub total_width: i32,
    pub margin: Edges,
}

impl ColumnsOpts {
    pub fn new() -> Self {
        ColumnsOpts::default()
    }
    pub fn gap(mut self, g: i32) -> Self {
        self.gap = g;
        self
    }
    pub fn separator(mut self, s: BorderStyle) -> Self {
        self.sep = s;
        self
    }
    fn raw(&self) -> ffi::ScColumnsOpts {
        ffi::ScColumnsOpts {
            gap: self.gap,
            sep: self.sep.raw(),
            valign: self.valign.raw(),
            total_width: self.total_width,
            margin: self.margin.raw(),
        }
    }
}

/// An owning side-by-side layout. Each `add*` captures the widget eagerly.
pub struct Columns {
    ptr: *mut ffi::ScColumns,
}

impl Columns {
    pub fn new(opts: ColumnsOpts) -> Columns {
        let ptr = unsafe { ffi::sc_columns_new(opts.raw()) };
        assert!(!ptr.is_null(), "sc_columns_new: out of memory");
        Columns { ptr }
    }
    pub fn add_str(&mut self, s: &str, item: ColItem) -> &mut Self {
        unsafe {
            ffi::sc_columns_add_str(self.ptr, cstring(s).as_ptr(), item.raw())
        };
        self
    }
    pub fn add_text(&mut self, t: &Text, item: ColItem) -> &mut Self {
        unsafe { ffi::sc_columns_add_text(self.ptr, t.as_ptr(), item.raw()) };
        self
    }
    pub fn add_table(
        &mut self,
        t: &Table,
        opts: TableOpts,
        item: ColItem,
    ) -> &mut Self {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        unsafe {
            ffi::sc_columns_add_table(self.ptr, t.as_ptr(), o, item.raw())
        };
        self
    }
    pub fn add_panel(
        &mut self,
        content: &str,
        opts: PanelOpts,
        item: ColItem,
    ) -> &mut Self {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let c = a.cstr(content);
        unsafe { ffi::sc_columns_add_panel_str(self.ptr, c, o, item.raw()) };
        self
    }
    pub fn add_list(&mut self, l: &List, item: ColItem) -> &mut Self {
        unsafe { ffi::sc_columns_add_list(self.ptr, l.as_ptr(), item.raw()) };
        self
    }
    pub fn add_tree(&mut self, t: &Tree, item: ColItem) -> &mut Self {
        unsafe { ffi::sc_columns_add_tree(self.ptr, t.as_ptr(), item.raw()) };
        self
    }
    pub fn add_rendered(&mut self, r: &Rendered, item: ColItem) -> &mut Self {
        unsafe { ffi::sc_columns_add_rendered(self.ptr, r.ptr, item.raw()) };
        self
    }
    pub fn print(&self) {
        unsafe { ffi::sc_columns_print(self.ptr) };
    }
    pub(crate) fn as_ptr(&self) -> *const ffi::ScColumns {
        self.ptr
    }
}

impl Drop for Columns {
    fn drop(&mut self) {
        unsafe { ffi::sc_columns_free(self.ptr) };
    }
}

/* ── Progress bar ────────────────────────────────────────────────────────── */

/// Options for a [`ProgressBar`].
#[derive(Clone, Debug, Default)]
pub struct ProgressBarOpts {
    pub kind: ProgressType,
    pub left_cap: Option<String>,
    pub right_cap: Option<String>,
    pub fill_color: Color,
    pub empty_color: Color,
    pub show_percent: bool,
    pub show_value: bool,
    pub bar_width: i32,
    pub width: i32,
    pub label_width: i32,
    pub label_style: Style,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl ProgressBarOpts {
    pub fn new() -> Self {
        ProgressBarOpts::default()
    }
    pub fn kind(mut self, k: ProgressType) -> Self {
        self.kind = k;
        self
    }
    pub fn brackets(mut self) -> Self {
        self.left_cap = Some("[".into());
        self.right_cap = Some("]".into());
        self
    }
    pub fn fill_color(mut self, c: Color) -> Self {
        self.fill_color = c;
        self
    }
    pub fn show_percent(mut self, on: bool) -> Self {
        self.show_percent = on;
        self
    }
    pub fn show_value(mut self, on: bool) -> Self {
        self.show_value = on;
        self
    }
    pub fn width(mut self, w: i32) -> Self {
        self.width = w;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    pub(crate) fn raw(&self, a: &mut Arena) -> ffi::ScProgressBarOpts {
        ffi::ScProgressBarOpts {
            type_: self.kind.raw(),
            left_cap: a.opt(self.left_cap.as_deref()),
            right_cap: a.opt(self.right_cap.as_deref()),
            fill_color: self.fill_color.raw(),
            empty_color: self.empty_color.raw(),
            thresholds: ffi::ScProgressThresholds {
                enabled: false,
                mid: 0.0,
                high: 0.0,
                color_low: Color::NONE.raw(),
                color_mid: Color::NONE.raw(),
                color_high: Color::NONE.raw(),
            },
            show_percent: self.show_percent,
            show_value: self.show_value,
            bar_width: self.bar_width,
            width: self.width,
            label_width: self.label_width,
            label_style: self.label_style.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// An owning, animated progress bar. Allocate once, `draw` in a loop, `finish`.
pub struct ProgressBar {
    ptr: *mut ffi::ScProgressBar,
}

impl ProgressBar {
    pub fn new(opts: ProgressBarOpts) -> ProgressBar {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let ptr = unsafe { ffi::sc_progressbar_new(o) };
        assert!(!ptr.is_null(), "sc_progressbar_new: out of memory");
        ProgressBar { ptr }
    }
    pub fn set_label(&mut self, label: &str) {
        unsafe {
            ffi::sc_progressbar_set_label(self.ptr, cstring(label).as_ptr())
        };
    }
    /// Redraws in place. `max == 0.0` means `value` is already a 0..1 ratio.
    pub fn draw(&mut self, value: f64, max: f64) {
        unsafe { ffi::sc_progressbar_draw(self.ptr, value, max) };
    }
    pub fn finish(&mut self, value: f64, max: f64) {
        unsafe { ffi::sc_progressbar_finish(self.ptr, value, max) };
    }
}

impl Drop for ProgressBar {
    fn drop(&mut self) {
        unsafe { ffi::sc_progressbar_free(self.ptr) };
    }
}

/* ── Spinner ─────────────────────────────────────────────────────────────── */

/// Options for a [`Spinner`].
#[derive(Clone, Debug, Default)]
pub struct SpinnerOpts {
    pub kind: SpinnerType,
    pub color: Color,
    pub label_style: Style,
    /// ANSI passthrough for user strings; zero-init inherits the
    /// process-wide [`set_allow_ansi`](crate::set_allow_ansi) setting.
    pub ansi: AnsiMode,
}

impl SpinnerOpts {
    pub fn new() -> Self {
        SpinnerOpts::default()
    }
    pub fn kind(mut self, k: SpinnerType) -> Self {
        self.kind = k;
        self
    }
    pub fn color(mut self, c: Color) -> Self {
        self.color = c;
        self
    }
    /// Sets the ANSI passthrough mode for this widget's strings.
    pub fn ansi(mut self, mode: AnsiMode) -> Self {
        self.ansi = mode;
        self
    }
    fn raw(&self) -> ffi::ScSpinnerOpts {
        ffi::ScSpinnerOpts {
            type_: self.kind.raw(),
            color: self.color.raw(),
            label_style: self.label_style.raw(),
            ansi: self.ansi.raw(),
        }
    }
}

/// An owning, animated spinner.
pub struct Spinner {
    ptr: *mut ffi::ScSpinner,
}

impl Spinner {
    pub fn new(label: &str, opts: SpinnerOpts) -> Spinner {
        let ptr =
            unsafe { ffi::sc_spinner_new(cstring(label).as_ptr(), opts.raw()) };
        assert!(!ptr.is_null(), "sc_spinner_new: out of memory");
        Spinner { ptr }
    }
    pub fn set_label(&mut self, label: &str) {
        unsafe { ffi::sc_spinner_set_label(self.ptr, cstring(label).as_ptr()) };
    }
    pub fn tick(&mut self) {
        unsafe { ffi::sc_spinner_tick(self.ptr) };
    }
    pub fn finish(&mut self, success: bool, label: &str) {
        unsafe {
            ffi::sc_spinner_finish(self.ptr, success, cstring(label).as_ptr())
        };
    }
}

impl Drop for Spinner {
    fn drop(&mut self) {
        unsafe { ffi::sc_spinner_free(self.ptr) };
    }
}

/* ── Capture / compose ───────────────────────────────────────────────────── */

/// Padding options for [`Rendered::pad`].
#[derive(Clone, Copy, Debug, Default)]
pub struct PadOpts {
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
    pub left: i32,
}

impl PadOpts {
    pub fn new() -> Self {
        PadOpts::default()
    }
    fn raw(self) -> ffi::ScPadOpts {
        ffi::ScPadOpts {
            top: self.top,
            right: self.right,
            bottom: self.bottom,
            left: self.left,
        }
    }
}

/// An owning captured rendering of a widget. Print it with [`Rendered::pad`] /
/// [`Rendered::align`], place it in a [`Columns`], or read its
/// [`lines`](Self::lines).
pub struct Rendered {
    ptr: *mut ffi::ScRendered,
}

impl Rendered {
    pub(crate) fn from_raw(ptr: *mut ffi::ScRendered) -> Rendered {
        assert!(!ptr.is_null(), "capture: out of memory");
        Rendered { ptr }
    }

    /// The rendered lines (ANSI codes included).
    pub fn lines(&self) -> Vec<String> {
        unsafe {
            let r = &*self.ptr;
            (0..r.line_count)
                .map(|i| {
                    let p = *r.lines.add(i);
                    std::ffi::CStr::from_ptr(p).to_string_lossy().into_owned()
                })
                .collect()
        }
    }

    pub fn line_count(&self) -> usize {
        unsafe { (*self.ptr).line_count }
    }

    /// Prints with padding.
    pub fn pad(&self, opts: PadOpts) {
        unsafe { ffi::sc_pad_print(self.ptr, opts.raw()) };
    }

    /// Prints aligned within `width` columns (`0` = terminal width).
    pub fn align(&self, align: Align, width: i32) {
        unsafe { ffi::sc_align_print(self.ptr, align.raw(), width) };
    }
}

impl Drop for Rendered {
    fn drop(&mut self) {
        unsafe { ffi::sc_rendered_free(self.ptr) };
    }
}

/// Capture widgets into reusable [`Rendered`] buffers.
pub mod capture {
    use super::*;

    pub fn str(s: &str) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_str(cstring(s).as_ptr()) })
    }
    pub fn text(t: &Text) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_text(t.as_ptr()) })
    }
    pub fn table(t: &Table, opts: TableOpts) -> Rendered {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        Rendered::from_raw(unsafe { ffi::sc_capture_table(t.as_ptr(), o) })
    }
    pub fn list(l: &List) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_list(l.as_ptr()) })
    }
    pub fn tree(t: &Tree) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_tree(t.as_ptr()) })
    }
    pub fn kv(k: &Kv) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_kv(k.as_ptr()) })
    }
    pub fn columns(c: &Columns) -> Rendered {
        Rendered::from_raw(unsafe { ffi::sc_capture_columns(c.as_ptr()) })
    }
    pub fn panel(content: &str, opts: PanelOpts) -> Rendered {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        let c = a.cstr(content);
        Rendered::from_raw(unsafe { ffi::sc_capture_panel_str(c, o) })
    }
    pub fn rule(opts: RuleOpts) -> Rendered {
        let mut a = Arena::new();
        let o = opts.raw(&mut a);
        Rendered::from_raw(unsafe {
            ffi::sc_capture_rule_str(std::ptr::null(), o)
        })
    }
}

/// Stacks captured renderings top-to-bottom into one column, with `gap` blank
/// lines between adjacent parts.
pub fn vstack(parts: &[&Rendered], gap: i32) -> Option<Rendered> {
    if parts.is_empty() {
        return None;
    }
    let raw: Vec<*const ffi::ScRendered> =
        parts.iter().map(|r| r.ptr as *const _).collect();
    let out = unsafe { ffi::sc_vstack(raw.as_ptr(), raw.len(), gap) };
    if out.is_null() {
        None
    } else {
        Some(Rendered { ptr: out })
    }
}

/// Prints a string with padding (capture + print in one step).
pub fn pad_str(s: &str, opts: PadOpts) {
    unsafe { ffi::sc_pad_str(cstring(s).as_ptr(), opts.raw()) };
}

/// Prints a string aligned within `width` columns.
pub fn align_str(s: &str, align: Align, width: i32) {
    unsafe { ffi::sc_align_str(cstring(s).as_ptr(), align.raw(), width) };
}

/* ── Live display ───────────────────────────────────────────────────────── */

/// Options for [`Live::begin`].
#[derive(Clone, Copy, Debug, Default)]
pub struct LiveOpts {
    /// Render fullscreen on the alternate screen buffer (htop-style); the
    /// previous terminal content is restored when the session ends.
    pub alt_screen: bool,

    /// Keep the cursor visible (default: hidden during the session).
    pub show_cursor: bool,

    /// Erase the live region on end instead of leaving the final frame.
    pub transient: bool,

    /// Emit redraw escape codes even when output is not a terminal.
    pub always: bool,

    /// Rows reserved below the frame for an interactive prompt (REPL
    /// dashboards): the cursor is parked there after every update. Reserve
    /// as many rows as the prompt draws. 0 = classic behavior.
    pub prompt_rows: i32,
}

impl LiveOpts {
    pub fn new() -> Self {
        Self::default()
    }
    pub fn alt_screen(mut self) -> Self {
        self.alt_screen = true;
        self
    }
    pub fn show_cursor(mut self) -> Self {
        self.show_cursor = true;
        self
    }
    pub fn transient(mut self) -> Self {
        self.transient = true;
        self
    }
    pub fn always(mut self) -> Self {
        self.always = true;
        self
    }
    pub fn prompt_rows(mut self, rows: i32) -> Self {
        self.prompt_rows = rows;
        self
    }
}

/// A live-display session: re-renders a composed frame in place, so multiple
/// widgets form a continuously updating dashboard.
///
/// Frames are built with the capture API ([`capture`], [`vstack`]); the
/// session only handles the in-place redraw. When the output stream is not a
/// terminal, updates are buffered and only the final frame is printed when
/// the session ends (clean output in scripts and CI).
///
/// ```no_run
/// use sparcli::{capture, Live, LiveOpts};
///
/// let live = Live::begin(LiveOpts::new());
/// for percent in (0..=100).step_by(10) {
///     let frame = capture::str(&format!("progress: {percent}%"));
///     live.update(&frame);
/// }
/// live.end();
/// ```
pub struct Live {
    ptr: *mut ffi::ScLive,
}

impl Live {
    /// Starts a live session on the current thread's output stream.
    pub fn begin(opts: LiveOpts) -> Live {
        let raw_opts = ffi::ScLiveOpts {
            alt_screen: opts.alt_screen,
            show_cursor: opts.show_cursor,
            transient: opts.transient,
            always: opts.always,
            prompt_rows: opts.prompt_rows,
        };
        let ptr = unsafe { ffi::sc_live_begin(raw_opts) };
        Live { ptr }
    }

    /// Redraws the live region with a captured frame.
    pub fn update(&self, frame: &Rendered) {
        if !self.ptr.is_null() {
            unsafe { ffi::sc_live_update(self.ptr, frame.ptr) };
        }
    }

    /// Redraws the live region with a plain (multi-line) string.
    pub fn update_str(&self, content: &str) {
        if self.ptr.is_null() {
            return;
        }
        let c = cstring(content);
        unsafe { ffi::sc_live_update_str(self.ptr, c.as_ptr()) };
    }

    /// Ends the session: restores the terminal and, off-terminal, prints
    /// the buffered final frame.
    pub fn end(mut self) {
        self.finish();
    }

    /// Shared end path for `end()` and `Drop`.
    fn finish(&mut self) {
        if !self.ptr.is_null() {
            unsafe { ffi::sc_live_end(self.ptr) };
            self.ptr = std::ptr::null_mut();
        }
    }
}

impl Drop for Live {
    fn drop(&mut self) {
        self.finish();
    }
}
