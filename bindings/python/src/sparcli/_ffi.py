"""Low-level FFI glue: the compiled module plus cdata conversion helpers.

This module is private. It centralizes the only place that touches raw cffi
objects so the public wrapper classes can stay declarative.

The conversion helpers mutate *nested* cdata views in place (``c.title.style.fg``
is a live alias into the struct's memory), which avoids by-value struct copies
and keeps the "zero-init == unset/default" semantics the C side relies on:
anything we don't assign stays zeroed.
"""

from __future__ import annotations

from typing import TYPE_CHECKING

from sparcli._sparcli_cffi import ffi, lib

if TYPE_CHECKING:  # pragma: no cover
    from .color import Color
    from .style import BorderStyle, Edges, Style, Title

__all__ = ["ffi", "lib", "cstr", "apply_color", "apply_style", "apply_border",
           "apply_edges", "apply_title", "Arena"]

# An arena is just a list that keeps cffi-owned buffers alive for the duration
# of a call (or a handle's lifetime). Once it is dropped, the buffers are freed.
Arena = list


def cstr(arena: list, s: str | None) -> object:
    """Encode ``s`` to a NUL-terminated C string kept alive by ``arena``.

    ``None`` maps to ``NULL``. Interior NUL bytes are rejected (display text
    never legitimately contains one).
    """
    if s is None:
        return ffi.NULL
    if not isinstance(s, str):
        raise TypeError(f"expected str or None, got {type(s).__name__}")
    if "\x00" in s:
        raise ValueError("string must not contain an interior NUL byte")
    buf = ffi.new("char[]", s.encode("utf-8"))
    arena.append(buf)
    return buf


def apply_color(dst, color: "Color") -> None:
    """Copy a :class:`Color` into a live ``ScColor`` cdata view."""
    dst.index = color.index
    dst.r = color.r
    dst.g = color.g
    dst.b = color.b


def apply_style(dst, style: "Style") -> None:
    """Copy a :class:`Style` into a live ``ScTextStyle`` cdata view."""
    dst.attr = int(style.attr)
    apply_color(dst.fg, style.fg)
    apply_color(dst.bg, style.bg)


def apply_border(dst, border: "BorderStyle") -> None:
    """Copy a :class:`BorderStyle` into a live ``ScBorderStyle`` cdata view."""
    dst.type = int(border.type)
    apply_color(dst.color, border.color)
    apply_color(dst.bg, border.bg)


def apply_edges(dst, edges: "Edges") -> None:
    """Copy :class:`Edges` into a live ``ScEdges`` cdata view."""
    dst.top = edges.top
    dst.right = edges.right
    dst.bottom = edges.bottom
    dst.left = edges.left


def apply_title(dst, title: "Title", arena: list) -> None:
    """Copy a :class:`Title` into a live ``ScTitle`` cdata view.

    A ``rich_text`` Text is borrowed by the C side for the duration of the
    render, so the caller must keep the Text object alive across the call.
    """
    if title.text is not None:
        dst.text = cstr(arena, title.text)
    if title.rich_text is not None:
        dst.rich_text = title.rich_text._ptr
    apply_style(dst.style, title.style)
    dst.halign = int(title.halign)
    dst.pad = title.pad
    dst.pos = int(title.pos)
