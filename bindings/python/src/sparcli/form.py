"""Grid-layout form (:class:`Form`): framed fields, 2D navigation, edit below.

Add fields row by row (:meth:`Form.row_begin`, ``add_*``, :meth:`Form.add_skip`),
:meth:`Form.run` it, then read values back with the ``get_*`` methods. The C
``validate`` callback is intentionally not exposed.
"""

from __future__ import annotations

import datetime
import weakref
from dataclasses import dataclass, field

from ._ffi import apply_color, apply_style, cstr, ffi, lib
from ._inputcommon import fill_hint, fill_shortcuts
from .color import Color
from .enums import FieldWidthMode, HintLayout, HintPos, VAlign, ValignScope
from .keys import KeyChord, Shortcuts
from .style import Style


@dataclass
class FieldOpts:
    """Per-field layout and behaviour (all optional)."""

    width_mode: FieldWidthMode = FieldWidthMode.AUTO
    width: int = 0            #: percent (PCT) or column count (FIXED)
    col_span: int = 0         #: columns spanned (0 = 1)
    row_span: int = 0         #: rows spanned (0 = 1)
    height: int = 0           #: content lines in the box (0 = 1)
    #: fullscreen only: grow this field's row to fill the remaining terminal
    #: height (``height`` is the minimum; the first such field wins)
    fill_height: bool = False
    required: bool = False
    multiline: bool = False   #: text field edited via the external editor
    date_optional: bool = False  #: date field may be empty (get_date -> None)
    help: str | None = None

    def _fill(self, c, arena: list) -> None:
        c.width_mode = int(self.width_mode)
        c.width = self.width
        c.col_span = self.col_span
        c.row_span = self.row_span
        c.height = self.height
        c.fill_height = self.fill_height
        c.required = self.required
        c.multiline = self.multiline
        c.date_optional = self.date_optional
        c.help = cstr(arena, self.help)


@dataclass
class FormOpts:
    """Options for a :class:`Form`."""

    title: str | None = None
    title_style: Style = field(default_factory=Style)
    accent: Color = Color.NONE
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    no_cycle: bool = False             #: stop at grid edges (default: arrows wrap)
    autoedit: bool = False             #: open the initial field's editor at start
    shortcuts: Shortcuts | None = None
    editor: str | None = None          #: external editor for multiline fields
    editor_key: KeyChord | None = None  #: opens the editor (None = Ctrl-G)
    #: extension for the editor's temp file (e.g. ``".md"``); None/empty = none
    editor_suffix: str | None = None
    edit_bg: Color = Color.NONE        #: editor-box background (default: gray)
    fullscreen: bool = False  #: fill the terminal (header + valign; use altscreen)
    valign: VAlign = VAlign.TOP  #: block alignment in fullscreen
    #: what ``valign`` applies to (fullscreen): ALL = whole block, CONTENT =
    #: pin header top / footer bottom and align only the grid in between
    valign_scope: ValignScope = ValignScope.ALL
    header: "Rendered | None" = None  #: pinned header (fullscreen); borrowed
    modified_marker: str | None = None  #: prefix on a changed field's title

    def _fill(self, c, arena: list) -> None:
        c.title = cstr(arena, self.title)
        apply_style(c.title_style, self.title_style)
        apply_color(c.accent, self.accent)
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        c.no_cycle = self.no_cycle
        c.autoedit = self.autoedit
        fill_shortcuts(c, self.shortcuts, arena)
        c.editor = cstr(arena, self.editor)
        if self.editor_key is not None:
            c.editor_key = self.editor_key.value
        c.editor_suffix = cstr(arena, self.editor_suffix)
        apply_color(c.edit_bg, self.edit_bg)
        c.fullscreen = self.fullscreen
        c.valign = int(self.valign)
        c.valign_scope = int(self.valign_scope)
        c.header = self.header._ptr if self.header is not None else ffi.NULL
        c.modified_marker = cstr(arena, self.modified_marker)


