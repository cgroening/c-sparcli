"""Capture widgets into :class:`Rendered` buffers; pad, align and stack them."""

from __future__ import annotations

import weakref
from dataclasses import dataclass

from ._ffi import cstr, ffi, lib
from .enums import Align
from .output import PanelOpts, RuleOpts
from .table import Table, TableOpts, _opts_to_c
from .text import Text


@dataclass
class PadOpts:
    """Padding around a captured rendering."""

    top: int = 0
    right: int = 0
    bottom: int = 0
    left: int = 0


class Rendered:
    """A captured, line-split rendering (ANSI codes preserved per line)."""

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, p) -> None:
        if p == ffi.NULL:
            raise MemoryError("capture failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_rendered_free, p)

    @property
    def lines(self) -> list[str]:
        """The rendered lines, including their ANSI escape codes."""
        n = self._p.line_count
        return [ffi.string(self._p.lines[i]).decode() for i in range(n)]

    @property
    def width(self) -> int:
        """Maximum visible column width across all lines."""
        return int(self._p.max_column_width)

    def pad(self, opts: PadOpts = PadOpts()) -> None:
        """Print the rendering with padding around each line."""
        c = ffi.new(
            "ScPadOpts *", (opts.top, opts.right, opts.bottom, opts.left))
        lib.sc_pad_print(self._p, c[0])

    def align(self, halign: Align, width: int = 0) -> None:
        """Print aligned within ``width`` columns (0 = terminal width)."""
        lib.sc_align_print(self._p, int(halign), width)

    def close(self) -> None:
        self._finalizer()

    @property
    def _ptr(self):
        return self._p

    def __enter__(self) -> "Rendered":
        return self

    def __exit__(self, *exc) -> None:
        self.close()


class _Capture:
    """The ``capture`` namespace: render any widget into a :class:`Rendered`."""

    @staticmethod
    def string(s: str) -> Rendered:
        arena: list = []
        return Rendered(lib.sc_capture_str(cstr(arena, s)))

    @staticmethod
    def text(t: Text) -> Rendered:
        return Rendered(lib.sc_capture_text(t._ptr))

    @staticmethod
    def table(t: Table, opts: TableOpts = TableOpts()) -> Rendered:
        arena: list = []
        c = _opts_to_c(opts, arena)
        return Rendered(lib.sc_capture_table(t._ptr, c[0]))

    @staticmethod
    def list(lst) -> Rendered:  # noqa: A003
        return Rendered(lib.sc_capture_list(lst._ptr))

    @staticmethod
    def tree(tr) -> Rendered:
        return Rendered(lib.sc_capture_tree(tr._ptr))

    @staticmethod
    def kv(kv) -> Rendered:
        return Rendered(lib.sc_capture_kv(kv._ptr))

    @staticmethod
    def columns(cl) -> Rendered:
        return Rendered(lib.sc_capture_columns(cl._ptr))

    @staticmethod
    def panel(content: "str | Text", opts: PanelOpts = PanelOpts()) -> Rendered:
        arena: list = []
        c = ffi.new("ScPanelOpts *")
        opts._fill(c, arena)
        if isinstance(content, Text):
            return Rendered(lib.sc_capture_panel_text(content._ptr, c[0]))
        return Rendered(lib.sc_capture_panel_str(cstr(arena, content), c[0]))

    @staticmethod
    def rule(title: "str | Text | None" = None,
             opts: RuleOpts = RuleOpts()) -> Rendered:
        arena: list = []
        c = ffi.new("ScRuleOpts *")
        opts._fill(c, arena)
        if isinstance(title, Text):
            return Rendered(lib.sc_capture_rule_text(title._ptr, c[0]))
        return Rendered(lib.sc_capture_rule_str(cstr(arena, title), c[0]))


capture = _Capture()


def vstack(parts: "list[Rendered]", gap: int = 0) -> Rendered | None:
    """Stack captured renderings top-to-bottom into one :class:`Rendered`.

    Inputs are not consumed. Returns ``None`` when ``parts`` is empty.
    """
    if not parts:
        return None
    arr = ffi.new("ScRendered *[]", [p._ptr for p in parts])
    return Rendered(lib.sc_vstack(arr, len(parts), max(0, gap)))


# ── one-step convenience wrappers ─────────────────────────────────────────────
def pad_str(s: str, opts: PadOpts = PadOpts()) -> None:
    c = ffi.new("ScPadOpts *", (opts.top, opts.right, opts.bottom, opts.left))
    arena: list = []
    lib.sc_pad_str(cstr(arena, s), c[0])


def pad_text(t: Text, opts: PadOpts = PadOpts()) -> None:
    c = ffi.new("ScPadOpts *", (opts.top, opts.right, opts.bottom, opts.left))
    lib.sc_pad_text(t._ptr, c[0])


def align_str(s: str, halign: Align, width: int = 0) -> None:
    arena: list = []
    lib.sc_align_str(cstr(arena, s), int(halign), width)


def align_text(t: Text, halign: Align, width: int = 0) -> None:
    lib.sc_align_text(t._ptr, int(halign), width)


class Live:
    """Re-render a composed frame in place: a continuously updating dashboard.

    Build each frame with the capture API (:data:`capture`, :func:`vstack`)
    and pass it to :meth:`update`; the previous frame is overwritten in
    place. When the output stream is not a terminal (pipes, CI), updates are
    buffered and only the final frame is printed when the session ends, so
    the same code produces clean output in scripts.

        with sc.Live() as live:
            for i in range(101):
                live.update(sc.capture.string(f"progress: {i}%"))

    Parameters
    ----------
    alt_screen
        Render fullscreen on the alternate screen buffer (htop-style); the
        previous terminal content is restored when the session ends.
    show_cursor
        Keep the cursor visible (default: hidden during the session).
    transient
        Erase the live region on end instead of leaving the final frame.
    always
        Emit redraw escape codes even when output is not a terminal.
    """

    __slots__ = ("_live",)

    def __init__(self, alt_screen: bool = False, show_cursor: bool = False,
                 transient: bool = False, always: bool = False) -> None:
        opts = ffi.new("ScLiveOpts *")
        opts.alt_screen = alt_screen
        opts.show_cursor = show_cursor
        opts.transient = transient
        opts.always = always
        self._live = lib.sc_live_begin(opts[0])

    def update(self, frame: Rendered | str) -> None:
        """Redraw the live region with a captured frame or a plain string."""
        if self._live is None:
            return
        if isinstance(frame, str):
            arena: list = []
            lib.sc_live_update_str(self._live, cstr(arena, frame))
        else:
            lib.sc_live_update(self._live, frame._ptr)

    def end(self) -> None:
        """End the session (idempotent): restore the terminal and, when the
        output is not a terminal, print the buffered final frame."""
        if self._live is not None:
            lib.sc_live_end(self._live)
            self._live = None

    def __enter__(self) -> "Live":
        return self

    def __exit__(self, *exc) -> None:
        self.end()
