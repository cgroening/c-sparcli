"""Selection list (:class:`Select`) and fuzzy finder (:class:`Fuzzy`)."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field

from ._ffi import apply_box, apply_color, apply_style, cstr, ffi, lib
from ._inputcommon import (fill_hint, fill_prompt_text, fill_shortcuts, result)
from .color import Color
from .enums import HintLayout, HintPos
from .keys import Shortcuts
from .style import BoxStyle, Style
from .table import TableOpts
from .text import Text


def fuzzy_match(pattern: str, s: str) -> tuple[bool, int]:
    """Pure subsequence match -> ``(matched, score)``; higher is better."""
    arena: list = []
    score = ffi.new("int *")
    ok = lib.sc_fuzzy_match(cstr(arena, pattern), cstr(arena, s), score)
    return bool(ok), int(score[0])


# ── Select ───────────────────────────────────────────────────────────────────
@dataclass
class SelectOpts:
    """Options for a :class:`Select`."""

    prompt: str | None = None
    multi: bool = False
    max_visible: int = 0
    prompt_style: Style = field(default_factory=Style)
    accent: Color = Color.NONE
    selected_style: Style = field(default_factory=Style)
    cursor_marker: str | None = None
    marker: str | None = None
    checkbox_on: str | None = None
    checkbox_off: str | None = None
    box: BoxStyle = field(default_factory=BoxStyle)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False

    def _fill(self, c, arena: list) -> None:
        c.prompt = cstr(arena, self.prompt)
        c.multi = self.multi
        c.max_visible = self.max_visible
        apply_style(c.prompt_style, self.prompt_style)
        apply_color(c.accent, self.accent)
        apply_style(c.selected_style, self.selected_style)
        c.cursor_marker = cstr(arena, self.cursor_marker)
        c.marker = cstr(arena, self.marker)
        c.checkbox_on = cstr(arena, self.checkbox_on)
        c.checkbox_off = cstr(arena, self.checkbox_off)
        apply_box(c.box, self.box)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


class Select:
    """A scrolling single- or multi-choice list (``opts.multi = True``)."""

    __slots__ = ("_p", "_finalizer", "_n", "_keepalive", "__weakref__")

    def __init__(self, opts: SelectOpts = SelectOpts()) -> None:
        arena: list = []
        c = ffi.new("ScSelectOpts *")
        opts._fill(c, arena)
        p = lib.sc_select_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_select_new failed")
        self._p = p
        self._n = 0
        # The C side borrows `shortcuts` / `prompt_text` for its lifetime;
        # keep the opts (and the cffi buffers built from them) alive with it.
        self._keepalive = (opts, arena)
        self._finalizer = weakref.finalize(self, lib.sc_select_free, p)

    def add(self, label: str) -> "Select":
        """Append a selectable item. Returns ``self``."""
        local: list = []
        lib.sc_select_add(self._p, cstr(local, label))
        self._n += 1
        return self

    def set_cursor(self, index: int) -> None:
        lib.sc_select_set_cursor(self._p, index)

    def set_checked(self, index: int, on: bool = True) -> None:
        lib.sc_select_set_checked(self._p, index, bool(on))

    def cursor(self) -> int:
        """Index of the currently highlighted item (e.g. from a callback)."""
        return int(lib.sc_select_cursor(self._p))

    def label(self, index: int) -> str | None:
        """Current label of item ``index`` (current order), or ``None``."""
        p = lib.sc_select_label(self._p, index)
        return None if p == ffi.NULL else ffi.string(p).decode()

    def set_label(self, index: int, label: str) -> None:
        local: list = []
        lib.sc_select_set_label(self._p, index, cstr(local, label))

    def remove(self, index: int) -> None:
        lib.sc_select_remove(self._p, index)
        if self._n:
            self._n -= 1

    def run(self) -> list[int] | None:
        """Run multi-select; returns the chosen indices (display order)."""
        cap = max(1, self._n)
        indices = ffi.new("size_t[]", cap)
        count = ffi.new("size_t *", cap)
        st = lib.sc_select_run(self._p, indices, count)
        return result(st, lambda: [int(indices[i]) for i in range(count[0])])

    def run_one(self) -> int | None:
        """Run single-select; returns the chosen index, or ``None``."""
        idx = self.run()
        return idx[0] if idx else None

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "Select":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


# ── Fuzzy ────────────────────────────────────────────────────────────────────
@dataclass
class FuzzyOpts:
    """Options for a :class:`Fuzzy` finder. Set ``table=True`` with ``headers``
    for a table view."""

    prompt: str | None = None
    max_visible: int = 0
    accent: Color = Color.NONE
    table: bool = False
    headers: list[str] | None = None
    search_columns: int = 0
    prompt_style: Style = field(default_factory=Style)
    selected_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    counter_style: Style = field(default_factory=Style)
    cursor_marker: str | None = None
    marker: str | None = None
    box: BoxStyle = field(default_factory=BoxStyle)
    table_opts: TableOpts = field(default_factory=TableOpts)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False

    def _fill(self, c, arena: list) -> None:
        c.prompt = cstr(arena, self.prompt)
        c.max_visible = self.max_visible
        apply_color(c.accent, self.accent)
        c.table = self.table
        if self.headers:
            bufs = [cstr(arena, h) for h in self.headers]
            arr = ffi.new("char *[]", bufs)
            arena.append(arr)
            c.headers = ffi.cast("const char *const *", arr)
            c.n_cols = len(bufs)
        c.search_columns = self.search_columns
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.selected_style, self.selected_style)
        apply_style(c.cursor_style, self.cursor_style)
        apply_style(c.counter_style, self.counter_style)
        c.cursor_marker = cstr(arena, self.cursor_marker)
        c.marker = cstr(arena, self.marker)
        apply_box(c.box, self.box)
        self.table_opts._fill(c.table_opts, arena)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


class Fuzzy:
    """An incremental fuzzy finder, list or table view."""

    __slots__ = ("_p", "_finalizer", "_keepalive", "__weakref__")

    def __init__(self, opts: FuzzyOpts = FuzzyOpts()) -> None:
        arena: list = []
        c = ffi.new("ScFuzzyOpts *")
        opts._fill(c, arena)
        p = lib.sc_fuzzy_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_fuzzy_new failed")
        self._p = p
        # The C side borrows `shortcuts` / `prompt_text` / table_opts strings
        # for its lifetime; keep the opts (and the cffi buffers built from
        # them) alive with it.
        self._keepalive = (opts, arena)
        self._finalizer = weakref.finalize(self, lib.sc_fuzzy_free, p)

    def add(self, label: str) -> "Fuzzy":
        """Add a single-field item. Returns ``self``."""
        local: list = []
        lib.sc_fuzzy_add(self._p, cstr(local, label))
        return self

    def add_row(self, fields: list[str]) -> "Fuzzy":
        """Add a multi-field row (table view); ``fields[0]`` is matched."""
        local: list = []
        bufs = [cstr(local, f) for f in fields]
        arr = ffi.new("char *[]", bufs)
        lib.sc_fuzzy_add_row(
            self._p, ffi.cast("const char *const *", arr), len(bufs))
        return self

    def cursor_index(self) -> int:
        """Add-order index of the highlighted row (e.g. from a callback)."""
        return int(lib.sc_fuzzy_cursor_index(self._p))

    def has_selection(self) -> bool:
        """Whether a row matches the current query (the selection is valid).

        Unlike :meth:`cursor_index`, this tells "no match" apart from "row 0" -
        useful in a forward/submit shortcut callback.
        """
        return bool(lib.sc_fuzzy_has_selection(self._p))

    def remove(self, index: int) -> None:
        lib.sc_fuzzy_remove(self._p, index)

    def run(self) -> int | None:
        """Run the finder; returns the chosen item's add-order index."""
        out = ffi.new("size_t *")
        st = lib.sc_fuzzy_run(self._p, out)
        return result(st, lambda: int(out[0]))

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "Fuzzy":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
