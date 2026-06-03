#!/usr/bin/env python3
"""Multi-line entry and the external-editor hook.

    make run-example EX=python/input/textarea_editor

Keys: Enter inserts a newline, Ctrl-D submits, Ctrl-G opens $EDITOR.
"""

from __future__ import annotations

import sparcli as sc


def run_textarea() -> None:
    """Boxed multi-line editor with soft-wrapped long lines."""
    notes = sc.textarea('Release notes (Ctrl-D submits)', sc.TextareaOpts(
        placeholder='What changed?', boxed=True,
        border=sc.BorderStyle(sc.BorderType.ROUNDED), width=52))
    if notes is not None:
        sc.println(f'  -> {len(notes.splitlines())} line(s), {len(notes)} bytes')


def run_editor_input() -> None:
    """External editor: Ctrl-G opens $EDITOR; text_input collapses newlines."""
    message = sc.text_input('Commit message (Ctrl-G opens the editor)',
                            sc.TextInputOpts(external_editor=True))
    if message is not None:
        sc.println(f'  -> {message!r}')


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    run_textarea()
    run_editor_input()


if __name__ == '__main__':
    main()
