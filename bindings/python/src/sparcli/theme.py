"""Process-wide default styling for input widgets."""

from __future__ import annotations

from dataclasses import dataclass, field

from ._ffi import apply_border, apply_color, apply_style, cstr, ffi, lib
from .color import Color
from .enums import HintLayout, HintPos
from .style import BorderStyle, Style


@dataclass
class Theme:
    """Shared default styling. Any field left at its zero-init default falls
    through to the built-in default; per-call options always win."""

    accent: Color = Color.NONE
    border: BorderStyle = field(default_factory=BorderStyle)
    prompt_style: Style = field(default_factory=Style)
    selected_style: Style = field(default_factory=Style)
    cursor_style: Style = field(default_factory=Style)
    count_style: Style = field(default_factory=Style)
    summary_style: Style = field(default_factory=Style)
    error_style: Style = field(default_factory=Style)
    hint_style: Style = field(default_factory=Style)
    cursor_marker: str | None = None
    marker: str | None = None
    checkbox_on: str | None = None
    checkbox_off: str | None = None
    hint_layout: HintLayout = HintLayout.DEFAULT
    hint_pos: HintPos = HintPos.DEFAULT


def set_theme(theme: Theme | None) -> None:
    """Install the process-wide input theme. ``None`` clears it. Not
    thread-safe."""
    if theme is None:
        lib.sc_input_set_theme(ffi.NULL)
        return
    # The C side copies the struct AND its string fields, so the cffi
    # buffers only need to live until the call returns.
    arena: list = []
    c = ffi.new("ScInputTheme *")
    apply_color(c.accent, theme.accent)
    apply_border(c.border, theme.border)
    apply_style(c.prompt_style, theme.prompt_style)
    apply_style(c.selected_style, theme.selected_style)
    apply_style(c.cursor_style, theme.cursor_style)
    apply_style(c.count_style, theme.count_style)
    apply_style(c.summary_style, theme.summary_style)
    apply_style(c.error_style, theme.error_style)
    apply_style(c.hint_style, theme.hint_style)
    c.cursor_marker = cstr(arena, theme.cursor_marker)
    c.marker = cstr(arena, theme.marker)
    c.checkbox_on = cstr(arena, theme.checkbox_on)
    c.checkbox_off = cstr(arena, theme.checkbox_off)
    c.hint_layout = int(theme.hint_layout)
    c.hint_pos = int(theme.hint_pos)
    lib.sc_input_set_theme(c)
