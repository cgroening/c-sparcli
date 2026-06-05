"""Colored unified diff of two texts.

    make run-example EX=python/output/diff
"""

import sparcli as sc


def main() -> None:
    before = "name = sparcli\nversion = 0.1.0\ncolor = blue\n"
    after = "name = sparcli\nversion = 0.2.0\ncolor = green\ndebug = true\n"

    sc.diff(before, after, sc.DiffOpts(
        old_label="config.toml (old)",
        new_label="config.toml (new)",
        context=1,
    ))

    print()

    # The same diff captured and framed with padding.
    r = sc.diff_rendered(before, after, sc.DiffOpts(no_header=True))
    r.pad(sc.PadOpts(left=2))


if __name__ == "__main__":
    main()
