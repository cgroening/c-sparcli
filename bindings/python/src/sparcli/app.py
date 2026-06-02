"""Application-framework helpers: XDG base directories, pager, pretty errors."""

from __future__ import annotations

import weakref
from pathlib import Path
from typing import NoReturn

from ._ffi import cstr, ffi, lib
from .enums import PathKind
from .errors import SparcliError


def app_dir(kind: PathKind, appname: str) -> Path:
    """Return the per-application XDG directory, creating it on first use.

    Parameters
    ----------
    kind
        Which base directory to resolve (config / data / cache / state).
    appname
        Application name; must be a single path component (no ``/``, no
        ``..``).

    Returns
    -------
    Path
        The absolute application directory, e.g. ``~/.config/myapp``.

    Raises
    ------
    SparcliError
        When ``appname`` is invalid or the directory cannot be created.
    """
    arena: list = []
    out = lib.sc_path(int(kind), cstr(arena, appname))
    return _take_path(out, appname)


def config_dir(appname: str) -> Path:
    """Return ``~/.config/<appname>`` (or ``$XDG_CONFIG_HOME``), created."""
    return app_dir(PathKind.CONFIG, appname)


def data_dir(appname: str) -> Path:
    """Return ``~/.local/share/<appname>`` (or ``$XDG_DATA_HOME``), created."""
    return app_dir(PathKind.DATA, appname)


def cache_dir(appname: str) -> Path:
    """Return ``~/.cache/<appname>`` (or ``$XDG_CACHE_HOME``), created."""
    return app_dir(PathKind.CACHE, appname)


def state_dir(appname: str) -> Path:
    """Return ``~/.local/state/<appname>`` (or ``$XDG_STATE_HOME``), created."""
    return app_dir(PathKind.STATE, appname)


def app_file(kind: PathKind, appname: str, relative: str) -> Path:
    """Return a file path inside an application directory.

    Parent directories are created; the file itself is not. ``relative``
    may contain subdirectories but no ``..`` segments.

    Raises
    ------
    SparcliError
        When any argument is invalid or directory creation fails.
    """
    arena: list = []
    out = lib.sc_path_file(
        int(kind), cstr(arena, appname), cstr(arena, relative)
    )
    return _take_path(out, f"{appname}/{relative}")


def _take_path(out, described: str) -> Path:
    """Convert a C-heap path to :class:`Path`; raise on NULL."""
    if out == ffi.NULL:
        raise SparcliError(
            f"cannot resolve application path for {described!r} "
            "(invalid name or directory creation failed)"
        )
    try:
        return Path(ffi.string(out).decode())
    finally:
        lib.free(out)


class Pager:
    """Pipe sparcli output through a pager (``$PAGER`` / ``less -R``).

    Usable as a context manager: output written inside the ``with`` block
    is paged. When the output stream is not a terminal (pipes, captures,
    CI) the pager is skipped and output goes through unchanged, so the
    same code works in scripts.

        with sc.Pager():
            table.print()

    Parameters
    ----------
    command
        Pager command (split on whitespace, executed without a shell);
        ``None`` = ``$PAGER``, then ``less -R``.
    always
        Page even when the output stream is not a terminal.
    """

    __slots__ = ("_pager", "_arena", "exit_status")

    def __init__(self, command: str | None = None, always: bool = False) -> None:
        self._arena: list = []
        opts = ffi.new("ScPagerOpts *")
        opts.command = cstr(self._arena, command)
        opts.always = always
        self._pager = lib.sc_pager_begin(opts[0])

        #: The pager's exit status; set by :meth:`end` / context exit.
        self.exit_status: int | None = None

    def end(self) -> int:
        """End the session and return the pager's exit status (idempotent)."""
        if self._pager is not None:
            self.exit_status = int(lib.sc_pager_end(self._pager))
            self._pager = None
        return self.exit_status if self.exit_status is not None else 0

    def __enter__(self) -> "Pager":
        return self

    def __exit__(self, *exc) -> None:
        self.end()


class ErrorReport:
    """A structured error report rendered as a red alert panel.

    Carries a message, an optional cause chain, an optional hint and an
    exit code - the pretty replacement for ``print(..., file=sys.stderr)``
    + ``sys.exit(1)``. Builder methods are chainable.

        sc.ErrorReport("Config could not be loaded") \\
            .cause("file not found: ~/.config/app/config.toml") \\
            .hint("Run 'app init' to create a default config") \\
            .code(2) \\
            .die()                  # renders to stderr, raises SystemExit(2)
    """

    __slots__ = ("_p", "_finalizer", "__weakref__")

    def __init__(self, message: str) -> None:
        arena: list = []
        p = lib.sc_error_new(cstr(arena, message))
        if p == ffi.NULL:
            raise MemoryError("sc_error_new failed")
        self._p = p
        self._finalizer = weakref.finalize(self, lib.sc_error_free, p)

    def cause(self, cause: str) -> "ErrorReport":
        """Append one ``caused by:`` line. Returns ``self`` for chaining."""
        arena: list = []
        lib.sc_error_add_cause(self._p, cstr(arena, cause))
        return self

    def hint(self, hint: str) -> "ErrorReport":
        """Set the ``Hint:`` line. Returns ``self`` for chaining."""
        arena: list = []
        lib.sc_error_set_hint(self._p, cstr(arena, hint))
        return self

    def code(self, exit_code: int) -> "ErrorReport":
        """Set the exit code used by :meth:`die` (default 1)."""
        lib.sc_error_set_code(self._p, exit_code)
        return self

    @property
    def exit_code(self) -> int:
        """The configured exit code."""
        return int(lib.sc_error_code(self._p))

    def print(self) -> None:
        """Render to the current output stream (does not exit)."""
        lib.sc_error_print(self._p)

    def print_stderr(self) -> None:
        """Render to stderr (does not exit)."""
        lib.sc_error_print_stderr(self._p)

    def die(self) -> NoReturn:
        """Render to stderr and raise ``SystemExit`` with the exit code.

        Raises ``SystemExit`` (instead of calling the C ``exit()``) so
        Python cleanup, ``finally`` blocks and ``atexit`` handlers run.
        """
        self.print_stderr()
        raise SystemExit(self.exit_code)
