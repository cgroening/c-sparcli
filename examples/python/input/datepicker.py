#!/usr/bin/env python3
"""Calendar month grid for picking a date.

    make run-example EX=python/input/datepicker

Keys: arrows move by day/week, PageUp/Down change the month, Enter picks.
"""

from __future__ import annotations

import datetime

import sparcli as sc


def main() -> None:
    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return

    # `None` seeds the picker with today's date; returns a datetime.date.
    delivery = sc.datepicker(None, sc.DatePickerOpts(
        prompt='Delivery date', week_start=sc.WeekStart.MONDAY,
        accent=sc.Color.CYAN))
    if delivery is not None:
        sc.println(f'  -> {delivery:%A, %d %B %Y}')

    # Pre-seed a specific start date.
    start = sc.datepicker(datetime.date(2027, 1, 1), sc.DatePickerOpts(
        prompt='Starts at 2027-01-01 (Sunday weeks)',
        week_start=sc.WeekStart.SUNDAY))
    if start is not None:
        sc.println(f'  -> {start:%Y-%m-%d}')


if __name__ == '__main__':
    main()
