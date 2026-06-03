#!/usr/bin/env python3
"""Styled text, rich Text, links and badges.

    make run-example EX=python/output/text_styles
"""

from __future__ import annotations

import sparcli as sc


def main() -> None:
    # Plain styled lines; Style.bold()/dim() are convenience constructors.
    sc.println('Bold red on the default background',
               sc.Style.bold(sc.Color.RED))
    sc.println('Italic + underline in 24-bit orange',
               sc.Style(attr=sc.Attr.ITALIC | sc.Attr.UNDERLINE,
                        fg=sc.Color.rgb(255, 165, 0)))
    sc.println('Dim text on a blue background',
               sc.Style.dim(bg=sc.Color.BLUE))
    sc.println('')

    # Rich Text: chained spans, each with its own style.
    line = sc.Text()
    line.append('status: ') \
        .append('passed', sc.Style.bold(sc.Color.GREEN)) \
        .append('  (3 warnings)', sc.Style.dim())
    line.print()
    sc.println(f'\nvisible width: {line.visible_width} columns\n')

    # OSC-8 hyperlink (clickable in supporting terminals).
    link_line = sc.Text()
    link_line.append('Docs: ') \
             .append_link('sparcli on GitHub',
                          'https://github.com/example/sparcli',
                          sc.Style(fg=sc.Color.CYAN))
    link_line.print()
    sc.println('')

    # Inline badges.
    sc.badge('PASS', sc.BadgeOpts(text_style=sc.Style.bold(sc.Color.GREEN)))
    sc.print_(' ')
    sc.badge('v1.2.0', sc.BadgeOpts(left_cap='(', right_cap=')', pad=1,
                                    text_style=sc.Style(fg=sc.Color.MAGENTA)))
    sc.println('\n')

    # Utilities return plain strings.
    plain = sc.strip_ansi('\x1b[1;32mgreen bold\x1b[0m')
    cut = sc.truncate('A sentence that is far too long', 16, '…')
    sc.println(f'stripped:  "{plain}"')
    sc.println(f'truncated: "{cut}"')

    # ANSI trust boundary: user strings are sanitized by default, so raw escape
    # codes in untrusted input cannot inject styling. Opt in per widget with
    # ansi=... (or process-wide via sc.set_allow_ansi) when input is trusted.
    sc.println('')
    pre_colored = '\x1b[31mpre-colored\x1b[0m input'
    sc.panel(pre_colored,
             sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE),
                          title='sanitized (default)'))
    sc.panel(pre_colored,
             sc.PanelOpts(border=sc.BorderStyle(sc.BorderType.SINGLE),
                          title='ansi allowed', ansi=sc.AnsiMode.ALLOW))


if __name__ == '__main__':
    main()
