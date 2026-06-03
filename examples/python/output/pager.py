#!/usr/bin/env python3
"""Route long output through $PAGER / `less -R`.

    make run-example EX=python/output/pager

In a terminal the table opens in the pager (press `q`). Off a TTY the pager
is skipped and the output prints directly.
"""

from __future__ import annotations

import sparcli as sc

ROW_COUNT = 100


def main() -> None:
    # Everything inside the block is paged; exit_status is set on exit.
    with sc.Pager() as pager:
        sc.rule('100 rows', sc.RuleOpts(type=sc.BorderType.DOUBLE))

        table = sc.Table()
        table.column('#', sc.ColOpts(halign=sc.Align.RIGHT)) \
             .column('Square', sc.ColOpts(halign=sc.Align.RIGHT))
        for i in range(1, ROW_COUNT + 1):
            table.row([str(i), str(i * i)])
        table.print(sc.TableOpts(border=sc.BorderType.SINGLE, header_row=True,
                                 header_style=sc.Style.bold()))

    print(f'pager exit status: {pager.exit_status}')


if __name__ == '__main__':
    main()
