#!/usr/bin/env python3
"""Rich-style inline markup.

    make run-example EX=python/output/markup
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    sc.markup.println('[bold]Bold[/], [italic]italic[/], [u]underline[/] '
                      'and [dim]dim[/].')
    sc.markup.println('[red]Named colors[/], [on blue] backgrounds [/] and '
                      '[rgb(255,165,0)]24-bit RGB[/].')
    sc.markup.println('[bold green on white] Combined in one tag [/]')
    sc.markup.println('[bold]Bold [red]and red[/] - still bold[/].')

    # Literal brackets, inline code, OSC-8 hyperlink.
    sc.markup.println('Escape with [[ to print a literal bracket.')
    sc.markup.println('Inline code: run `make qa` before committing.')
    sc.markup.println('Read the [link=https://github.com]docs[/link].')
    sc.println('')

    # Parse into a Text for any widget that accepts rich text.
    content = sc.Text.from_markup(
        '[bold]sparcli[/] renders [green]markup[/] inside panels,\n'
        'tables, lists and every other widget.')
    sc.panel(content, sc.PanelOpts(title='markup',
                                   border=sc.BorderStyle(sc.BorderType.ROUNDED)))

    # Custom code-span style via the code_style keyword.
    sc.markup.println(
        '[blink]Unknown tags[/blink] stay verbatim; `code` is yellow here.',
        code_style=sc.Style.bold(sc.Color.YELLOW))


if __name__ == '__main__':
    main()
