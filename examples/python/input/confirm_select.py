#!/usr/bin/env python3
"""Confirmation and single/multi selection.

    make run-example EX=python/input/confirm_select

Each prompt returns its value, None on cancel, and raises
SparcliInputUnavailable with no TTY.
"""

from __future__ import annotations

import sparcli as sc

LANGUAGES = ['C', 'C++', 'Rust', 'Python', 'Zig', 'Go']


def run_confirm() -> None:
    """Yes/no with custom labels and a default answer."""
    deploy = sc.confirm('Deploy to production?',
                        sc.ConfirmOpts(default_yes=False, yes_label='Ship it',
                                       no_label='Not yet',
                                       accent=sc.Color.GREEN))
    if deploy is not None:
        sc.markup.println('  [green]-> deploying[/]' if deploy
                          else '  [yellow]-> skipped[/]')


def run_single_select() -> None:
    """Single selection: run_one() returns the chosen index."""
    select = sc.Select(sc.SelectOpts(prompt='Primary language',
                                     accent=sc.Color.CYAN))
    for language in LANGUAGES:
        select.add(language)
    index = select.run_one()
    if index is not None:
        sc.println(f'  -> {LANGUAGES[index]}')


def run_multi_select() -> None:
    """Multi selection: run() returns the indices in display order."""
    select = sc.Select(sc.SelectOpts(prompt='Languages you use (Space toggles)',
                                     multi=True, max_visible=5,
                                     accent=sc.Color.MAGENTA))
    for language in LANGUAGES:
        select.add(language)
    select.set_checked(0, True)
    select.set_cursor(1)
    indices = select.run()
    if indices is not None:
        picked = [LANGUAGES[i] for i in indices]
        sc.println(f'  -> {picked}')


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    run_confirm()
    run_single_select()
    run_multi_select()


if __name__ == '__main__':
    main()
