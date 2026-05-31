"""Simple output: styled text, panels, rules, badges, alerts, utilities."""

from __future__ import annotations

import os
from dataclasses import dataclass, field

from ._ffi import (apply_border, apply_color, apply_edges, apply_style,
                   apply_title, cstr, ffi, lib)
from .color import Color
from .enums import Align, BorderType
from .style import BorderStyle, Edges, Style, Title
from .text import Text


def _as_title(t: "str | Title | None") -> Title | None:
    if t is None or isinstance(t, Title):
        return t
    return Title(text=t)


# ── styled text ──────────────────────────────────────────────────────────────
def print_(s: str, style: Style = Style()) -> None:
    """Print ``s`` with ANSI styling (no newline). Named ``print_`` to avoid
    shadowing the builtin; also exported as ``sparcli.print_``."""
    arena: list = []
    cstyle = ffi.new("ScTextStyle *")
    apply_style(cstyle, style)
    lib.sc_print(cstr(arena, s), cstyle[0])


def println(s: str, style: Style = Style()) -> None:
    """Print ``s`` with ANSI styling, followed by a newline."""
    arena: list = []
    cstyle = ffi.new("ScTextStyle *")
    apply_style(cstyle, style)
    lib.sc_println(cstr(arena, s), cstyle[0])


# ── panel ────────────────────────────────────────────────────────────────────
@dataclass
class PanelOpts:
    """Layout/visual options for :func:`panel`."""

    border: BorderStyle = field(default_factory=BorderStyle)
    bg: Color = Color.NONE
    title: "str | Title | None" = None
    subtitle: Title | None = None
    full_width: bool = False
    width: int = 0
    content_align: Align = Align.LEFT
    padding: Edges = field(default_factory=Edges)
    margin: Edges = field(default_factory=Edges)

    def _fill(self, c, arena: list) -> None:
        apply_border(c.border, self.border)
        apply_color(c.bg, self.bg)
        title = _as_title(self.title)
        if title is not None:
            apply_title(c.title, title, arena)
        if self.subtitle is not None:
            apply_title(c.subtitle, self.subtitle, arena)
        c.full_width = self.full_width
        c.width = self.width
        c.content_align = int(self.content_align)
        apply_edges(c.padding, self.padding)
        apply_edges(c.margin, self.margin)


def panel(content: "str | Text", opts: PanelOpts = PanelOpts()) -> None:
    """Render ``content`` (str or :class:`~sparcli.text.Text`) in a bordered panel."""
    arena: list = []
    c = ffi.new("ScPanelOpts *")
    opts._fill(c, arena)
    if isinstance(content, Text):
        lib.sc_panel_text(content._ptr, c[0])
    else:
        lib.sc_panel_str(cstr(arena, content), c[0])


# ── rule ─────────────────────────────────────────────────────────────────────
@dataclass
class RuleOpts:
    """Options for :func:`rule`. ``title_*`` style the title passed to
    :func:`rule`; ``halign`` places the rule itself when ``width > 0``."""

    type: BorderType = BorderType.SINGLE
    color: Color = Color.NONE
    width: int = 0
    halign: Align = Align.CENTER
    margin: Edges = field(default_factory=Edges)
    title_style: Style = field(default_factory=Style)
    title_align: Align = Align.CENTER
    title_pad: int = 1

    def _fill(self, c, arena: list) -> None:
        c.type = int(self.type)
        apply_color(c.color, self.color)
        c.width = self.width
        c.halign = int(self.halign)
        apply_edges(c.margin, self.margin)
        apply_style(c.title.style, self.title_style)
        c.title.halign = int(self.title_align)
        c.title.pad = self.title_pad


def rule(title: "str | Text | None" = None, opts: RuleOpts = RuleOpts()) -> None:
    """Draw a horizontal rule with an optional (styled) ``title``."""
    arena: list = []
    c = ffi.new("ScRuleOpts *")
    opts._fill(c, arena)
    if isinstance(title, Text):
        lib.sc_rule_text(title._ptr, c[0])
    else:
        lib.sc_rule_str(cstr(arena, title), c[0])


