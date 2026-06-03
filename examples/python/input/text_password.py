#!/usr/bin/env python3
"""Text input (validation, filters, autocomplete, boxed) and masked password.

    make run-example EX=python/input/text_password
"""

from __future__ import annotations

import sparcli as sc


def validate_not_empty(value: str) -> str | None:
    """Return an error message to keep the prompt open, else None."""
    return 'Please enter a value' if value == '' else None


def run_plain_input() -> None:
    """Placeholder + a Python validate callback."""
    name = sc.text_input('Project name',
                         sc.TextInputOpts(placeholder='my-project',
                                          validate=validate_not_empty))
    if name is not None:
        sc.println(f'  -> {name!r}')


def run_autocomplete_input() -> None:
    """Suggestions in a fuzzy dropdown plus a built-in no-space filter."""
    cmd = sc.text_input('Git command', sc.TextInputOpts(
        char_filter=sc.filter_no_space,
        suggestions=['commit', 'checkout', 'cherry-pick', 'clone', 'config'],
        suggest=sc.SuggestOpts(mode=sc.SuggestMode.DROPDOWN,
                               match=sc.SuggestMatch.FUZZY)))
    if cmd is not None:
        sc.println(f'  -> {cmd!r}')


def run_boxed_input() -> None:
    """Boxed mode: panel frame, character counter, length limit."""
    title = sc.text_input('Title', sc.TextInputOpts(
        placeholder='Short and descriptive', max_chars=32,
        boxed=True, border=sc.BorderStyle(sc.BorderType.ROUNDED), width=44))
    if title is not None:
        sc.println(f'  -> {title!r}')


def run_password() -> None:
    """Masked input; the value is never echoed."""
    secret = sc.password_input('Passphrase', sc.PasswordOpts(mask='•',
                                                             max_chars=64))
    if secret is not None:
        sc.println(f'  -> captured {len(secret)} chars (not echoed)')


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    run_plain_input()
    run_autocomplete_input()
    run_boxed_input()
    run_password()


if __name__ == '__main__':
    main()
