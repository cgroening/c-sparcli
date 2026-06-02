"""Application-framework helpers: XDG base directories and pager."""

from __future__ import annotations

from pathlib import Path

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
