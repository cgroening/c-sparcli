"""Text style, border style, edges and title value types."""

from __future__ import annotations

from dataclasses import dataclass, field

from .color import Color
from .enums import Align, Attr, BorderType, Position


@dataclass
class Style:
    """A styled text span: attribute flags + foreground/background color.

    Construct with keyword args - ``Style(attr=Attr.BOLD, fg=Color.GREEN)`` - or
    use the convenience constructors :meth:`bold`, :meth:`dim`, :meth:`italic`,
    :meth:`underline`. A default ``Style()`` applies no formatting.
    """

    attr: Attr = Attr.NONE
    fg: Color = Color.NONE
    bg: Color = Color.NONE

    @classmethod
    def bold(cls, fg: Color = Color.NONE, bg: Color = Color.NONE) -> "Style":
        return cls(Attr.BOLD, fg, bg)

    @classmethod
    def dim(cls, fg: Color = Color.NONE, bg: Color = Color.NONE) -> "Style":
        return cls(Attr.DIM, fg, bg)

    @classmethod
    def italic(cls, fg: Color = Color.NONE, bg: Color = Color.NONE) -> "Style":
        return cls(Attr.ITALIC, fg, bg)

    @classmethod
    def underline(
        cls, fg: Color = Color.NONE, bg: Color = Color.NONE
    ) -> "Style":
        return cls(Attr.UNDERLINE, fg, bg)


@dataclass
class BorderStyle:
    """A border's character set plus its foreground and background color."""

    type: BorderType = BorderType.NONE
    color: Color = Color.NONE
    bg: Color = Color.NONE


@dataclass
class Edges:
    """Box-model insets (top, right, bottom, left). Zero = no inset."""

    top: int = 0
    right: int = 0
    bottom: int = 0
    left: int = 0


@dataclass
class Title:
    """A component title: text (or rich :class:`~sparcli.text.Text`), style,
    alignment, padding and edge.

    ``rich_text`` (a :class:`~sparcli.text.Text`) overrides ``text``/``style``
    where supported (panels, boxed prompts). ``pos`` is ignored by rules.
    """

    text: str | None = None
    rich_text: object | None = None  # sparcli.text.Text; avoids an import cycle
    style: Style = field(default_factory=Style)
    halign: Align = Align.LEFT
    pad: int = 0
    pos: Position = Position.TOP
