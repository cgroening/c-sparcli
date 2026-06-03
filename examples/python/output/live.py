#!/usr/bin/env python3
"""Live display: re-render a composed frame in place (dashboard).

    make run-example EX=python/output/live

Off a TTY (piped) only the final frame is printed.
"""

from __future__ import annotations

import time

import sparcli as sc

TOTAL_STEPS = 30


def build_frame(step: int) -> sc.Rendered:
    """Compose one dashboard frame: a status table plus a progress line."""
    percent = step * 100 // TOTAL_STEPS

    table = sc.Table()
    table.column('Job', sc.ColOpts(min_width=10)) \
         .column('Status', sc.ColOpts(min_width=14))
    table.row(['build', 'done' if percent >= 50 else 'running'])
    table.row(['package',
               'done' if percent >= 100
               else 'running' if percent >= 50 else 'waiting'])

    table_part = sc.capture.table(
        table, sc.TableOpts(border=sc.TableBorder(type=sc.BorderType.ROUNDED,
                                                  outer_color=sc.Color.CYAN),
                            header_row=True, header_style=sc.Style.bold()))
    line_part = sc.capture.string(f'progress: {percent} %')
    return sc.vstack([table_part, line_part], gap=1)


def main() -> None:
    sc.markup.println('[bold]Live dashboard[/] [dim](redraws in place)[/]\n')

    with sc.Live() as live:
        for step in range(TOTAL_STEPS + 1):
            live.update(build_frame(step))
            time.sleep(0.06)

    sc.markup.println('\n[green]✔[/] Frame above was redrawn in place '
                      'on every update.')

    # Fullscreen variant: alt_screen takes over the whole terminal and
    # restores the previous screen on exit (a no-op off a TTY).
    sc.markup.println('[dim]Next: the same dashboard on the alternate '
                      'screen...[/]')
    time.sleep(1)
    with sc.Live(alt_screen=True) as full:
        for step in range(TOTAL_STEPS + 1):
            full.update(build_frame(step))
            time.sleep(0.04)
    sc.markup.println('[green]✔[/] The previous screen was restored.')


if __name__ == '__main__':
    main()
