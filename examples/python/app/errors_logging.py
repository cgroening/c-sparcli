#!/usr/bin/env python3
"""Structured error reports and leveled logging.

    make run-example EX=python/app/errors_logging
"""

from __future__ import annotations

import sys

import sparcli as sc


def show_error_report() -> None:
    """Build an error with cause chain + hint and render it (no exit)."""
    # A real tool would end with .die() (renders to stderr, raises SystemExit).
    (sc.ErrorReport('Configuration could not be loaded')
        .cause('file not found: ~/.config/app/config.toml')
        .cause('no such file or directory')
        .hint("Run 'app init' to create a default config.")
        .code(2)
        .print())
    sc.println('')


def show_global_logger() -> None:
    """The global logger: stderr sink with a level filter."""
    sc.log_set_level(sc.LogLevel.DEBUG)

    sc.log_debug('cache warmed')
    sc.log_info('server listening on port 8080')
    sc.log_warning('disk usage at 85 %')
    sc.log_error('connection to db-01 lost')

    sc.log_reset()  # restore defaults for code that runs afterwards


def show_handle_logger() -> None:
    """A handle-based logger: own sinks and options, independent of global."""
    logger = sc.Logger(hide_timestamps=True, show_source=True, markup=True)
    logger.add_terminal(sys.stderr, sc.LogLevel.DEBUG)
    # logger.add_file('debug.log')

    # Messages are passed as data, never as a format string.
    logger.info('deployment [bold green]succeeded[/]')
    logger.warning('rollback plan not configured')


def main() -> None:
    show_error_report()
    show_global_logger()
    show_handle_logger()


if __name__ == '__main__':
    main()
