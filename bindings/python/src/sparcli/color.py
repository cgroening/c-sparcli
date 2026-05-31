"""Terminal colors."""

from __future__ import annotations

from dataclasses import dataclass
from typing import ClassVar


@dataclass(frozen=True)
class Color:
    """A terminal color: unset, a named ANSI color, or 24-bit RGB.

    ``index`` follows the C sentinel: ``0`` = not set (no escape codes emitted),
    ``-1`` = RGB mode (uses ``r``/``g``/``b``), ``1``–``8`` = named ANSI color.
    A default-constructed ``Color()`` is :data:`Color.NONE`.

    Use the named class constants (:data:`Color.RED`, …) or :meth:`rgb`.
    """

    index: int = 0
    r: int = 0
    g: int = 0
    b: int = 0

    @staticmethod
    def rgb(r: int, g: int, b: int) -> "Color":
        """A 24-bit RGB color (channels clamped to 0–255)."""
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
