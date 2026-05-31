"""The ``markup`` namespace: parse / print Rich-style inline markup."""

from __future__ import annotations

from ._ffi import cstr, ffi, lib
from .text import Text


def parse(markup: str, *, strip_unknown: bool = False) -> Text:
    """Parse ``markup`` into a :class:`~sparcli.text.Text`."""
    return Text.from_markup(markup, strip_unknown=strip_unknown)


def print(markup: str, *, strip_unknown: bool = False) -> None:  # noqa: A001
    """Parse and print ``markup`` (no trailing newline)."""
    arena: list = []
    opts = ffi.new("ScMarkupOpts *")
    opts.strip_unknown = strip_unknown
    lib.sc_markup_print_opts(cstr(arena, markup), opts[0])


def println(markup: str, *, strip_unknown: bool = False) -> None:
    """Parse and print ``markup`` followed by a newline."""
    arena: list = []
    opts = ffi.new("ScMarkupOpts *")
    opts.strip_unknown = strip_unknown
    lib.sc_markup_println_opts(cstr(arena, markup), opts[0])
