"""Interactive prompts: confirm, text, password, number, textarea, date picker.

Every widget returns its value on success, ``None`` when the user cancels
(Esc / Ctrl-C), and raises :class:`~sparcli.errors.SparcliInputUnavailable`
when no controlling terminal is available.
"""

from __future__ import annotations

import datetime as _dt
from dataclasses import dataclass, field
from decimal import Decimal

from ._ffi import apply_border, apply_color, apply_style, cstr, ffi, lib
from ._inputcommon import (fill_char_filter, fill_hint, fill_prompt_text,
                           fill_shortcuts, fill_suggestions, fill_validate,
                           result, take_cstr)
from .color import Color
from .enums import HintLayout, HintPos, WeekStart
from .keys import KeyChord, Shortcuts
from .style import BorderStyle, Style
from .text import Text

# Built-in character filters (pass as ``char_filter``).
filter_digits = lib.sc_filter_digits
filter_decimal = lib.sc_filter_decimal
filter_alpha = lib.sc_filter_alpha
filter_alnum = lib.sc_filter_alnum
filter_no_space = lib.sc_filter_no_space


def input_available() -> bool:
    """``True`` when an interactive prompt can run (a TTY is available)."""
    return bool(lib.sc_input_available())


# ── confirm ──────────────────────────────────────────────────────────────────
@dataclass
class ConfirmOpts:
    """Options for :func:`confirm`."""

    default_yes: bool = False
    yes_label: str | None = None
    no_label: str | None = None
    prompt_style: Style = field(default_factory=Style)
    accent: Color = Color.NONE
    selected_style: Style = field(default_factory=Style)
    unselected_style: Style = field(default_factory=Style)
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
        c.default_yes = self.default_yes
        c.yes_label = cstr(arena, self.yes_label)
        c.no_label = cstr(arena, self.no_label)
        apply_style(c.prompt_style, self.prompt_style)
        apply_color(c.accent, self.accent)
        apply_style(c.selected_style, self.selected_style)
        apply_style(c.unselected_style, self.unselected_style)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


def confirm(question: str, opts: ConfirmOpts = ConfirmOpts()) -> bool | None:
    """Ask a yes/no question. Returns the choice, or ``None`` on cancel."""
    arena: list = []
    c = ffi.new("ScConfirmOpts *")
    opts._fill(c, arena)
    out = ffi.new("bool *")
    st = lib.sc_confirm(cstr(arena, question), out, c[0])
    return result(st, lambda: bool(out[0]))


# ── text input ───────────────────────────────────────────────────────────────
@dataclass
class TextInputOpts:
    """Options for :func:`text_input`."""

    initial: str | None = None
    placeholder: str | None = None
    prompt_style: Style = field(default_factory=Style)
    value_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    error_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    max_chars: int = 0
    hide_char_count: bool = False
    count_style: Style = field(default_factory=Style)
    boxed: bool = False
    border: BorderStyle = field(default_factory=BorderStyle)
    width: int = 0
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    char_filter: object | None = None
    suggestions: list[str] | None = None
    validate: object | None = None
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False
    external_editor: bool = False
    editor: str | None = None
    editor_key: KeyChord | None = None

    def _fill(self, c, arena: list) -> None:
        c.initial = cstr(arena, self.initial)
        c.placeholder = cstr(arena, self.placeholder)
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.value_style, self.value_style)
        apply_style(c.cursor_style, self.cursor_style)
        apply_style(c.error_style, self.error_style)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        c.max_chars = self.max_chars
        c.hide_char_count = self.hide_char_count
        apply_style(c.count_style, self.count_style)
        c.boxed = self.boxed
        apply_border(c.border, self.border)
        c.width = self.width
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_char_filter(c, self.char_filter, arena)
        fill_suggestions(c, self.suggestions, arena)
        fill_validate(c, self.validate, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)
        c.external_editor = self.external_editor
        c.editor = cstr(arena, self.editor)
        if self.editor_key is not None:
            c.editor_key = self.editor_key.value


