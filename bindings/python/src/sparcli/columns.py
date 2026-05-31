"""Side-by-side multi-column layout (:class:`Columns`)."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field

from ._ffi import (apply_border, apply_color, apply_edges, cstr, ffi, lib)
from .color import Color
from .enums import Align, VAlign
from .output import PanelOpts
from .style import BorderStyle, Edges
from .table import Table, TableOpts, _opts_to_c
from .text import Text


@dataclass
class ColItem:
    """Per-column options passed with each ``add_*`` call."""

    min_w: int = 0
    max_w: int = 0
    fixed_w: int = 0
    halign: Align = Align.LEFT
    valign: VAlign | None = None  # None = inherit the columns-wide valign
    bg: Color = Color.NONE
    stretch: bool = False

    def _to_c(self):
        c = ffi.new("ScColItem *")
        c.min_w = self.min_w
        c.max_w = self.max_w
        c.fixed_w = self.fixed_w
        c.halign = int(self.halign)
        if self.valign is not None:
            c.valign_set = True
            c.valign = int(self.valign)
        apply_color(c.bg, self.bg)
        c.stretch = self.stretch
        return c


@dataclass
class ColumnsOpts:
    """Layout options for a :class:`Columns` container."""

    gap: int = 0
    sep: BorderStyle = field(default_factory=BorderStyle)
    valign: VAlign = VAlign.TOP
    total_width: int = 0
    margin: Edges = field(default_factory=Edges)

    def _fill(self, c) -> None:
        c.gap = self.gap
        apply_border(c.sep, self.sep)
        c.valign = int(self.valign)
        c.total_width = self.total_width
        apply_edges(c.margin, self.margin)


class Columns:
    """Renders captured widgets side by side. Each ``add_*`` snapshots its
    argument at call time, so later mutations of the source have no effect."""

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, opts: ColumnsOpts = ColumnsOpts()) -> None:
        c = ffi.new("ScColumnsOpts *")
        opts._fill(c)
        p = lib.sc_columns_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_columns_new failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_columns_free, p)

    def add_str(self, s: str, item: ColItem = ColItem()) -> "Columns":
        arena: list = []
        lib.sc_columns_add_str(self._p, cstr(arena, s), item._to_c()[0])
        return self

    def add_text(self, t: Text, item: ColItem = ColItem()) -> "Columns":
        lib.sc_columns_add_text(self._p, t._ptr, item._to_c()[0])
        return self

    def add_table(self, table: Table, opts: TableOpts = TableOpts(),
                  item: ColItem = ColItem()) -> "Columns":
        arena: list = []
        c = _opts_to_c(opts, arena)
        lib.sc_columns_add_table(self._p, table._ptr, c[0], item._to_c()[0])
        return self

    def add_panel(self, content: "str | Text", opts: PanelOpts = PanelOpts(),
                  item: ColItem = ColItem()) -> "Columns":
        arena: list = []
        c = ffi.new("ScPanelOpts *")
        opts._fill(c, arena)
        ci = item._to_c()[0]
        if isinstance(content, Text):
            lib.sc_columns_add_panel_text(self._p, content._ptr, c[0], ci)
        else:
            lib.sc_columns_add_panel_str(
                self._p, cstr(arena, content), c[0], ci)
        return self

    def add_list(self, lst, item: ColItem = ColItem()) -> "Columns":
        lib.sc_columns_add_list(self._p, lst._ptr, item._to_c()[0])
        return self

    def add_tree(self, tr, item: ColItem = ColItem()) -> "Columns":
        lib.sc_columns_add_tree(self._p, tr._ptr, item._to_c()[0])
        return self

    def add_columns(self, nested: "Columns",
                    item: ColItem = ColItem()) -> "Columns":
        lib.sc_columns_add_columns(self._p, nested._ptr, item._to_c()[0])
        return self

    def add_rendered(self, rendered, item: ColItem = ColItem()) -> "Columns":
        lib.sc_columns_add_rendered(self._p, rendered._ptr, item._to_c()[0])
        return self

    def print(self) -> None:
        lib.sc_columns_print(self._p)

    def close(self) -> None:
        self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Columns":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
