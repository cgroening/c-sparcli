#!/usr/bin/env python3
"""Numeric input, exact Decimal input, calculator mode and the evaluator.

    make run-example EX=python/input/number_calc
"""

from __future__ import annotations

import sparcli as sc


def run_stepper() -> None:
    """Up/Down steps the value; the result is clamped to [min, max]."""
    quantity = sc.number_input('Quantity', sc.NumberOpts(
        initial=1, min=1, max=99, step=1, decimals=0, boxed=True, width=28))
    if quantity is not None:
        sc.println(f'  -> {quantity:.0f}')


def run_decimal_calculator() -> None:
    """Exact Decimal via the calculator; never round-trips through float."""
    amount = sc.decimal_input('Amount (try =1/3)', sc.NumberOpts(
        decimals=2, decimal_sep=',', start_empty=True, calculator=True))
    if amount is not None:
        # `amount` is a decimal.Decimal, safe for money.
        sc.println(f'  -> exact Decimal: {amount}')


def run_pure_evaluator() -> None:
    """calc_eval is the pure evaluator behind calculator mode."""
    result = sc.calc_eval('(2,5 + 1,5) * 4 - 1')
    if result is not None:
        sc.println(f'  (2,5 + 1,5) * 4 - 1 = {result}', sc.Style.dim())


def main() -> None:
    run_pure_evaluator()  # pure, works without a terminal

    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    run_stepper()
    run_decimal_calculator()


if __name__ == '__main__':
    main()
