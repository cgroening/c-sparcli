"""Non-interactive tests: pure functions, render-and-capture, no-TTY behaviour.

These run headless (no terminal needed). Interactive widget behaviour itself is
covered by the C library's own PTY suite; here we exercise the Python plumbing.
"""

from __future__ import annotations

import pytest

import sparcli as sc


# ── pure functions ────────────────────────────────────────────────────────────
def test_version():
    assert sc.version_string().count(".") == 2
    maj, minr, pat = sc.version()
    parts = tuple(int(x) for x in sc.version_string().split("."))
    assert (maj, minr, pat) == parts


def test_fuzzy_match():
    ok, score = sc.fuzzy_match("ab", "cab")
    assert ok and score > 0
    assert sc.fuzzy_match("xyz", "cab")[0] is False
    # empty pattern always matches
    assert sc.fuzzy_match("", "anything")[0] is True


def test_strip_ansi():
    assert sc.strip_ansi("\033[1;31mhi\033[0m") == "hi"
    assert sc.strip_ansi("plain") == "plain"


def test_truncate():
    assert sc.truncate("hello world", 5) == "hell…"
    assert sc.truncate("hi", 10) == "hi"
    assert sc.truncate("hello", 4, ellipsis="...") == "h..."


# ── colors / styles ───────────────────────────────────────────────
def test_color_constants_and_rgb():
    assert sc.Color.NONE.index == 0
    assert sc.Color.RED.index == 2
    assert sc.Color.rgb(300, -5, 50) == sc.Color(-1, 255, 0, 50)  # clamped


def test_attr_flags_combine():
    combined = sc.Attr.BOLD | sc.Attr.ITALIC
    assert int(combined) == 1 | 4


# ── render & capture ──────────────────────────────────────────────
def _plain(rendered: sc.Rendered) -> list[str]:
    return [sc.strip_ansi(line) for line in rendered.lines]


def test_capture_panel_contains_content():
    r = sc.capture.panel("hello", sc.PanelOpts(title="hi"))
    text = "\n".join(_plain(r))
    assert "hello" in text
    assert r.width > 0


def test_capture_table_rows():
    t = sc.Table()
    t.column("Name").column("Age")
    t.row(["Ada", "36"])
    t.row(["Alan", "41"])
    lines = _plain(sc.capture.table(t, sc.TableOpts(header_row=True)))
    joined = "\n".join(lines)
    for token in ("Name", "Age", "Ada", "36", "Alan", "41"):
        assert token in joined


def test_capture_table_column_style():
    # A per-column style colors unstyled data cells (cyan = ESC[36m); a
    # markup cell keeps its own style (red = ESC[31m).
    t = sc.Table()
    t.column("Name", sc.ColOpts(style=sc.Style(fg=sc.Color.CYAN)))
    t.column("Age")
    t.row(["Ada", "36"])
    t.row([sc.Cell.markup("[red]Alan[/]"), "41"])
    raw = "\n".join(sc.capture.table(t, sc.TableOpts(header_row=True)).lines)
    assert "\x1b[36m" in raw
    assert "\x1b[31m" in raw
    assert "Ada" in sc.strip_ansi(raw)


def test_capture_markup_text():
    t = sc.Text.from_markup("[bold red]Err[/] ok")
    lines = _plain(sc.capture.text(t))
    assert "Err ok" in "\n".join(lines)
    assert t.visible_width >= len("Err ok")


def test_vstack_combines():
    a = sc.capture.string("one")
    b = sc.capture.string("two")
    stacked = sc.vstack([a, b], gap=1)
    plain = _plain(stacked)
    assert "one" in plain[0]
    assert "two" in plain[-1]
    assert sc.vstack([]) is None


def test_text_append_chaining():
    t = sc.Text()
    assert t.append("a", sc.Style.bold()).append("b") is t
    assert t.visible_width == 2


def test_text_append_link():
    t = sc.Text()
    assert t.append_link("click", "https://example.com") is t
    # OSC-8 escape bytes occupy no visible columns
    assert t.visible_width == 5
    # None / empty URL degrades to a plain span
    plain = sc.Text().append_link("plain", None).append_link("x", "")
    assert plain.visible_width == 6


