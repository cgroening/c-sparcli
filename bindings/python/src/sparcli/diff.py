"""Colored line-based unified diff of two texts."""

from __future__ import annotations

from dataclasses import dataclass

from ._ffi import apply_color, cstr, ffi, lib
from .capture import Rendered
from .color import Color


@dataclass
class DiffOpts:
    """Options for the unified diff (defaults match the C zero-init)."""

    context: int = 0          # 0 = default (3); negative = full file
    no_header: bool = False
    old_label: str | None = None
    new_label: str | None = None
    add_color: Color = Color.NONE
    del_color: Color = Color.NONE
    hunk_color: Color = Color.NONE

    def _fill(self, c, arena: list) -> None:
        c.context = self.context
        c.no_header = self.no_header
        c.old_label = cstr(arena, self.old_label)
        c.new_label = cstr(arena, self.new_label)
        apply_color(c.add_color, self.add_color)
        apply_color(c.del_color, self.del_color)
        apply_color(c.hunk_color, self.hunk_color)


def diff(old: str, new: str, opts: DiffOpts | None = None) -> None:
    """Print a colored unified diff to the current output stream."""
    arena: list = []
    c = ffi.new("ScDiffOpts *")
    (opts or DiffOpts())._fill(c, arena)
    lib.sc_diff_print(cstr(arena, old), cstr(arena, new), c[0])


def diff_rendered(old: str, new: str, opts: DiffOpts | None = None) -> Rendered:
    """Capture a colored unified diff into a :class:`Rendered`."""
    arena: list = []
    c = ffi.new("ScDiffOpts *")
    (opts or DiffOpts())._fill(c, arena)
    return Rendered(lib.sc_capture_diff(cstr(arena, old), cstr(arena, new),
                                        c[0]))
