#!/usr/bin/env python3
"""Per-shortcut footer/help metadata and the auto-built keyboard help screen.

    make run-example EX=python/input/shortcuts_help

The picker's footer shows the visible shortcuts (F1 help, Ctrl-N new); Ctrl-X
delete is bound but hidden from the footer (in_footer=False). Press F1 to open
a full-screen, filterable help screen built from the same Shortcuts set, with
section() headers and a help_row() documenting a built-in select key.
"""

from __future__ import annotations

import sparcli as sc

ACT_HELP, ACT_NEW, ACT_DELETE = 1, 2, 3
FRUIT = ['Apples', 'Bananas', 'Cherries']


def build_shortcuts() -> sc.Shortcuts:
    """One builder drives both the footer and the help screen."""
    return (sc.Shortcuts()
            .section('Actions')
            .on_return(sc.key_ctrl('n'), ACT_NEW, 'new',
                       help='create a new item')
            .on_return(sc.key_ctrl('x'), ACT_DELETE, 'delete',
                       help='delete the highlighted item', in_footer=False)
            .section('Other')
            .on_return(sc.key_fn(1), ACT_HELP, 'help',
                       help='show this keyboard help')
            .section('Navigation')
            .help_row('↑/↓ or j/k', 'move the cursor')
            .help_row('↵', 'pick the highlighted item')
            .help_row('Esc', 'cancel'))


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return

    shortcuts = build_shortcuts()
    while True:
        select = sc.Select(sc.SelectOpts(prompt='Fruit', shortcuts=shortcuts))
        for fruit in FRUIT:
            select.add(fruit)
        index = select.run_one()
        if index is None:
            break                       # Esc / Ctrl-C
        if shortcuts.fired() == ACT_HELP:
            sc.show_shortcuts(shortcuts,
                              sc.ShortcutHelpOpts(title='Fruit picker · keys'))
            continue                    # reopen the picker
        sc.println(f'  -> {select.label(index)!r}')
        break


if __name__ == '__main__':
    main()