def test_columns_capture_runs():
    kv = sc.Kv()
    kv.add("k", "v")
    cols = sc.Columns(sc.ColumnsOpts(gap=2))
    cols.add_rendered(sc.capture.kv(kv)).add_str("side")
    out = _plain(sc.capture.columns(cols))
    assert any("k" in line and "v" in line for line in out)


# ── no-TTY input behaviour ────────────────────────────────────────
def test_input_unavailable_without_tty():
    # Under pytest stdin/stdout are not a terminal.
    assert sc.input_available() is False
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.confirm("ok?")
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.text_input("name")
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.decimal_input("amount", sc.NumberOpts(decimals=2, decimal_sep=","))
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.Select().add("a").run_one()


def test_number_opts_start_empty_accepted():
    # start_empty must reach the C struct (cdef field + _fill); without a
    # TTY the prompt still raises cleanly instead of erroring on the field.
    opts = sc.NumberOpts(decimals=2, decimal_sep=",", start_empty=True)
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.decimal_input("amount", opts)


def test_fuzzy_outlives_temporary_opts_strings():
    # The prompt is built from a temporary string; the finder must keep its
    # own copy so a GC between construction and run() cannot free it
    # (previously seen as a garbled prompt - use-after-free).
    import gc

    finder = sc.Fuzzy(sc.FuzzyOpts(prompt="".join(["Cat", "egory"])))
    finder.add("Groceries")
    gc.collect()
    with pytest.raises(sc.SparcliInputUnavailable):
        finder.run()


def test_progressbar_caps_survive_gc(tmp_path):
    # left/right caps are copied by the C side at construction; the cffi
    # buffers may be garbage-collected right afterwards (previously a
    # use-after-free on every draw).
    import gc

    bar = sc.ProgressBar(sc.ProgressBarOpts(left_cap="<", right_cap=">",
                                            bar_width=10))
    gc.collect()
    path = tmp_path / "bar.txt"
    with open(path, "w") as f, sc.ScopedOutput(f):
        bar.finish(1.0, 1.0)
    out = sc.strip_ansi(path.read_text())
    assert "<" in out and ">" in out


def test_theme_strings_survive_gc():
    # Theme glyph strings are copied process-wide by the C side; the cffi
    # buffers may be garbage-collected right after set_theme.
    import gc

    sc.set_theme(sc.Theme(cursor_marker="==> "))
    gc.collect()
    sel = sc.Select()
    sel.add("a")
    with pytest.raises(sc.SparcliInputUnavailable):
        sel.run_one()
    sc.set_theme(None)


# ── output redirection ────────────────────────────────────────────
def test_scoped_output_redirect(tmp_path):
    path = tmp_path / "out.txt"
    with open(path, "w") as f, sc.ScopedOutput(f):
        sc.println("captured", sc.Style.bold())
    content = path.read_text()
    assert "captured" in sc.strip_ansi(content)


# ── interior NUL rejection ────────────────────────────────────────
def test_interior_nul_rejected():
    with pytest.raises(ValueError):
        sc.println("a\x00b")


# ── ANSI-injection protection ─────────────────────────────────────
def test_ansi_injection_stripped_by_default():
    """Escape codes and control bytes in user strings are removed."""
    assert not sc.allow_ansi()
    r = sc.capture.panel("\x1b[31mevil\x1b]0;title\x07\x1b[0m content")
    joined = "\n".join(r.lines)
    assert "\x1b[31m" not in joined
    assert "\x1b]0;" not in joined
    assert "evil content" in sc.strip_ansi(joined)


def test_ansi_global_allow_roundtrip():
    """set_allow_ansi(True) passes well-formed escape codes through."""
    sc.set_allow_ansi(True)
    try:
        assert sc.allow_ansi()
        r = sc.capture.panel("\x1b[31mred\x1b[0m")
        assert any("\x1b[31m" in line for line in r.lines)
    finally:
        sc.set_allow_ansi(False)
    assert not sc.allow_ansi()


def test_ansi_per_widget_override():
    """PanelOpts(ansi=AnsiMode.ALLOW) overrides the global default."""
    r = sc.capture.panel(
        "\x1b[32mgreen\x1b[0m",
        sc.PanelOpts(ansi=sc.AnsiMode.ALLOW),
    )
    lines = r.lines
    assert any("\x1b[32m" in line for line in lines)
    # Borders stay aligned: every line strips to the same visible width
    widths = {len(sc.strip_ansi(line)) for line in lines}
    assert len(widths) == 1
