#!/usr/bin/env python3
"""Horizontal rules and key-value lists.

    make run-example EX=python/output/rule_kv
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    # Plain divider, then a titled one.
    sc.rule(opts=sc.RuleOpts(type=sc.BorderType.SINGLE))
    sc.rule('Results', sc.RuleOpts(type=sc.BorderType.DOUBLE,
                                   color=sc.Color.CYAN,
                                   title_style=sc.Style.bold(sc.Color.CYAN),
                                   title_align=sc.Align.CENTER))

    # Fixed width, left placement.
    sc.rule('left', sc.RuleOpts(type=sc.BorderType.THICK, width=40,
                                halign=sc.Align.LEFT,
                                title_align=sc.Align.LEFT))
    sc.println('')

    # Key-value list with a styled key column.
    info = sc.Kv(sc.KvOpts(key_style=sc.Style.bold(sc.Color.CYAN)))
    info.add('Version', '1.2.0').add('License', 'MIT') \
        .add('Platform', 'macOS / Linux')
    info.print()
    sc.println('')

    # Fixed key width and wrapped values.
    details = sc.Kv(sc.KvOpts(key_width=12, wrap_val=True,
                              val_style=sc.Style.dim()))
    details.add('Description',
                'A C11 library for styled terminal output: panels, tables, '
                'rules, lists, trees and interactive input widgets.')
    details.add('Homepage', 'https://github.com/example/sparcli')
    details.print()


if __name__ == '__main__':
    main()
