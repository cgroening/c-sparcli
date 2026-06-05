"""Several progress bars updated together in place."""

from __future__ import annotations

import weakref
from dataclasses import dataclass

from ._ffi import cstr, ffi, lib
from .progress import ProgressBarOpts


@dataclass
class MultiProgressOpts:
    """Options for :class:`MultiProgress`."""

    transient: bool = False
    always: bool = False

    def _fill(self, c) -> None:
        c.transient = self.transient
        c.always = self.always


class MultiProgress:
    """A live stack of progress bars. Off a terminal, only the final stack is
    printed. Usable as a context manager; ends on close/GC."""

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, opts: MultiProgressOpts = MultiProgressOpts()) -> None:
        c = ffi.new("ScMultiProgressOpts *")
        opts._fill(c)
        p = lib.sc_multiprogress_begin(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_multiprogress_begin failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_multiprogress_end, p)

    def add(self, label: str | None,
            opts: ProgressBarOpts = ProgressBarOpts()) -> int:
        """Add a bar with ``label``; returns its index (-1 on failure)."""
        arena: list = []
        c = ffi.new("ScProgressBarOpts *")
        opts._fill(c, arena)
        return lib.sc_multiprogress_add(self._p, cstr(arena, label), c[0])

    def update(self, index: int, value: float, max: float = 0.0) -> None:
        """Update bar ``index`` and redraw. ``max == 0`` -> ``value`` is a
        0..1 ratio."""
        lib.sc_multiprogress_update(self._p, index, value, max)

    def set_label(self, index: int, label: str | None) -> None:
        """Replace a bar's label and redraw."""
        arena: list = []
        lib.sc_multiprogress_set_label(self._p, index, cstr(arena, label))

    def close(self) -> None:
        """End the session (leaves the final stack unless ``transient``)."""
        self._finalizer()

    def __enter__(self) -> "MultiProgress":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
