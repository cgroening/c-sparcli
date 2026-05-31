"""Exception types raised by the sparcli wrapper."""

from __future__ import annotations


class SparcliError(Exception):
    """Base class for every error raised by sparcli."""


class SparcliInputUnavailable(SparcliError):
    """An interactive prompt could not run.

    Raised when a widget returns ``SC_INPUT_ERROR`` - typically because there is
    no controlling terminal (stdin/stdout redirected, running under CI, …) or a
    read error occurred. Catch it to fall back to a non-interactive default.
    """