# ── badge ────────────────────────────────────────────────────────────────────
@dataclass
class BadgeOpts:
    """Options for :func:`badge`."""

    left_cap: str | None = None
    right_cap: str | None = None
    text_style: Style = field(default_factory=Style)
    pad: int = 0

    def _fill(self, c, arena: list) -> None:
        c.left_cap = cstr(arena, self.left_cap)
        c.right_cap = cstr(arena, self.right_cap)
        apply_style(c.text_style, self.text_style)
        c.pad = self.pad


def badge(text: str, opts: BadgeOpts = BadgeOpts()) -> None:
    """Print an inline badge token (no trailing newline)."""
    arena: list = []
    c = ffi.new("ScBadgeOpts *")
    opts._fill(c, arena)
    lib.sc_print_badge(cstr(arena, text), c[0])


# ── alerts ───────────────────────────────────────────────────────────────────
class _Alert:
    """The ``alert`` namespace: full-width preset panels."""

    @staticmethod
    def _emit(fn_str, fn_text, content):
        if isinstance(content, Text):
            fn_text(content._ptr)
        else:
            arena: list = []
            fn_str(cstr(arena, content))

    def info(self, content: "str | Text") -> None:
        self._emit(lib.sc_alert_info, lambda p: lib.sc_alert_text(0, p), content)

    def debug(self, content: "str | Text") -> None:
        self._emit(lib.sc_alert_debug, lambda p: lib.sc_alert_text(1, p), content)

    def warning(self, content: "str | Text") -> None:
        self._emit(lib.sc_alert_warning, lambda p: lib.sc_alert_text(2, p), content)

    def error(self, content: "str | Text") -> None:
        self._emit(lib.sc_alert_error, lambda p: lib.sc_alert_text(3, p), content)

    def success(self, content: "str | Text") -> None:
        self._emit(lib.sc_alert_success, lambda p: lib.sc_alert_text(4, p), content)


alert = _Alert()


# ── version & utilities ──────────────────────────────────────────────────────
def version() -> tuple[int, int, int]:
    """Runtime library version as a ``(major, minor, patch)`` tuple."""
    maj = ffi.new("int *")
    minr = ffi.new("int *")
    pat = ffi.new("int *")
    lib.sc_version(maj, minr, pat)
    return (maj[0], minr[0], pat[0])


def version_string() -> str:
    """Human-readable version string, e.g. ``"0.1.0"``."""
    return ffi.string(lib.sc_version_string()).decode()


def strip_ansi(s: str) -> str:
    """Return ``s`` with all ANSI CSI escape sequences removed."""
    arena: list = []
    out = lib.sc_strip_ansi(cstr(arena, s))
    if out == ffi.NULL:
        raise MemoryError("sc_strip_ansi failed")
    try:
        return ffi.string(out).decode()
    finally:
        lib.free(out)


def truncate(s: str, max_cols: int, ellipsis: str | None = "…") -> str:
    """Truncate ``s`` to ``max_cols`` visible columns, appending ``ellipsis``."""
    arena: list = []
    out = lib.sc_truncate(cstr(arena, s), max_cols, cstr(arena, ellipsis))
    if out == ffi.NULL:
        raise MemoryError("sc_truncate failed")
    try:
        return ffi.string(out).decode()
    finally:
        lib.free(out)


def clear_line() -> None:
    """Overwrite the current terminal line with spaces and reset the cursor."""
    lib.sc_clear_line()


class ScopedOutput:
    """Redirect sparcli's output stream to a writable file for the ``with`` block.

    ``target`` must expose a real OS file descriptor (an open file, a pipe end,
    …). The fd is duplicated, so the original file stays open; on exit sparcli
    reverts to ``stdout`` and the duplicate is flushed and closed.
    """

    def __init__(self, target) -> None:
        self._fd = os.dup(target.fileno())
        self._fp = None

    def __enter__(self) -> "ScopedOutput":
        self._fp = lib.fdopen(self._fd, b"w")
        if self._fp == ffi.NULL:
            os.close(self._fd)
            raise OSError("fdopen failed")
        lib.sc_output_set_stream(self._fp)
        return self

    def __exit__(self, *exc) -> None:
        lib.fflush(self._fp)
        lib.sc_output_set_stream(ffi.NULL)
        lib.fclose(self._fp)  # closes the duplicated fd
        self._fp = None
