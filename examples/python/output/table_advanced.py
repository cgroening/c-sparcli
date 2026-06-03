#!/usr/bin/env python3
"""Tables: colspan/rowspan, stripes, per-row background and markup cells.

    make run-example EX=python/output/table_advanced
"""

from __future__ import annotations

import sparcli as sc


def print_spans_table() -> None:
    """Colspan/rowspan via Cell; covered slots use Cell.skip / Cell.row_skip."""
    table = sc.Table()
    table.column('Quarter').column('Region') \
         .column('Revenue', sc.ColOpts(halign=sc.Align.RIGHT))

    # Rowspan: "Q1" covers two rows; the covered slot uses Cell.row_skip().
    table.row([sc.Cell('Q1', rowspan=2), 'Europe', '1.2 M'])
    table.row([sc.Cell.row_skip(), 'Americas', '2.4 M'])
    # Colspan: one cell spanning the last two columns.
    table.row(['Q2', sc.Cell('no data yet', colspan=2, halign=sc.Align.CENTER),
               sc.Cell.skip()])

    table.print(sc.TableOpts(border=sc.BorderType.SINGLE, header_row=True,
                             header_style=sc.Style.bold(), title='Spans'))


def print_styled_table() -> None:
    """Stripes, a header column and markup cells; per-row background."""
    table = sc.Table()
    table.column('Check').column('Result') \
         .column('Duration', sc.ColOpts(halign=sc.Align.RIGHT))

    table.row(['build', sc.Cell.markup('[green]✔ passed[/]'), '41 s'])
    table.row(['lint', sc.Cell.markup('[green]✔ passed[/]'), '3 s'])
    # Per-row background highlights the failing check.
    table.row(['fuzz', sc.Cell.markup('[red]✖ failed[/]'), '122 s'],
              bg=sc.Color.rgb(60, 30, 30))

    table.print(sc.TableOpts(border=sc.BorderType.ROUNDED, header_col=True,
                             header_col_bg=sc.Color.rgb(40, 40, 60),
                             striped=True, stripe_bg=sc.Color.rgb(30, 30, 30),
                             cell_pad=sc.Edges(right=1, left=1)))


def main() -> None:
    print_spans_table()
    sc.println('')
    print_styled_table()


if __name__ == '__main__':
    main()
