#!/usr/bin/env python3
"""Custom keyboard shortcuts on a widget and the process-wide input theme.

    make run-example EX=python/input/shortcuts_theme

Keys: F2 archives (closes, reported via fired()), Ctrl-X deletes the
highlighted item in place, Enter picks.
"""

from __future__ import annotations

import sparcli as sc

ACTION_ARCHIVE = 1
NOTES = ['Meeting notes', 'Shopping list', 'Project ideas', 'Travel plans']


def apply_theme() -> None:
    """Process-wide defaults inherited by every widget for unset options."""
    sc.set_theme(sc.Theme(accent=sc.Color.MAGENTA,
                          prompt_style=sc.Style.bold(sc.Color.MAGENTA),
                          cursor_marker='▶ ', marker='  ',
                          hint_layout=sc.HintLayout.INLINE))


def run_select_with_shortcuts() -> None:
    """A select with a RETURN shortcut (F2) and a CALLBACK shortcut (Ctrl-X)."""
    select = sc.Select(sc.SelectOpts(prompt='Pick a note'))
    for note in NOTES:
        select.add(note)

    def delete_highlighted() -> bool:
        # Runs in place; returning True (or None) keeps the prompt open.
        select.remove(select.cursor())
        return True

    shortcuts = (sc.Shortcuts()
                 .on_return(sc.key_fn(2), ACTION_ARCHIVE, name='archive')
                 .on_callback(sc.key_ctrl('x'), delete_highlighted,
                              name='delete'))

    # Re-create the select with the shortcuts wired in (opts are read at run).
    select = sc.Select(sc.SelectOpts(prompt='Pick a note',
                                     shortcuts=shortcuts))
    for note in NOTES:
        select.add(note)

    index = select.run_one()
    if index is None:
        return
    label = select.label(index)
    if shortcuts.fired() == ACTION_ARCHIVE:
        sc.println(f'  -> archive {label!r}')
    else:
        sc.println(f'  -> picked {label!r}')


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    apply_theme()
    try:
        run_select_with_shortcuts()
    finally:
        sc.set_theme(None)  # restore the built-in defaults


if __name__ == '__main__':
    main()
