#!/usr/bin/env python3
"""Nested lists and tree views.

    make run-example EX=python/output/list_tree
"""

from __future__ import annotations

import sparcli as sc


def print_lists() -> None:
    """Markers, nesting and a styled marker."""
    steps = sc.List(sc.ListOpts(marker=sc.ListMarker.NUMBER,
                                marker_style=sc.Style.bold(sc.Color.CYAN)))
    steps.add('Check out the repository')
    build = steps.add('Build the project')
    steps.add('Run the test suite')

    # A sub-list is owned by the parent item.
    sub = build.sub(sc.ListOpts(marker=sc.ListMarker.ALPHA_LOWER, indent=2))
    sub.add('make')
    sub.add('make qa')
    steps.print()
    sc.println('')

    notes = sc.List(sc.ListOpts(marker=sc.ListMarker.BULLET, bullet='→'))
    notes.add('Long items are word-wrapped and continuation lines align '
              'under the first word of the item.')
    notes.add(sc.Text.from_markup('Items can be [bold green]rich text[/].'))
    notes.print()


def print_tree() -> None:
    """A file-system-like tree; add() returns a node usable as a parent."""
    tree = sc.Tree(sc.TreeOpts(type=sc.BorderType.SINGLE,
                               connector_color=sc.Color.CYAN))
    dir_style = sc.Style.bold(sc.Color.BLUE)

    root = tree.add('sparcli/', style=dir_style)
    src = tree.add('src/', root, style=dir_style)
    tree.add('core/', src, style=dir_style)
    tree.add('output/', src, style=dir_style)
    docs = tree.add('docs/', root, style=dir_style)
    tree.add('examples.md', docs, prefix='▪ ',
             prefix_style=sc.Style(fg=sc.Color.GREEN))
    tree.add('Makefile', root)
    tree.print()


def main() -> None:
    print_lists()
    sc.println('')
    print_tree()


if __name__ == '__main__':
    main()
