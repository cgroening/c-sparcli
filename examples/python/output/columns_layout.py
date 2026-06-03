#!/usr/bin/env python3
"""Side-by-side columns, capture/vstack composition, pad and align.

    make run-example EX=python/output/columns_layout
"""

from __future__ import annotations

import sparcli as sc


def print_basic_columns() -> None:
    """Columns with a separator and per-column alignment."""
    cols = sc.Columns(sc.ColumnsOpts(
        gap=2, sep=sc.BorderStyle(sc.BorderType.SINGLE, sc.Color.CYAN)))
    cols.add_str('Left column\nwith two lines.')
    cols.add_panel('Boxed middle column.',
                   sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.ROUNDED)),
                   sc.ColItem(fixed_w=26))
    cols.add_text(sc.Text.from_markup('[bold]Right[/]\n[dim]column[/]'),
                  sc.ColItem(halign=sc.Align.RIGHT))
    cols.print()


def print_composed_layout() -> None:
    """Capture widgets, stack them vertically, place the stack in a column."""
    lst = sc.List(sc.ListOpts(marker=sc.ListMarker.NUMBER))
    lst.add('capture widgets')
    lst.add('stack them with vstack')
    lst.add('drop the stack into a column')

    r_list = sc.capture.list(lst)
    r_rule = sc.capture.rule('composed',
                             sc.RuleOpts(type=sc.BorderType.SINGLE, width=32))
    stack = sc.vstack([r_list, r_rule], gap=1)

    cols = sc.Columns(sc.ColumnsOpts(gap=4))
    cols.add_rendered(stack)
    cols.add_panel('Columns copy captured\ncontent, so the sources\n'
                   'can be freed right away.',
                   sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE)))
    cols.print()


def print_pad_and_align() -> None:
    """Pad and align a captured block, then the one-step string helpers."""
    block = sc.capture.panel('padded + centered',
                             sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.THICK)))
    block.pad(sc.PadOpts(top=1, left=4))
    block.align(sc.Align.CENTER)  # 0 = terminal width

    sc.pad_str('indented by eight columns', sc.PadOpts(left=8))
    sc.align_str('centered heading', sc.Align.CENTER)


def main() -> None:
    print_basic_columns()
    sc.println('')
    print_composed_layout()
    sc.println('')
    print_pad_and_align()


if __name__ == '__main__':
    main()
