"""Animated progress bar and spinner."""

from __future__ import annotations

import weakref
from dataclasses import dataclass, field

from ._ffi import apply_color, apply_style, cstr, ffi, lib
from .color import Color
from .enums import ProgressType, SpinnerType
from .style import Style


@dataclass
class Thresholds:
    """Ratio-based fill-color switching for a :class:`ProgressBar`."""

    enabled: bool = False
    mid: float = 0.5
    high: float = 0.75
    color_low: Color = Color.NONE
    color_mid: Color = Color.NONE
    color_high: Color = Color.NONE


@dataclass
class ProgressBarOpts:
    """Rendering options for a :class:`ProgressBar`."""

    type: ProgressType = ProgressType.BLOCK
    left_cap: str | None = None
    right_cap: str | None = None
    fill_color: Color = Color.NONE
    empty_color: Color = Color.NONE
    thresholds: Thresholds = field(default_factory=Thresholds)
    show_percent: bool = True
    show_value: bool = False
    bar_width: int = 0
    width: int = 0
    label_width: int = 0
    label_style: Style = field(default_factory=Style)

    def _fill(self, c, arena: list) -> None:
        c.type = int(self.type)
        c.left_cap = cstr(arena, self.left_cap)
        c.right_cap = cstr(arena, self.right_cap)
        apply_color(c.fill_color, self.fill_color)
        apply_color(c.empty_color, self.empty_color)
        t = self.thresholds
        c.thresholds.enabled = t.enabled
        c.thresholds.mid = t.mid
        c.thresholds.high = t.high
        apply_color(c.thresholds.color_low, t.color_low)
        apply_color(c.thresholds.color_mid, t.color_mid)
        apply_color(c.thresholds.color_high, t.color_high)
        c.show_percent = self.show_percent
        c.show_value = self.show_value
        c.bar_width = self.bar_width
        c.width = self.width
        c.label_width = self.label_width
        apply_style(c.label_style, self.label_style)


class ProgressBar:
    """An in-place animated progress bar.

    ``value``/``max``: when ``max > 0`` the ratio is ``value/max``; when
    ``max == 0`` ``value`` is already a 0.0–1.0 ratio.
    """

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, opts: ProgressBarOpts = ProgressBarOpts()) -> None:
        arena: list = []
        c = ffi.new("ScProgressBarOpts *")
        opts._fill(c, arena)
        p = lib.sc_progressbar_new(c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_progressbar_new failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_progressbar_free, p)

    def set_label(self, label: str | None) -> None:
        arena: list = []
        lib.sc_progressbar_set_label(self._p, cstr(arena, label))

    def draw(self, value: float, max: float) -> None:  # noqa: A002
        lib.sc_progressbar_draw(self._p, float(value), float(max))

    def finish(self, value: float, max: float) -> None:  # noqa: A002
        lib.sc_progressbar_finish(self._p, float(value), float(max))

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "ProgressBar":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


@dataclass
class SpinnerOpts:
    """Rendering options for a :class:`Spinner`."""

    type: SpinnerType = SpinnerType.BRAILLE
    color: Color = Color.NONE
    label_style: Style = field(default_factory=Style)

    def _fill(self, c) -> None:
        c.type = int(self.type)
        apply_color(c.color, self.color)
        apply_style(c.label_style, self.label_style)


class Spinner:
    """An in-place animated spinner with an updatable label."""

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, label: str | None = None,
                 opts: SpinnerOpts = SpinnerOpts()) -> None:
        arena: list = []
        c = ffi.new("ScSpinnerOpts *")
        opts._fill(c)
        p = lib.sc_spinner_new(cstr(arena, label), c[0])
        if p == ffi.NULL:
            raise MemoryError("sc_spinner_new failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_spinner_free, p)

    def set_label(self, label: str | None) -> None:
        arena: list = []
        lib.sc_spinner_set_label(self._p, cstr(arena, label))

    def tick(self) -> None:
        lib.sc_spinner_tick(self._p)

    def finish(self, success: bool, label: str | None = None) -> None:
        arena: list = []
        lib.sc_spinner_finish(self._p, bool(success), cstr(arena, label))

    def close(self) -> None:
        self._finalizer()

    def __enter__(self) -> "Spinner":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
