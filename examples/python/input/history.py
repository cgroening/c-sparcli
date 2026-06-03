#!/usr/bin/env python3
"""Input history with Up/Down recall and persistence.

    make run-example EX=python/input/history

As a context manager, History saves on exit, so a second run recalls lines
from the first.
"""

from __future__ import annotations

import sparcli as sc

APP_NAME = 'sparcli-history-example'


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return

    # app=... persists to ~/.local/state/<app>/history; loaded on creation,
    # saved on context-manager exit.
    with sc.History(app=APP_NAME, max_entries=100) as history:
        if len(history) > 0:
            sc.println(f'Loaded {len(history)} line(s) from the previous run; '
                       'press Up to recall them.')

        # Submitted lines are auto-added (empty + duplicates skipped).
        for _ in range(3):
            line = sc.text_input('>', sc.TextInputOpts(
                history=history, placeholder='type, or press Up'))
            if line is None:
                break

        sc.println('\nHistory contents:')
        for i, entry in enumerate(history.entries()):
            sc.println(f'  {i:2}: {entry}')


if __name__ == '__main__':
    main()
