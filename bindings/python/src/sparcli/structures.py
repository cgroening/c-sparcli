"""Lists, trees and key/value blocks."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field

from ._ffi import (apply_color, apply_edges, apply_style, cstr, ffi, lib)
from .color import Color
from .enums import AnsiMode, BorderType, ListMarker
from .style import Edges, Style
from .text import Text


# ── List ─────────────────────────────────────────────────────────────────────
@dataclass
class ListOpts:
    """Rendering options for a :class:`List`."""

    marker: ListMarker = ListMarker.BULLET
    bullet: str | None = None
    marker_prefix: str | None = None
    marker_suffix: str | None = None
    marker_style: Style = field(default_factory=Style)
    indent: int = 0
    item_gap: int = 0
    width: int = 0
    margin: Edges = field(default_factory=Edges)
    ansi: AnsiMode = AnsiMode.DEFAULT

    def _fill(self, c, arena: list) -> None:
        c.marker = int(self.marker)
        c.bullet = cstr(arena, self.bullet)
        c.marker_prefix = cstr(arena, self.marker_prefix)
        c.marker_suffix = cstr(arena, self.marker_suffix)
        apply_style(c.marker_style, self.marker_style)
        c.indent = self.indent
        c.item_gap = self.item_gap
        c.width = self.width
        apply_edges(c.margin, self.margin)
        c.ansi = int(self.ansi)


class ListItem:
    """An item returned by :meth:`List.add`; can carry a nested sub-list."""

    __slots__ = ("_p", "_owner")

    def __init__(self, p, owner: "List") -> None:
        self._p = p
        self._owner = owner

    def sub(self, opts: ListOpts = ListOpts()) -> "List":
        """Attach and return a sub-list (owned/freed by the root list)."""
        arena: list = []
        c = ffi.new("ScListOpts *")
        opts._fill(c, arena)
        p = lib.sc_list_add_sub(self._p, c[0])
        sub = List._wrap(p, arena)
        self._owner._children.append(sub)  # keep arena/refs alive
        return sub


class List:
    """A bulleted / numbered list with nesting and word-wrap."""

    __slots__ = ("_p", "_finalizer", "_arena", "_children", "__weakref__")

    def __init__(self, opts: ListOpts = ListOpts()) -> None:
        arena: list = []
        c = ffi.new("ScListOpts *")
        opts._fill(c, arena)
        p = lib.sc_list_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_list_new failed")
        self._p = p
        self._arena = arena
        self._children: list = []
        self._finalizer = weakref.finalize(self, lib.sc_list_free, p)

    @classmethod
    def _wrap(cls, p, arena: list) -> "List":
        """Wrap a sub-list pointer owned by a parent (no finalizer)."""
        self = cls.__new__(cls)
        self._p = p
        self._arena = arena
        self._children = []
        self._finalizer = None
        return self

    def add(self, value: "str | Text", style: Style = Style()) -> ListItem:
        """Append an item (str or :class:`~sparcli.text.Text`)."""
        if isinstance(value, Text):
            p = lib.sc_list_add_text(self._p, value._ptr)
            self._children.append(value)
        else:
            cstyle = ffi.new("ScTextStyle *")
            apply_style(cstyle, style)
            p = lib.sc_list_add_str(
                self._p, cstr(self._arena, value), cstyle[0])
        return ListItem(p, self)

    def print(self) -> None:
        lib.sc_list_print(self._p)

    def close(self) -> None:
        if self._finalizer is not None:
            self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "List":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── Tree ─────────────────────────────────────────────────────────────────────
@dataclass
class TreeOpts:
    """Rendering options for a :class:`Tree`."""

    type: BorderType = BorderType.SINGLE
    connector_color: Color = Color.NONE
    indent: int = 1
    no_guide: bool = False
    ansi: AnsiMode = AnsiMode.DEFAULT

    def _fill(self, c) -> None:
        c.type = int(self.type)
        apply_color(c.connector_color, self.connector_color)
        c.indent = self.indent
        c.no_guide = self.no_guide
        c.ansi = int(self.ansi)


class TreeNode:
    """A node returned by :meth:`Tree.add`; pass it as ``parent`` to nest."""

    __slots__ = ("_p",)

    def __init__(self, p) -> None:
        self._p = p


class Tree:
    """A tree layout with box-drawing connectors."""

    __slots__ = ("_p", "_finalizer", "_arena", "_texts", "__weakref__")

    def __init__(self, opts: TreeOpts = TreeOpts()) -> None:
        c = ffi.new("ScTreeOpts *")
        opts._fill(c)
        p = lib.sc_tree_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_tree_new failed")
        self._p = p
        self._arena: list = []
        self._texts: list = []
        self._finalizer = weakref.finalize(self, lib.sc_tree_free, p)

    def add(self, value: "str | Text", parent: TreeNode | None = None,
            style: Style = Style(), prefix: str | None = None,
            prefix_style: Style = Style()) -> TreeNode:
        """Add a node under ``parent`` (``None`` = new root)."""
        par = parent._p if parent is not None else ffi.NULL
        ps = ffi.new("ScTextStyle *")
        apply_style(ps, prefix_style)
        if isinstance(value, Text):
            self._texts.append(value)
            p = lib.sc_tree_add_text(self._p, par, value._ptr,
                                     cstr(self._arena, prefix), ps[0])
        else:
            st = ffi.new("ScTextStyle *")
            apply_style(st, style)
            p = lib.sc_tree_add_str(
                self._p, par, cstr(self._arena, value), st[0],
                cstr(self._arena, prefix), ps[0])
        return TreeNode(p)

    def print(self) -> None:
        lib.sc_tree_print(self._p)

    def close(self) -> None:
        self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Tree":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── Key/Value ────────────────────────────────────────────────────────────────
@dataclass
class KvOpts:
    """Rendering options for a :class:`Kv` block."""

    sep: str | None = None
    key_width: int = 0
    width: int = 0
    margin: Edges = field(default_factory=Edges)
    item_gap: int = 0
    wrap_val: bool = False
    key_style: Style = field(default_factory=Style)
    val_style: Style = field(default_factory=Style)
    ansi: AnsiMode = AnsiMode.DEFAULT

    def _fill(self, c, arena: list) -> None:
        c.sep = cstr(arena, self.sep)
        c.key_width = self.key_width
        c.width = self.width
        apply_edges(c.margin, self.margin)
        c.item_gap = self.item_gap
        c.wrap_val = self.wrap_val
        apply_style(c.key_style, self.key_style)
        apply_style(c.val_style, self.val_style)
        c.ansi = int(self.ansi)


class Kv:
    """A key/value list with aligned keys and optional value wrapping."""

    __slots__ = ("_p", "_finalizer", "_arena", "__weakref__")

    def __init__(self, opts: KvOpts = KvOpts()) -> None:
        arena: list = []
        c = ffi.new("ScKVOpts *")
        opts._fill(c, arena)
        p = lib.sc_kv_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_kv_new failed")
        self._p = p
        self._arena = arena
        self._finalizer = weakref.finalize(self, lib.sc_kv_free, p)

    def add(self, key: str, value: str) -> "Kv":
        """Append a ``key`` → ``value`` pair. Returns ``self``."""
        local: list = []
        lib.sc_kv_add(self._p, cstr(local, key), cstr(local, value))
        return self

    def print(self) -> None:
        lib.sc_kv_print(self._p)

    def close(self) -> None:
        self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Kv":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
