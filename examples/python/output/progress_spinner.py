#!/usr/bin/env python3
"""Animated progress bars and spinners.

    make run-example EX=python/output/progress_spinner
"""

from __future__ import annotations

import time

import sparcli as sc

# Seconds between animation frames (kept short for the demo).
FRAME_DELAY = 0.008


def run_progress_bars() -> None:
    """A styled block bar and a threshold-colored line bar."""
    download = sc.ProgressBar(sc.ProgressBarOpts(
        type=sc.ProgressType.BLOCK, left_cap='[', right_cap=']',
        fill_color=sc.Color.CYAN, show_value=True, width=60, label_width=10,
        label_style=sc.Style.bold()))
    download.set_label('download')
    for step in range(201):
        download.draw(step, 200)
        time.sleep(FRAME_DELAY)
    download.finish(200, 200)

    deploy = sc.ProgressBar(sc.ProgressBarOpts(
        type=sc.ProgressType.LINE, width=60, label_width=10,
        thresholds=sc.Thresholds(enabled=True, mid=0.4, high=0.8,
                                 color_low=sc.Color.RED,
                                 color_mid=sc.Color.YELLOW,
                                 color_high=sc.Color.GREEN)))
    deploy.set_label('deploy')
    for step in range(101):
        # max == 0: the value is already a ratio between 0.0 and 1.0.
        deploy.draw(step / 100.0, 0)
        time.sleep(FRAME_DELAY)
    deploy.finish(1.0, 0)


def run_spinner() -> None:
    """A spinner whose label changes mid-animation."""
    spinner = sc.Spinner('Connecting...', sc.SpinnerOpts(
        type=sc.SpinnerType.BRAILLE, color=sc.Color.CYAN,
        label_style=sc.Style.dim()))
    for _ in range(25):
        spinner.tick()
        time.sleep(0.04)
    spinner.set_label('Fetching data...')
    for _ in range(25):
        spinner.tick()
        time.sleep(0.04)
    spinner.finish(True, 'Done')


def run_transient_line() -> None:
    """clear_line() overwrites the current line in place, for a transient
    status that should leave no trace afterwards."""
    import sys

    sc.print_('Preparing...')
    sys.stdout.flush()
    time.sleep(0.5)
    sc.clear_line()
    sc.println('Ready.')


def main() -> None:
    run_progress_bars()
    sc.println('')
    run_spinner()
    sc.println('')
    run_transient_line()


if __name__ == '__main__':
    main()