def text_input(
    prompt: str, opts: TextInputOpts = TextInputOpts()
) -> str | None:
    """Prompt for a single line of text."""
    arena: list = []
    c = ffi.new("ScTextInputOpts *")
    opts._fill(c, arena)
    out = ffi.new("char **")
    st = lib.sc_text_input(cstr(arena, prompt), out, c[0])
    return result(st, lambda: take_cstr(out))


# ── password ─────────────────────────────────────────────────────────────────
@dataclass
class PasswordOpts:
    """Options for :func:`password_input`."""

    placeholder: str | None = None
    mask: str | None = None
    prompt_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    error_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    max_chars: int = 0
    hide_char_count: bool = False
    count_style: Style = field(default_factory=Style)
    boxed: bool = False
    border: BorderStyle = field(default_factory=BorderStyle)
    width: int = 0
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    char_filter: object | None = None
    validate: object | None = None
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False

    def _fill(self, c, arena: list) -> None:
        c.placeholder = cstr(arena, self.placeholder)
        c.mask = cstr(arena, self.mask)
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.cursor_style, self.cursor_style)
        apply_style(c.error_style, self.error_style)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        c.max_chars = self.max_chars
        c.hide_char_count = self.hide_char_count
        apply_style(c.count_style, self.count_style)
        c.boxed = self.boxed
        apply_border(c.border, self.border)
        c.width = self.width
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_char_filter(c, self.char_filter, arena)
        fill_validate(c, self.validate, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


def password_input(
    prompt: str, opts: PasswordOpts = PasswordOpts()
) -> str | None:
    """Prompt for a secret, rendering each character as a mask glyph."""
    arena: list = []
    c = ffi.new("ScPasswordOpts *")
    opts._fill(c, arena)
    out = ffi.new("char **")
    st = lib.sc_password_input(cstr(arena, prompt), out, c[0])
    return result(st, lambda: take_cstr(out))


# ── number ───────────────────────────────────────────────────────────────────
@dataclass
class NumberOpts:
    """Options for :func:`number_input` / :func:`decimal_input`."""

    initial: float = 0.0
    min: float = 0.0
    max: float = 0.0
    step: float = 0.0
    decimals: int = 0
    decimal_sep: str = "."
    prompt_style: Style = field(default_factory=Style)
    value_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    boxed: bool = False
    border: BorderStyle = field(default_factory=BorderStyle)
    width: int = 0
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False

    def _fill(self, c, arena: list) -> None:
        c.initial = self.initial
        c.min = self.min
        c.max = self.max
        c.step = self.step
        c.decimals = self.decimals
        c.decimal_sep = (self.decimal_sep or ".").encode("ascii")[:1]
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.value_style, self.value_style)
        apply_style(c.cursor_style, self.cursor_style)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        c.boxed = self.boxed
        apply_border(c.border, self.border)
        c.width = self.width
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


def number_input(prompt: str, opts: NumberOpts = NumberOpts()) -> float | None:
    """Prompt for a number (type it, or adjust with ↑/↓ by ``step``)."""
    arena: list = []
    c = ffi.new("ScNumberOpts *")
    opts._fill(c, arena)
    out = ffi.new("double *")
    st = lib.sc_number_input(cstr(arena, prompt), out, c[0])
    return result(st, lambda: float(out[0]))


def decimal_input(
    prompt: str, opts: NumberOpts = NumberOpts()
) -> Decimal | None:
    """Prompt for a number, returning an exact :class:`~decimal.Decimal`.

    Same widget as :func:`number_input`, but the value is constructed from
    the submitted text (always ``'.'``-separated, clamping applied) and never
    round-trips through ``float`` - safe for money amounts.
    """
    arena: list = []
    c = ffi.new("ScNumberOpts *")
    opts._fill(c, arena)
    out = ffi.new("double *")
    out_text = ffi.new("char **")
    c.out_text = out_text
    st = lib.sc_number_input(cstr(arena, prompt), out, c[0])
    return result(st, lambda: Decimal(take_cstr(out_text)))


