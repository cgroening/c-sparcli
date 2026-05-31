"""Enumerations mirroring sparcli's C enums.

All are ``IntEnum`` (or ``IntFlag`` for the bitmask ``Attr``) so they convert
transparently to the integers the FFI layer expects.
"""

from __future__ import annotations

from enum import IntEnum, IntFlag


class Attr(IntFlag):
    """Text attribute bitmask; combine with ``|`` (e.g. ``Attr.BOLD | Attr.ITALIC``)."""

    NONE = 0
    BOLD = 1
    DIM = 2
    ITALIC = 4
    UNDERLINE = 8


class BorderType(IntEnum):
    """Border / line character set."""

    NONE = 0
    ASCII = 1
    SINGLE = 2
    DOUBLE = 3
    ROUNDED = 4
    THICK = 5


class Position(IntEnum):
    """Title placement (top / bottom edge)."""

    TOP = 0
    BOTTOM = 1


class Align(IntEnum):
    """Horizontal alignment."""

    LEFT = 0
    CENTER = 1
    RIGHT = 2


class VAlign(IntEnum):
    """Vertical alignment."""

    TOP = 0
    MIDDLE = 1
    BOTTOM = 2


class ListMarker(IntEnum):
    """List item marker style."""

    BULLET = 0
    NUMBER = 1
    ALPHA_LOWER = 2
    ALPHA_UPPER = 3
    ROMAN_LOWER = 4
    ROMAN_UPPER = 5


class ProgressType(IntEnum):
    """Progress-bar fill style."""

    BLOCK = 0
    ASCII = 1
    LINE = 2
    SHADED = 3


class SpinnerType(IntEnum):
    """Spinner animation frames."""

    BRAILLE = 0
    PIPE = 1
    DOTS = 2
    ARROW = 3


class AlertType(IntEnum):
    """Alert preset (icon + color)."""

    INFO = 0
    DEBUG = 1
    WARNING = 2
    ERROR = 3
    SUCCESS = 4


class HintLayout(IntEnum):
    """Key-hint footer layout."""

    DEFAULT = 0
    INLINE = 1
    STACKED = 2
    HIDDEN = 3


class HintPos(IntEnum):
    """Key-hint footer placement."""

    DEFAULT = 0
    TOP = 1
    BOTTOM = 2
    LEFT = 3
    RIGHT = 4


class WeekStart(IntEnum):
    """First weekday column in the date picker."""

    DEFAULT = 0
    MONDAY = 1
    SUNDAY = 2
