"""Logging: leveled, colored terminal output + plain-text file sinks.

The :class:`Logger` class wraps a handle-based C logger; the module-level
``log_*`` functions drive the process-wide global logger (a colored stderr
sink at ``LogLevel.INFO`` plus optional file sinks).

Messages are always passed to C as *data* (via a ``"%s"`` format), never as
a printf format string, so ``%`` characters in messages are safe.
"""

from __future__ import annotations

import os
import weakref

from ._ffi import cstr, ffi, lib
from .enums import LogLevel


def _emit(call, level: LogLevel, message: str) -> None:
    """Call a C log function with ``message`` passed as data ("%s")."""
    arena: list = []
    # Variadic args must be explicit cdata objects for cffi
    call(int(level), ffi.NULL, 0, b"%s", cstr(arena, message))


class Logger:
    """A handle-based logger with its own sinks and format options.

    Use it for a subsystem that must not touch the process-wide logger;
    for normal application logging the module-level ``sc.log_info(...)``
    functions are simpler.

        logger = sc.Logger(hide_timestamps=True)
        logger.add_file("subsystem.log", sc.LogLevel.DEBUG)
        logger.info("subsystem started")

    Not thread-safe: use one logger per thread or lock externally.
    """

    __slots__ = ("_p", "_finalizer", "_terminal_files", "__weakref__")

    def __init__(
        self,
        level: LogLevel = LogLevel.DEBUG,
        *,
        hide_timestamps: bool = False,
        show_source: bool = False,
        markup: bool = False,
    ) -> None:
        opts = ffi.new("ScLoggerOpts *")
        opts.level = int(level)
        opts.hide_timestamps = hide_timestamps
        opts.show_source = show_source
        opts.markup = markup
        p = lib.sc_logger_new(opts[0])
        if p == ffi.NULL:
            raise MemoryError("sc_logger_new failed")
        self._p = p
        self._terminal_files: list = []
        self._finalizer = weakref.finalize(self, lib.sc_logger_free, p)

    def add_terminal(self, target, min_level: LogLevel = LogLevel.DEBUG) -> "Logger":
        """Add a colored terminal sink writing to ``target``.

        ``target`` must expose a real OS file descriptor (``sys.stderr``,
        an open file, ...). The descriptor is duplicated, so the original
        stays open. Returns ``self`` for chaining.
        """
        fd = os.dup(target.fileno())
        stream = lib.fdopen(fd, b"w")
        if stream == ffi.NULL:
            os.close(fd)
            raise OSError("fdopen failed")
        # Keep the FILE* alive for the lifetime of the logger
        self._terminal_files.append(stream)
        lib.sc_logger_add_terminal(self._p, stream, int(min_level))
        return self

    def add_file(self, path, min_level: LogLevel = LogLevel.DEBUG) -> bool:
        """Add a plain-text file sink (append mode). Returns success."""
        arena: list = []
        return bool(lib.sc_logger_add_file(self._p, cstr(arena, str(path)), int(min_level)))

    def set_level(self, level: LogLevel) -> None:
        """Set the logger-wide minimum level."""
        lib.sc_logger_set_level(self._p, int(level))

    def log(self, level: LogLevel, message: str) -> None:
        """Emit one record (``message`` is data, not a format string)."""
        arena: list = []
        lib.sc_logger_log(
            self._p, int(level), ffi.NULL, 0, b"%s", cstr(arena, message)
        )

    def debug(self, message: str) -> None:
        """Emit a ``DEBUG`` record."""
        self.log(LogLevel.DEBUG, message)

    def info(self, message: str) -> None:
        """Emit an ``INFO`` record."""
        self.log(LogLevel.INFO, message)

    def warning(self, message: str) -> None:
        """Emit a ``WARN`` record."""
        self.log(LogLevel.WARN, message)

    def error(self, message: str) -> None:
        """Emit an ``ERROR`` record."""
        self.log(LogLevel.ERROR, message)

    def close(self) -> None:
        """Free the underlying C logger now (idempotent)."""
        self._finalizer()


# ── Global logger ──────────────────────────────────────────────────────────

def log_set_level(level: LogLevel) -> None:
    """Set the global stderr-sink level (default ``LogLevel.INFO``)."""
    lib.sc_log_set_level(int(level))


def log_level() -> LogLevel:
    """Return the global stderr-sink level."""
    return LogLevel(lib.sc_log_level())


def log_add_file(path, min_level: LogLevel = LogLevel.DEBUG) -> bool:
    """Add a plain-text file sink to the global logger. Returns success.

    Configure before starting threads (configuration is not thread-safe).
    """
    arena: list = []
    return bool(lib.sc_log_add_file(cstr(arena, str(path)), int(min_level)))


def log_hide_timestamps(hide: bool = True) -> None:
    """Hide (or show) the timestamp prefix of the global logger."""
    opts = ffi.new("ScLoggerOpts *")
    opts.hide_timestamps = hide
    lib.sc_log_set_opts(opts[0])


def log_reset() -> None:
    """Close global file sinks and restore the default configuration."""
    lib.sc_log_reset()


def log_debug(message: str) -> None:
    """Emit a ``DEBUG`` record on the global logger."""
    _emit(lib.sc_log_log, LogLevel.DEBUG, message)


def log_info(message: str) -> None:
    """Emit an ``INFO`` record on the global logger."""
    _emit(lib.sc_log_log, LogLevel.INFO, message)


def log_warning(message: str) -> None:
    """Emit a ``WARN`` record on the global logger."""
    _emit(lib.sc_log_log, LogLevel.WARN, message)


def log_error(message: str) -> None:
    """Emit an ``ERROR`` record on the global logger."""
    _emit(lib.sc_log_log, LogLevel.ERROR, message)