class Form:
    """A grid-layout form built field by field; read values after :meth:`run`."""

    __slots__ = ("_p", "_finalizer", "_keepalive", "__weakref__")

    def __init__(self, opts: FormOpts = FormOpts()) -> None:
        arena: list = []
        c = ffi.new("ScFormOpts *")
        opts._fill(c, arena)
        p = lib.sc_form_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_form_new failed")
        self._p = p
        # The C side borrows `shortcuts` for the run; keep opts + buffers alive.
        self._keepalive = (opts, arena)
        self._finalizer = weakref.finalize(self, lib.sc_form_free, p)

    # ── construction ──────────────────────────────────────────────────────
    def row_begin(self) -> "Form":
        """Start a new grid row. Returns ``self``."""
        lib.sc_form_row_begin(self._p)
        return self

    def add_skip(self) -> "Form":
        """Placeholder under a col/row-spanning field. Returns ``self``."""
        lib.sc_form_add_skip(self._p)
        return self

    def _field(self, opts: FieldOpts | None, arena: list):
        c = ffi.new("ScFieldOpts *")
        (opts or FieldOpts())._fill(c, arena)
        return c

    def add_text(self, label: str, initial: str = "",
                 opts: FieldOpts | None = None) -> int:
        """Add a text field; returns its index."""
        a: list = []
        c = self._field(opts, a)
        return int(lib.sc_form_add_text(
            self._p, cstr(a, label), cstr(a, initial), c[0]))

    def add_number(self, label: str, initial: float = 0.0,
                   opts: FieldOpts | None = None) -> int:
        """Add a number field; returns its index."""
        a: list = []
        c = self._field(opts, a)
        return int(lib.sc_form_add_number(
            self._p, cstr(a, label), float(initial), c[0]))

    def add_bool(self, label: str, initial: bool = False,
                 opts: FieldOpts | None = None) -> int:
        """Add a bool field; returns its index."""
        a: list = []
        c = self._field(opts, a)
        return int(lib.sc_form_add_bool(
            self._p, cstr(a, label), bool(initial), c[0]))

    def add_select(self, label: str, choices: list[str], initial: int = 0,
                   opts: FieldOpts | None = None) -> int:
        """Add a single-choice field; returns its index."""
        a: list = []
        c = self._field(opts, a)
        bufs = [cstr(a, s) for s in choices]
        arr = ffi.new("char *[]", bufs)
        a.append(arr)
        return int(lib.sc_form_add_select(
            self._p, cstr(a, label),
            ffi.cast("const char *const *", arr), len(bufs), initial, c[0]))

    def add_multiselect(self, label: str, choices: list[str],
                        checked: list[int] | None = None,
                        opts: FieldOpts | None = None) -> int:
        """Add a multi-choice field; returns its index."""
        a: list = []
        c = self._field(opts, a)
        bufs = [cstr(a, s) for s in choices]
        arr = ffi.new("char *[]", bufs)
        a.append(arr)
        idx = checked or []
        cidx = ffi.new("size_t[]", idx) if idx else ffi.NULL
        return int(lib.sc_form_add_multiselect(
            self._p, cstr(a, label),
            ffi.cast("const char *const *", arr), len(bufs),
            cidx, len(idx), c[0]))

    def add_date(self, label: str, initial: datetime.date | None = None,
                 opts: FieldOpts | None = None) -> int:
        """Add a date field; returns its index. ``initial=None`` seeds today."""
        a: list = []
        c = self._field(opts, a)
        tm = ffi.new("struct tm *")
        if initial is not None:
            tm.tm_year = initial.year - 1900
            tm.tm_mon = initial.month - 1
            tm.tm_mday = initial.day
        return int(lib.sc_form_add_date(
            self._p, cstr(a, label), tm[0], c[0]))

    # ── run + getters ─────────────────────────────────────────────────────
    def run(self) -> bool:
        """Run the form. Returns ``True`` on submit, ``False`` on cancel/no-TTY."""
        return lib.sc_form_run(self._p) == lib.SC_INPUT_OK

    def modified(self) -> bool:
        """Whether any field differs from the value it was added with (e.g. for
        an "unsaved changes?" prompt when the user cancels)."""
        return bool(lib.sc_form_modified(self._p))

    def get_string(self, field: int) -> str | None:
        p = lib.sc_form_get_string(self._p, field)
        return None if p == ffi.NULL else ffi.string(p).decode()

    def get_number(self, field: int) -> float:
        return float(lib.sc_form_get_number(self._p, field))

    def get_bool(self, field: int) -> bool:
        return bool(lib.sc_form_get_bool(self._p, field))

    def get_choice(self, field: int) -> int:
        return int(lib.sc_form_get_choice(self._p, field))

    def get_checked(self, field: int) -> list[int]:
        """Checked add-order indices of a multiselect field."""
        n = lib.sc_form_get_checked(self._p, field, ffi.NULL, 0)
        if n == 0:
            return []
        out = ffi.new("size_t[]", n)
        lib.sc_form_get_checked(self._p, field, out, n)
        return [int(out[i]) for i in range(n)]

    def get_date(self, field: int) -> datetime.date | None:
        """Picked date of a date field, or ``None`` if not a date field."""
        tm = ffi.new("struct tm *")
        if lib.sc_form_get_date(self._p, field, tm):
            return datetime.date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday)
        return None

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "Form":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
