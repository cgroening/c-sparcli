"""Rich multi-span text (:class:`Text`)."""

from __future__ import annotations

import weakref

from ._ffi import apply_style, cstr, ffi, lib
from .style import Style


class Text:
    """A sequence of independently styled text spans.

    Build it incrementally with :meth:`append` (chainable), or parse markup with
    :meth:`from_markup`. Freed automatically; also usable as a context manager
    for deterministic release.

    Held by a raw C pointer, so a ``Text`` is **not** thread-safe to share - the
    C output target is thread-local; build and use it on one thread.
    """

    __slots__ = ("_p", "_finalizer", "_arena", "__weakref__")

    def __init__(self) -> None:
        p = lib.sc_text_new()
        if p == ffi.NULL:
            raise MemoryError("sc_text_new failed")
        self._p = p
        self._arena: list = []
        self._finalizer = weakref.finalize(self, lib.sc_text_free, p)

    # ── constructors ────────────────────────────────────────────────────────
    @classmethod
    def from_str(cls, s: str) -> "Text":
        """A Text holding ``s`` as one unstyled span."""
        t = cls()
        t.append(s)
        return t

    @classmethod
    def from_markup(cls, markup: str, *, strip_unknown: bool = False) -> "Text":
        """Parse Rich-style ``markup`` (e.g. ``[bold red]hi[/]``) into Text."""
        t = cls()
        t.append_markup(markup, strip_unknown=strip_unknown)
        return t

    # ── building ────────────────────────────────────────────────────────────
    def append(self, s: str, style: Style = Style()) -> "Text":
        """Append a styled span. Returns ``self`` for chaining."""
        arena: list = []
        cstyle = ffi.new("ScTextStyle *")
        apply_style(cstyle, style)
        lib.sc_text_append(self._p, cstr(arena, s), cstyle[0])
        return self

    def append_markup(
        self, markup: str, *, strip_unknown: bool = False
    ) -> "Text":
        """Parse ``markup`` and append the resulting spans. Returns ``self``."""
        arena: list = []
        opts = ffi.new("ScMarkupOpts *")
        opts.strip_unknown = strip_unknown
        lib.sc_markup_append_opts(self._p, cstr(arena, markup), opts[0])
        return self

    # ── queries / output ─────────────────────────────────────────────────────
    @property
    def visible_width(self) -> int:
        """Max visible column width across all lines (ANSI/UTF-8 aware)."""
        return int(lib.sc_text_visible_width(self._p))

    def print(self) -> None:
        """Print to the current output stream."""
        lib.sc_print_text(self._p)

    def close(self) -> None:
        """Free the underlying C object now (idempotent)."""
        self._finalizer()

    # ── plumbing ─────────────────────────────────────────────────────────────
    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Text":
        return self

    def __exit__(self, *exc) -> None:
        self.close()
