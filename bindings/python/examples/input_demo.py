#!/usr/bin/env python3
"""Input-only demo: confirm, text/password/number/textarea, select, fuzzy, date.

Run in a real terminal:

    python examples/input_demo.py
"""

from __future__ import annotations

import sys

import sparcli as sc


def main() -> int:
    if not sc.input_available():
        sc.println("No interactive terminal – run this directly in a shell.",
                   sc.Style.dim())
        return 1

    sc.rule("sparcli – input demo", sc.RuleOpts(type=sc.BorderType.DOUBLE))

    if sc.confirm("Proceed?", sc.ConfirmOpts(default_yes=True)) is None:
        sc.println("cancelled", sc.Style.dim())
        return 0

    name = sc.text_input(
        "Your name",
        sc.TextInputOpts(placeholder="Ada", boxed=True, width=40))
    sc.println(f"  -> {name!r}", sc.Style.dim())

    # A custom Ctrl-R shortcut (callback mode) that beeps a message.
    shortcuts = sc.Shortcuts().on_callback(
        sc.key_ctrl("r"),
        lambda: sc.println("  (reload pressed)", sc.Style.dim()),
        name="reload")
    note = sc.text_input(
        "Note (Ctrl-R reload · Ctrl-G editor)",
        sc.TextInputOpts(external_editor=True, shortcuts=shortcuts))
    sc.println(f"  -> {note!r}", sc.Style.dim())

    pw = sc.password_input("Password", sc.PasswordOpts())
    sc.println(f"  -> {len(pw) if pw else 0} chars", sc.Style.dim())

    qty = sc.number_input("Quantity",
                          sc.NumberOpts(min=0, max=10, step=1, initial=3))
    sc.println(f"  -> {qty}", sc.Style.dim())

    color = sc.Select(sc.SelectOpts(prompt="Favourite color"))
    color.add("red").add("green").add("blue")
    sc.println(f"  -> index {color.run_one()}", sc.Style.dim())

    langs = sc.Select(sc.SelectOpts(prompt="Languages you use", multi=True))
    for lang in ("C", "Python", "Rust", "Go", "Zig"):
        langs.add(lang)
    sc.println(f"  -> {langs.run()}", sc.Style.dim())

    fz = sc.Fuzzy(sc.FuzzyOpts(prompt="Pick a fruit"))
    fruits = ("apple", "apricot", "banana", "cherry", "date", "fig", "grape")
    for fruit in fruits:
        fz.add(fruit)
    sc.println(f"  -> index {fz.run()}", sc.Style.dim())

    d = sc.datepicker(opts=sc.DatePickerOpts(prompt="Pick a date"))
    sc.println(f"  -> {d}", sc.Style.dim())
    return 0


if __name__ == "__main__":
    sys.exit(main())
