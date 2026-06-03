"""The ``markup`` namespace: parse / print Rich-style inline markup."""

from __future__ import annotations

from ._ffi import apply_style, cstr, ffi, lib
from .style import Style
from .text import Text


def parse(
    markup: str,
    *,
    strip_unknown: bool = False,
    code_style: Style | None = None,
) -> Text:
    """Parse ``markup`` into a :class:`~sparcli.text.Text`.

    ``code_style`` styles backtick ```inline code``` spans; ``None`` renders
    them in the default magenta foreground.
    """
    return Text.from_markup(
        markup, strip_unknown=strip_unknown, code_style=code_style
    )


def print(  # noqa: A001
    markup: str,
    *,
    strip_unknown: bool = False,
    code_style: Style | None = None,
) -> None:
    """Parse and print ``markup`` (no trailing newline)."""
    arena: list = []
    opts = _markup_opts(strip_unknown, code_style)
    lib.sc_markup_print_opts(cstr(arena, markup), opts[0])


def println(
    markup: str,
    *,
    strip_unknown: bool = False,
    code_style: Style | None = None,
) -> None:
    """Parse and print ``markup`` followed by a newline."""
    arena: list = []
    opts = _markup_opts(strip_unknown, code_style)
    lib.sc_markup_println_opts(cstr(arena, markup), opts[0])


def _markup_opts(strip_unknown: bool, code_style: Style | None):
    """Build a populated ``ScMarkupOpts *`` cdata from the keyword options."""
    opts = ffi.new("ScMarkupOpts *")
    opts.strip_unknown = strip_unknown
    if code_style is not None:
        apply_style(opts.code_style, code_style)
    return opts
