"""Several progress bars updated together in place.

    make run-example EX=python/output/multiprogress

On a terminal the bars animate; piped/redirected, only the final stack is
printed (the live display buffers off a TTY).
"""

import time

import sparcli as sc


def main() -> None:
    bar = sc.ProgressBarOpts(
        type=sc.ProgressType.BLOCK, bar_width=24, show_percent=True,
        left_cap="[", right_cap="]",
    )
    with sc.MultiProgress() as mp:
        download = mp.add("download", bar)
        extract = mp.add("extract", bar)
        verify = mp.add("verify", bar)
        for i in range(101):
            mp.update(download, i, 100)
            mp.update(extract, i * 0.7, 100)
            mp.update(verify, i * 0.4, 100)
            time.sleep(0.02)


if __name__ == "__main__":
    main()