# ── textarea ─────────────────────────────────────────────────────────────────
@dataclass
class TextareaOpts:
    """Options for :func:`textarea`."""

    initial: str | None = None
    placeholder: str | None = None
    prompt_style: Style = field(default_factory=Style)
    value_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    hide_summary: bool = False
    hint: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT
    hint_style: Style = field(default_factory=Style)
    boxed: bool = False
    border: BorderStyle = field(default_factory=BorderStyle)
    width: int = 0
    shortcuts: Shortcuts | None = None
    prompt_text: Text | None = None
    prompt_markup: bool = False
    external_editor: bool = False
    editor: str | None = None
    editor_key: KeyChord | None = None

    def _fill(self, c, arena: list) -> None:
        c.initial = cstr(arena, self.initial)
        c.placeholder = cstr(arena, self.placeholder)
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.value_style, self.value_style)
        apply_style(c.cursor_style, self.cursor_style)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        c.boxed = self.boxed
        apply_border(c.border, self.border)
        c.width = self.width
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)
        c.external_editor = self.external_editor
        c.editor = cstr(arena, self.editor)
        if self.editor_key is not None:
            c.editor_key = self.editor_key.value


def textarea(prompt: str, opts: TextareaOpts = TextareaOpts()) -> str | None:
    """Prompt for multi-line text (Enter = newline, Ctrl-D submits)."""
    arena: list = []
    c = ffi.new("ScTextareaOpts *")
    opts._fill(c, arena)
    out = ffi.new("char **")
    st = lib.sc_textarea(cstr(arena, prompt), out, c[0])
    return result(st, lambda: take_cstr(out))


# ── date picker ──────────────────────────────────────────────────────────────
@dataclass
class DatePickerOpts:
    """Options for :func:`datepicker`."""

    prompt: str | None = None
    week_start: WeekStart = WeekStart.DEFAULT
    accent: Color = Color.NONE
    prompt_style: Style = field(default_factory=Style)
    header_style: Style = field(default_factory=Style)
    weekday_style: Style = field(default_factory=Style)
    selected_style: Style = field(default_factory=Style)
    header_prev: str | None = None
    header_next: str | None = None
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
        c.week_start = int(self.week_start)
        apply_color(c.accent, self.accent)
        apply_style(c.prompt_style, self.prompt_style)
        apply_style(c.header_style, self.header_style)
        apply_style(c.weekday_style, self.weekday_style)
        apply_style(c.selected_style, self.selected_style)
        c.header_prev = cstr(arena, self.header_prev)
        c.header_next = cstr(arena, self.header_next)
        apply_style(c.summary_style, self.summary_style)
        c.hide_summary = self.hide_summary
        fill_hint(c, self.hint, self.hint_layout, self.hint_pos,
                  self.hint_style, arena)
        fill_shortcuts(c, self.shortcuts, arena)
        fill_prompt_text(c, self.prompt_text, self.prompt_markup)


def datepicker(initial: _dt.date | None = None,
               opts: DatePickerOpts = DatePickerOpts()) -> _dt.date | None:
    """Pick a date from a month calendar. ``initial`` seeds the view (``None`` =
    today). Returns a :class:`datetime.date`, or ``None`` on cancel."""
    arena: list = []
    c = ffi.new("ScDatePickerOpts *")
    opts._fill(c, arena)
    tm = ffi.new("struct tm *")
    if initial is not None:
        tm.tm_year = initial.year - 1900
        tm.tm_mon = initial.month - 1
        tm.tm_mday = initial.day
    st = lib.sc_datepicker(tm, c[0])
    return result(
        st,
        lambda: _dt.date(tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday),
    )
