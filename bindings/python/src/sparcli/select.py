"""Selection list (:class:`Select`) and fuzzy finder (:class:`Fuzzy`)."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field
from enum import IntEnum

from ._ffi import apply_box, apply_color, apply_style, cstr, ffi, lib
from ._inputcommon import (fill_hint, fill_prompt_text, fill_shortcuts, result)
from .color import Color
from .enums import HintLayout, HintPos
from .keys import KeyChord, Shortcuts
from .style import BoxStyle, Style
from .table import TableOpts
from .text import Text


class FuzzyOrder(IntEnum):
    """Result ordering for a :class:`Fuzzy` finder (within sections if any)."""

    SCORE = 0       #: match score (default)
    INSERTION = 1   #: stable add order (e.g. tasks by time)
    COLUMN = 2      #: by a chosen column (case-insensitive)


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
    no_cycle: bool = False
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
        c.no_cycle = self.no_cycle
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
    no_cycle: bool = False
    accent: Color = Color.NONE
    table: bool = False
    headers: list[str] | None = None
    search_columns: int = 0
    stretch_columns: int = 0  #: table cols that fill a bounded box width (mask)
    max_height: int = 0       #: cap finder height in rows (scrolls); 0 = auto-fit
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
    multi: bool = False
    checkbox_on: str | None = None
    checkbox_off: str | None = None
    checkbox_column: bool = False
    toggle_all_key: KeyChord | None = None
    toggle_section_key: KeyChord | None = None
    section_style: Style = field(default_factory=Style)
    section_counts: bool = False
    disabled_style: Style = field(default_factory=Style)
    empty_text: str | None = None
    empty_style: Style = field(default_factory=Style)
    order: FuzzyOrder = FuzzyOrder.SCORE
    order_column: int = 0
    order_desc: bool = False
    initial_query: str | None = None
    modal: bool = False
    start_in_insert: bool = False
    insert_key: KeyChord | None = None
    normal_key: KeyChord | None = None
    clear_key: KeyChord | None = None
    hide_mode_badge: bool = False
    normal_label: str | None = None
    insert_label: str | None = None
    mode_normal_style: Style = field(default_factory=Style)
    mode_insert_style: Style = field(default_factory=Style)

    def _fill(self, c, arena: list) -> None:
        c.prompt = cstr(arena, self.prompt)
        c.max_visible = self.max_visible
        c.no_cycle = self.no_cycle
        apply_color(c.accent, self.accent)
        c.table = self.table
        if self.headers:
            bufs = [cstr(arena, h) for h in self.headers]
            arr = ffi.new("char *[]", bufs)
            arena.append(arr)
            c.headers = ffi.cast("const char *const *", arr)
            c.n_cols = len(bufs)
        c.search_columns = self.search_columns
        c.stretch_columns = self.stretch_columns
        c.max_height = self.max_height
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
        c.multi = self.multi
        c.checkbox_on = cstr(arena, self.checkbox_on)
        c.checkbox_off = cstr(arena, self.checkbox_off)
        c.checkbox_column = self.checkbox_column
        if self.toggle_all_key is not None:
            c.toggle_all_key = self.toggle_all_key.value
        if self.toggle_section_key is not None:
            c.toggle_section_key = self.toggle_section_key.value
        apply_style(c.section_style, self.section_style)
        c.section_counts = self.section_counts
        apply_style(c.disabled_style, self.disabled_style)
        c.empty_text = cstr(arena, self.empty_text)
        apply_style(c.empty_style, self.empty_style)
        c.order = int(self.order)
        c.order_column = self.order_column
        c.order_desc = self.order_desc
        c.initial_query = cstr(arena, self.initial_query)
        c.modal = self.modal
        c.start_in_insert = self.start_in_insert
        if self.insert_key is not None:
            c.insert_key = self.insert_key.value
        if self.normal_key is not None:
            c.normal_key = self.normal_key.value
        if self.clear_key is not None:
            c.clear_key = self.clear_key.value
        c.hide_mode_badge = self.hide_mode_badge
        c.normal_label = cstr(arena, self.normal_label)
        c.insert_label = cstr(arena, self.insert_label)
        apply_style(c.mode_normal_style, self.mode_normal_style)
        apply_style(c.mode_insert_style, self.mode_insert_style)


class Fuzzy:
    """An incremental fuzzy finder, list or table view."""

    __slots__ = ("_p", "_finalizer", "_keepalive", "_count", "__weakref__")

    def __init__(self, opts: FuzzyOpts = FuzzyOpts()) -> None:
        arena: list = []
        c = ffi.new("ScFuzzyOpts *")
        opts._fill(c, arena)
        p = lib.sc_fuzzy_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_fuzzy_new failed")
        self._p = p
        self._count = 0
        # The C side borrows `shortcuts` / `prompt_text` / table_opts strings
        # for its lifetime; keep the opts (and the cffi buffers built from
        # them) alive with it.
        self._keepalive = (opts, arena)
        self._finalizer = weakref.finalize(self, lib.sc_fuzzy_free, p)

    def add(self, label: str) -> "Fuzzy":
        """Add a single-field item. Returns ``self``."""
        local: list = []
        lib.sc_fuzzy_add(self._p, cstr(local, label))
        self._count += 1
        return self

    def add_row(self, fields: list[str], styles: list[Style] | None = None
                ) -> "Fuzzy":
        """Add a multi-field row (table view); ``fields[0]`` is matched. With
        ``styles`` each cell gets a base text style (padded to the field
        count)."""
        local: list = []
        bufs = [cstr(local, f) for f in fields]
        arr = ffi.new("char *[]", bufs)
        cfields = ffi.cast("const char *const *", arr)
        if styles is None:
            lib.sc_fuzzy_add_row(self._p, cfields, len(bufs))
        else:
            st = ffi.new("ScTextStyle[]", len(bufs))
            for i in range(len(bufs)):
                if i < len(styles):
                    apply_style(st[i], styles[i])
            lib.sc_fuzzy_add_row_styled(self._p, cfields, st, len(bufs))
        self._count += 1
        return self

    def add_section(self, title: str) -> "Fuzzy":
        """Add a non-selectable section header (e.g. a day). Returns ``self``."""
        local: list = []
        lib.sc_fuzzy_add_section(self._p, cstr(local, title))
        self._count += 1
        return self

    def add_styled(self, label: str, style: Style) -> "Fuzzy":
        """Add a single item with a base text style. Returns ``self``."""
        local: list = []
        st = ffi.new("ScTextStyle *")
        apply_style(st, style)
        lib.sc_fuzzy_add_styled(self._p, cstr(local, label), st[0])
        self._count += 1
        return self

    def add_row_rich(self, cells: list[Text]) -> "Fuzzy":
        """Add a row of rich :class:`Text` cells (deep-copied). The match key
        is each cell's flattened text. Returns ``self``."""
        arr = ffi.new("ScText *[]", [c._ptr for c in cells])
        lib.sc_fuzzy_add_row_rich(
            self._p, ffi.cast("ScText *const *", arr), len(cells))
        self._count += 1
        return self

    def set_disabled(self, index: int, on: bool = True) -> None:
        """Grey out the row at ``index`` (add order) and make it unselectable."""
        lib.sc_fuzzy_set_disabled(self._p, index, bool(on))

    def set_id(self, index: int, id: int) -> None:
        """Set a stable caller id on the row at ``index`` (add order)."""
        lib.sc_fuzzy_set_id(self._p, index, id)

    def id_at(self, index: int) -> int:
        """Stable id of the row at ``index`` (add order), or 0."""
        return int(lib.sc_fuzzy_id_at(self._p, index))

    def cursor_id(self) -> int:
        """Stable id of the currently highlighted row, or 0."""
        return int(lib.sc_fuzzy_cursor_id(self._p))

    def set_checked(self, index: int, on: bool = True) -> None:
        """Pre-check/uncheck the row at ``index`` (multi-select)."""
        lib.sc_fuzzy_set_checked(self._p, index, bool(on))

    def is_checked(self, index: int) -> bool:
        """Whether the row at ``index`` (add order) is checked."""
        return bool(lib.sc_fuzzy_is_checked(self._p, index))

    def check_all(self, on: bool = True) -> None:
        """Check or uncheck every selectable row."""
        lib.sc_fuzzy_check_all(self._p, bool(on))

    def checked_count(self) -> int:
        """Number of checked rows."""
        return int(lib.sc_fuzzy_checked_count(self._p))

    def set_cursor(self, index: int) -> None:
        """Pre-position the cursor on ``index`` (add order, clamped)."""
        lib.sc_fuzzy_set_cursor(self._p, index)

    def label(self, index: int) -> str | None:
        """First field of the row at ``index`` (add order), or ``None``."""
        p = lib.sc_fuzzy_label(self._p, index)
        return None if p == ffi.NULL else ffi.string(p).decode()

    def set_label(self, index: int, label: str) -> None:
        """Replace the first field of the row at ``index`` (add order)."""
        local: list = []
        lib.sc_fuzzy_set_label(self._p, index, cstr(local, label))

    def set_row(self, index: int, fields: list[str]) -> None:
        """Replace all fields of the row at ``index`` (add order)."""
        local: list = []
        bufs = [cstr(local, f) for f in fields]
        arr = ffi.new("char *[]", bufs)
        lib.sc_fuzzy_set_row(
            self._p, index, ffi.cast("const char *const *", arr), len(bufs))

    def set_row_style(self, index: int, col: int, style: Style) -> None:
        """Set the base style of cell ``col`` in the row at ``index``."""
        st = ffi.new("ScTextStyle *")
        apply_style(st, style)
        lib.sc_fuzzy_set_row_style(self._p, index, col, st[0])

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
        if self._count:
            self._count -= 1

    def refresh(self) -> None:
        """Re-apply the query after appending rows mid-run (no-op otherwise)."""
        lib.sc_fuzzy_refresh(self._p)

    def run(self) -> int | None:
        """Run the finder; returns the chosen item's add-order index."""
        out = ffi.new("size_t *")
        st = lib.sc_fuzzy_run(self._p, out)
        return result(st, lambda: int(out[0]))

    def run_multi(self) -> list[int] | None:
        """Run multi-select; returns the checked add-order indices (``[]`` when
        nothing was checked), or ``None`` on cancel."""
        cap = max(1, self._count)
        indices = ffi.new("size_t[]", cap)
        count = ffi.new("size_t *", cap)
        st = lib.sc_fuzzy_run_multi(self._p, indices, count)
        return result(st, lambda: [int(indices[i]) for i in range(count[0])])

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "Fuzzy":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
