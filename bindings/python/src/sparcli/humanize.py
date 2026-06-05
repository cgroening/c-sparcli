"""Human-readable formatting: byte sizes, durations, relative time, numbers.

Each function returns a Python ``str``. Mirrors the C ``sc_humanize_*`` API.
"""

from __future__ import annotations

from dataclasses import dataclass

from ._ffi import ffi, lib
from .enums import ByteUnit


@dataclass
class HumanizeOpts:
    """Formatting options (defaults match the C zero-init behavior)."""

    decimals: int = 0
    decimal_sep: str | None = None   # None = '.'  (use ',' for de_DE)
    group_sep: str | None = None     # None = ','  (use '.' for de_DE)
    no_space: bool = False

    def _fill(self, c) -> None:
        c.decimals = self.decimals
        c.decimal_sep = self.decimal_sep.encode()[:1] if self.decimal_sep \
            else b"\x00"
        c.group_sep = self.group_sep.encode()[:1] if self.group_sep \
            else b"\x00"
        c.no_space = self.no_space


def _take(out) -> str:
    """Decode a C-heap string and free it."""
    if out == ffi.NULL:
        raise MemoryError("humanize: allocation failed")
    try:
        return ffi.string(out).decode()
    finally:
        lib.free(out)


def bytes(n: int, unit: ByteUnit = ByteUnit.SI,
          opts: HumanizeOpts | None = None) -> str:
    """Format a byte count, e.g. ``bytes(1536)`` -> ``"1.5 KB"``."""
    if opts is None:
        return _take(lib.sc_humanize_bytes(n, int(unit)))
    c = ffi.new("ScHumanizeOpts *")
    opts._fill(c)
    return _take(lib.sc_humanize_bytes_opts(n, int(unit), c[0]))


def number(value: float, opts: HumanizeOpts | None = None) -> str:
    """Format a number with thousands grouping, e.g. ``"1,234,567"``."""
    c = ffi.new("ScHumanizeOpts *")
    (opts or HumanizeOpts())._fill(c)
    return _take(lib.sc_humanize_number(value, c[0]))


def compact(value: float, opts: HumanizeOpts | None = None) -> str:
    """Format a number compactly, e.g. ``12400`` -> ``"12.4k"``."""
    c = ffi.new("ScHumanizeOpts *")
    (opts or HumanizeOpts())._fill(c)
    return _take(lib.sc_humanize_compact(value, c[0]))


def percent(ratio: float, opts: HumanizeOpts | None = None) -> str:
    """Format a ratio as a percentage, e.g. ``0.42`` -> ``"42%"``."""
    c = ffi.new("ScHumanizeOpts *")
    (opts or HumanizeOpts())._fill(c)
    return _take(lib.sc_humanize_percent(ratio, c[0]))


def duration(seconds: float) -> str:
    """Format a duration as two units, e.g. ``93`` -> ``"1m 33s"``."""
    return _take(lib.sc_humanize_duration(seconds))


def duration_clock(seconds: float) -> str:
    """Format a duration as a clock value, e.g. ``3725`` -> ``"01:02:05"``."""
    return _take(lib.sc_humanize_duration_clock(seconds))


def relative(when: int, now: int) -> str:
    """Format ``when`` relative to ``now`` (Unix seconds), e.g. "3 hours ago"."""
    return _take(lib.sc_humanize_relative(when, now))
