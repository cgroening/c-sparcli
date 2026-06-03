#!/usr/bin/env python3
"""Tables: columns, header row, alignment, borders.

    make run-example EX=python/output/table_basic
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    # The data is built once; rendering options are passed at print time, so
    # the same table can be printed in different styles.
    table = sc.Table()
    table.column('City') \
         .column('Country') \
         .column('Pop. (M)', sc.ColOpts(halign=sc.Align.RIGHT))
    table.row(['Tokyo', 'Japan', '37.4'])
    table.row(['Delhi', 'India', '32.9'])
    table.row(['Oslo', 'Norway', '1.1'])
    table.footer_row(['Total', '3 countries', '71.4'])

    # 1. Default look: single border, bold header row.
    table.print(sc.TableOpts(border=sc.BorderType.SINGLE, header_row=True,
                             header_style=sc.Style.bold(),
                             footer_style=sc.Style.dim()))
    sc.println('')

    # 2. Same data, rounded border, title, padding.
    table.print(sc.TableOpts(
        border=sc.TableBorder(type=sc.BorderType.ROUNDED,
                              outer_color=sc.Color.CYAN,
                              inner_color=sc.Color.BLUE),
        header_row=True, header_style=sc.Style.bold(sc.Color.CYAN),
        footer_style=sc.Style.bold(), title='World cities',
        cell_pad=sc.Edges(right=1, left=1)))
    sc.println('')

    # 3. Per-column style: bold cyan first column.
    styled = sc.Table()
    styled.column('Key', sc.ColOpts(style=sc.Style.bold(sc.Color.CYAN))) \
          .column('Value')
    styled.row(['version', '1.2.0'])
    styled.row(['license', 'MIT'])
    styled.print(sc.TableOpts(border=sc.BorderType.SINGLE, header_row=True))


if __name__ == '__main__':
    main()
