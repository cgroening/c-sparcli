#!/usr/bin/env python3
"""Fuzzy finder over a list or a table, plus the pure match scorer.

    make run-example EX=python/input/fuzzy

Type to filter, arrows move the highlight, Enter picks, Esc cancels.
"""

from __future__ import annotations

import sparcli as sc


def run_list_finder() -> None:
    """List mode: the returned index is the original add order."""
    branches = ['main', 'develop', 'feature/fuzzy-finder',
                'feature/live-display', 'fix/table-wrap', 'release/1.2']
    # Rounded frame + dark content background: result rows inherit the
    # background and the cursor row is a full-width highlight bar.
    pink = sc.Color.rgb(255, 121, 198)
    finder = sc.Fuzzy(sc.FuzzyOpts(
        prompt='Switch branch', accent=sc.Color.CYAN,
        selected_style=sc.Style(sc.Attr.BOLD, sc.Color.WHITE, pink),
        box=sc.BoxStyle(enabled=True,
                        border=sc.BorderStyle(type=sc.BorderType.ROUNDED,
                                              color=pink),
                        bg=sc.Color.rgb(30, 30, 46),
                        padding=sc.Edges(left=1, right=1)),
    ))
    for branch in branches:
        finder.add(branch)
    index = finder.run()
    if index is not None:
        sc.println(f'  -> {branches[index]}')


def run_table_finder() -> None:
    """Table mode: the query searches and highlights across all columns."""
    finder = sc.Fuzzy(sc.FuzzyOpts(
        prompt='Pick a host', table=True,
        headers=['Host', 'Region', 'Status'], accent=sc.Color.BLUE,
        table_opts=sc.TableOpts(border=sc.BorderType.ROUNDED, header_row=True,
                                header_style=sc.Style.bold())))
    finder.add_row(['web-01', 'eu-central', 'healthy'])
    finder.add_row(['web-02', 'eu-central', 'degraded'])
    finder.add_row(['db-01', 'us-east', 'healthy'])
    finder.add_row(['cache-01', 'ap-south', 'healthy'])
    index = finder.run()
    if index is not None:
        sc.println(f'  -> row {index}')


def show_pure_matcher() -> None:
    """fuzzy_match is the pure scoring function behind the widget."""
    hit, score = sc.fuzzy_match('ffind', 'feature/fuzzy-finder')
    sc.println(f'  match "ffind" in "feature/fuzzy-finder": {hit} (score {score})',
               sc.Style.dim())


def main() -> None:
    show_pure_matcher()  # pure, works without a terminal

    if not sc.input_available():
        sc.alert.warning('Run this example in a real terminal (not piped).')
        return
    run_list_finder()
    run_table_finder()


if __name__ == '__main__':
    main()
