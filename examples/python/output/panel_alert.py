#!/usr/bin/env python3
"""Bordered panels and the alert presets.

    make run-example EX=python/output/panel_alert
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    # Minimal panel.
    sc.panel('Hello from a panel.',
             sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE)))
    sc.println('')

    # Border, styled title (a Title object), padding, fixed width.
    sc.panel('Borders: single, double, rounded, thick.\n'
             'Multi-line content wraps into the frame.',
             sc.PanelOpts(
                 border=sc.BorderStyle(sc.BorderType.DOUBLE, sc.Color.CYAN),
                 title=sc.Title(text='Settings',
                                style=sc.Style.bold(sc.Color.CYAN),
                                halign=sc.Align.LEFT, pad=1),
                 padding=sc.Edges(1, 2, 1, 2),
                 width=56))
    sc.println('')

    # Full width, centered content.
    sc.panel('This panel spans the whole terminal.',
             sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.ROUNDED),
                          full_width=True, content_align=sc.Align.CENTER))
    sc.println('')

    # Alert presets: panel wrappers with a preset icon + color.
    sc.alert.info('Service started on port 8080.')
    sc.alert.debug('Cache warmed in 132 ms.')
    sc.alert.warning('Disk usage at 85 %.')
    sc.alert.error('Connection to database lost.\nRetrying in 5 s.')
    sc.alert.success('All 128 tests passed.')


if __name__ == '__main__':
    main()
