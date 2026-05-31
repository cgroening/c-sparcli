"""Feature-rich tables (:class:`Table`) and their options."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field

from ._ffi import (apply_color, apply_edges, apply_style, apply_title, cstr,
                   ffi, lib)
from .color import Color
from .enums import Align, BorderType, VAlign
from .style import Edges, Style, Title
from .text import Text

_KIND_STR, _KIND_TEXT, _KIND_MARKUP = 0, 1, 2


@dataclass
class Cell:
    """One table cell. Plain values are usually passed as ``str`` directly; use
    :class:`Cell` for alignment overrides, colspan/rowspan, markup or rich text.
    """

    value: "str | Text | None" = ""
    kind: str = "str"  # "str" | "text" | "markup" | "skip" | "row_skip"
    halign: Align | None = None
    valign: VAlign | None = None
    colspan: int = 0
    rowspan: int = 0

    @classmethod
    def markup(cls, markup: str, **kw) -> "Cell":
        """A cell whose text is parsed from inline markup (owned by the table)."""
        return cls(markup, kind="markup", **kw)

    @classmethod
    def text(cls, t: Text, **kw) -> "Cell":
        """A rich-text cell (the :class:`~sparcli.text.Text` must outlive printing)."""
        return cls(t, kind="text", **kw)

    @classmethod
    def skip(cls) -> "Cell":
        """Placeholder covered by a preceding colspan."""
        return cls("", kind="skip")

    @classmethod
    def row_skip(cls) -> "Cell":
        """Placeholder covered by a preceding rowspan."""
        return cls("", kind="row_skip")


def _coerce_cell(x: "str | Text | Cell") -> Cell:
    if isinstance(x, Cell):
        return x
    if isinstance(x, Text):
        return Cell.text(x)
    return Cell(x)


@dataclass
class ColOpts:
    """Per-column display options."""

    min_width: int = 0
    max_width: int = 0
    fixed_width: int = 0
    halign: Align = Align.LEFT
    valign: VAlign = VAlign.TOP
    word_wrap: bool = False
    bg: Color = Color.NONE

    def _fill(self, c) -> None:
        c.min_width = self.min_width
        c.max_width = self.max_width
        c.fixed_width = self.fixed_width
        c.halign = int(self.halign)
        c.valign = int(self.valign)
        c.word_wrap = self.word_wrap
        apply_color(c.bg, self.bg)


@dataclass
class TableBorder:
    """Granular table border styling (use in place of a bare ``BorderType``)."""

    type: BorderType = BorderType.SINGLE
    outer_color: Color = Color.NONE
    inner_color: Color = Color.NONE
    header_row_sep_color: Color = Color.NONE
    header_col_sep_color: Color = Color.NONE
    no_outer: bool = False
    no_inner_h: bool = False
    no_inner_v: bool = False


@dataclass
class TableOpts:
    """Rendering options for :meth:`Table.print` / capture.

    Common header/footer toggles are flattened here; pass a :class:`TableBorder`
    as ``border`` for granular separator colors.
    """

    border: "BorderType | TableBorder" = BorderType.SINGLE
    header_row: bool = False
    header_col: bool = False
    header_row_bg: Color = Color.NONE
    header_col_bg: Color = Color.NONE
    header_style: Style = field(default_factory=Style)
    striped: bool = False
    stripe_bg: Color = Color.NONE
    footer_row_bg: Color = Color.NONE
    footer_col_bg: Color = Color.NONE
    footer_style: Style = field(default_factory=Style)
    title: "str | Title | None" = None
    cell_pad: Edges = field(default_factory=Edges)
    margin: Edges = field(default_factory=Edges)
    total_width: int = 0
    max_rows: int = 0
    rtl: bool = False

    def _fill(self, c, arena: list) -> None:
        b = self.border if isinstance(self.border, TableBorder) else TableBorder(type=self.border)
        c.border.type = int(b.type)
        apply_color(c.border.outer_color, b.outer_color)
        apply_color(c.border.inner_color, b.inner_color)
        apply_color(c.border.header_row_sep_color, b.header_row_sep_color)
        apply_color(c.border.header_col_sep_color, b.header_col_sep_color)
        c.border.no_outer = b.no_outer
        c.border.no_inner_h = b.no_inner_h
        c.border.no_inner_v = b.no_inner_v

        c.header.row = self.header_row
        c.header.col = self.header_col
        apply_color(c.header.row_bg, self.header_row_bg)
        apply_color(c.header.col_bg, self.header_col_bg)
        apply_style(c.header.style, self.header_style)

        c.striped = self.striped
        apply_color(c.stripe_bg, self.stripe_bg)

        apply_color(c.footer.row_bg, self.footer_row_bg)
        apply_color(c.footer.col_bg, self.footer_col_bg)
        apply_style(c.footer.style, self.footer_style)

        if self.title is not None:
            title = self.title if isinstance(self.title, Title) else Title(text=self.title)
            apply_title(c.title, title, arena)

        apply_edges(c.cell_pad, self.cell_pad)
        apply_edges(c.margin, self.margin)
        c.total_width = self.total_width
        c.max_rows = self.max_rows
        c.right_to_left = self.rtl


def _opts_to_c(opts: TableOpts, arena: list):
    c = ffi.new("ScTableOpts *")
    opts._fill(c, arena)
    return c


class Table:
    """A table built column-by-column then printed (or captured) with options.

    The same table can be printed repeatedly with different :class:`TableOpts`.
    Cell strings and rich :class:`~sparcli.text.Text` are borrowed by the C side
    until rendering, so the table keeps them alive internally for you.
    """

    __slots__ = ("_p", "_finalizer", "_arena", "_texts", "__weakref__")

    def __init__(self) -> None:
        p = lib.sc_table_new()
        if p == ffi.NULL:
            raise MemoryError("sc_table_new failed")
        self._p = p
        self._arena: list = []   # cstr buffers borrowed by the C table
        self._texts: list = []   # Text objects borrowed by the C table
        self._finalizer = weakref.finalize(self, lib.sc_table_free, p)

    def column(self, header: str | None = None, opts: ColOpts = ColOpts()) -> "Table":
        """Append a column. Returns ``self`` for chaining."""
        c = ffi.new("ScColOpts *")
        opts._fill(c)
        local: list = []
        lib.sc_table_add_column(self._p, cstr(local, header), c[0])
        return self

    def _build_row(self, cells):
        coerced = [_coerce_cell(x) for x in cells]
        arr = ffi.new("ScCell[]", len(coerced))
        for i, cell in enumerate(coerced):
            self._fill_cell(arr[i], cell)
        return arr, len(coerced)

    def _fill_cell(self, elem, cell: Cell) -> None:
        if cell.kind == "markup":
            elem[0] = lib.sc_cell_from_markup(cstr([], str(cell.value)))
            # transient buffer is fine: sc_cell_from_markup parses immediately.
        elif cell.kind == "text":
            elem.kind = _KIND_TEXT
            elem.text = cell.value._ptr
            self._texts.append(cell.value)
        elif cell.kind == "skip":
            elem.kind = _KIND_STR
            elem.str = cstr(self._arena, "")
            elem.col_span = -1
            return
        elif cell.kind == "row_skip":
            elem.kind = _KIND_STR
            elem.str = cstr(self._arena, "")
            elem.row_span = -1
            return
        else:  # "str"
            elem.kind = _KIND_STR
            elem.str = cstr(self._arena, "" if cell.value is None else str(cell.value))
        if cell.halign is not None:
            elem.halign_set = True
            elem.halign = int(cell.halign)
        if cell.valign is not None:
            elem.valign_set = True
            elem.valign = int(cell.valign)
        if cell.colspan:
            elem.col_span = cell.colspan
        if cell.rowspan:
            elem.row_span = cell.rowspan

    def row(self, cells, bg: Color | None = None) -> "Table":
        """Append a data row. ``cells`` is a sequence of str / Text / :class:`Cell`."""
        arr, n = self._build_row(cells)
        if bg is None:
            lib.sc_table_add_row(self._p, arr, n)
        else:
            cbg = ffi.new("ScColor *")
            apply_color(cbg, bg)
            lib.sc_table_add_row_bg(self._p, arr, n, cbg[0])
        return self

    def footer_row(self, cells) -> "Table":
        """Append a footer row."""
        arr, n = self._build_row(cells)
        lib.sc_table_add_footer_row(self._p, arr, n)
        return self

    def print(self, opts: TableOpts = TableOpts()) -> None:
        """Render the table to the current output stream."""
        arena: list = []
        c = _opts_to_c(opts, arena)
        lib.sc_table_print(self._p, c[0])

    def close(self) -> None:
        self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Table":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
