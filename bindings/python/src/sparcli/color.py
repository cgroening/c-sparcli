"""Terminal colors."""

from __future__ import annotations

from dataclasses import dataclass
from typing import ClassVar


@dataclass(frozen=True)
class Color:
    """A terminal color: unset, a named ANSI color, or 24-bit RGB.

    ``index`` follows the C sentinel: ``0`` = not set (no escape codes emitted),
    ``-1`` = RGB mode (uses ``r``/``g``/``b``), ``1``-``8`` = named ANSI color.
    A default-constructed ``Color()`` is :data:`Color.NONE`.

    Use the named class constants (:data:`Color.RED`, …) or :meth:`rgb`.
    """

    index: int = 0
    r: int = 0
    g: int = 0
    b: int = 0

    @staticmethod
    def rgb(r: int, g: int, b: int) -> "Color":
        """A 24-bit RGB color (channels clamped to 0-255)."""
        clamp = lambda v: max(0, min(255, int(v)))  # noqa: E731
        return Color(-1, clamp(r), clamp(g), clamp(b))

    # Named ANSI colors (set below). ClassVar so they aren't dataclass fields.
    NONE: ClassVar["Color"]
    BLACK: ClassVar["Color"]
    RED: ClassVar["Color"]
    GREEN: ClassVar["Color"]
    YELLOW: ClassVar["Color"]
    BLUE: ClassVar["Color"]
    MAGENTA: ClassVar["Color"]
    CYAN: ClassVar["Color"]
    WHITE: ClassVar["Color"]


Color.NONE = Color(0)
Color.BLACK = Color(1)
Color.RED = Color(2)
Color.GREEN = Color(3)
Color.YELLOW = Color(4)
Color.BLUE = Color(5)
Color.MAGENTA = Color(6)
Color.CYAN = Color(7)
Color.WHITE = Color(8)


class Palette:
    """Named 24-bit RGB palette (the C ``SC_COLOR_*`` constants).

    Additional to the eight ANSI :class:`Color` constants; every member is an
    RGB :class:`Color`. Use as ``sc.Palette.ACCENT``.
    """

    # Standard colors
    RED = Color.rgb(243, 139, 139)
    ORANGE = Color.rgb(248, 178, 136)
    YELLOW = Color.rgb(249, 230, 175)
    GREEN = Color.rgb(165, 227, 164)
    CYAN = Color.rgb(148, 225, 239)
    BLUE = Color.rgb(186, 213, 255)
    PURPLE = Color.rgb(207, 173, 247)
    MAGENTA = Color.rgb(245, 159, 224)
    BLACK = Color.rgb(0, 0, 0)
    WHITE = Color.rgb(255, 255, 255)
    # Vivid variants
    RED_VIVID = Color.rgb(255, 69, 87)
    ORANGE_VIVID = Color.rgb(255, 140, 58)
    YELLOW_VIVID = Color.rgb(255, 213, 81)
    GREEN_VIVID = Color.rgb(0, 227, 53)
    CYAN_VIVID = Color.rgb(64, 230, 255)
    BLUE_VIVID = Color.rgb(108, 165, 255)
    PURPLE_VIVID = Color.rgb(175, 77, 255)
    MAGENTA_VIVID = Color.rgb(255, 81, 223)
    # Dark variants
    RED_DARK = Color.rgb(65, 11, 16)
    ORANGE_DARK = Color.rgb(63, 30, 7)
    YELLOW_DARK = Color.rgb(60, 48, 12)
    GREEN_DARK = Color.rgb(0, 49, 5)
    CYAN_DARK = Color.rgb(8, 54, 61)
    BLUE_DARK = Color.rgb(21, 38, 64)
    PURPLE_DARK = Color.rgb(43, 13, 67)
    MAGENTA_DARK = Color.rgb(64, 14, 55)
    # Background / foreground
    BG = Color.rgb(21, 21, 21)
    BG_DARKEN_1 = Color.rgb(17, 17, 17)
    BG_DARKEN_2 = Color.rgb(12, 12, 12)
    BG_LIGHTEN_1 = Color.rgb(26, 26, 26)
    BG_LIGHTEN_2 = Color.rgb(37, 37, 37)
    BG_LIGHTEN_3 = Color.rgb(43, 43, 43)
    BG_SELECTED = Color.rgb(3, 101, 198)
    FG = Color.rgb(212, 212, 212)
    FG_DARKEN_1 = Color.rgb(204, 204, 204)
    FG_DARKEN_2 = Color.rgb(187, 187, 187)
    FG_DARKEN_3 = Color.rgb(170, 170, 170)
    FG_LIGHTEN_1 = Color.rgb(238, 238, 238)
    FG_LIGHTEN_2 = Color.rgb(253, 253, 253)
    # Accent colors
    ACCENT = Color.rgb(140, 210, 204)
    ACCENT_DIM = Color.rgb(113, 162, 157)
    ACCENT_DARKER = Color.rgb(79, 114, 111)
    ACCENT_DARK = Color.rgb(49, 73, 71)
    ACCENT_IMPORTANT = Color.rgb(255, 236, 150)
    # Status colors
    ENABLED = Color.rgb(112, 223, 129)
    DISABLED = Color.rgb(224, 108, 117)
    DISABLED_DIM = Color.rgb(128, 128, 128)
    # Diagnostics
    ERROR = Color.rgb(244, 135, 113)
    WARNING = Color.rgb(255, 185, 84)
    SUCCESS = Color.rgb(166, 227, 161)
    INFO = Color.rgb(148, 225, 239)
    HINT = Color.rgb(170, 170, 170)
    UNUSED = Color.rgb(98, 98, 98)
