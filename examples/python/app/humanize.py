"""Human-readable sizes, durations, relative time and grouped numbers.

    make run-example EX=python/app/humanize
"""

import sparcli as sc


def main() -> None:
    print("1536 (SI):  ", sc.humanize.bytes(1536))
    print("1536 (IEC): ", sc.humanize.bytes(1536, sc.ByteUnit.IEC))
    print("5 GB:       ", sc.humanize.bytes(5_000_000_000))

    print("grouped:    ", sc.humanize.number(1_234_567))
    print("compact:    ", sc.humanize.compact(12_400))
    print("percent:    ", sc.humanize.percent(0.42))

    de = sc.HumanizeOpts(decimals=2, decimal_sep=",", group_sep=".")
    print("de_DE:      ", sc.humanize.number(1_234_567.89, de))

    print("93s:        ", sc.humanize.duration(93))
    print("8054s:      ", sc.humanize.duration(8054))
    print("clock:      ", sc.humanize.duration_clock(3725))

    now = 1_000_000_000
    print("3h ago:     ", sc.humanize.relative(now - 3 * 3600, now))
    print("in 2d:      ", sc.humanize.relative(now + 2 * 86_400, now))


if __name__ == "__main__":
    main()
