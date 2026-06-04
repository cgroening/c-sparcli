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


def test_special_key_chords():
    # Arrow/Enter/Tab chords construct and are accepted by the shortcut builder
    # (parity with the CLI's --arrow-nav navigation).
    for build in (sc.key_left, sc.key_right, sc.key_up, sc.key_down,
                  sc.key_enter, sc.key_tab):
        assert isinstance(build(), sc.KeyChord)
    shortcuts = sc.Shortcuts().on_return(sc.key_left(), 1, name="back")
    assert shortcuts.fired() == -1  # nothing fired yet


def test_fuzzy_has_no_selection_before_run():
    fz = sc.Fuzzy(sc.FuzzyOpts(prompt="Find"))
    fz.add("alpha").add("beta")
    assert fz.has_selection() is False


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


def test_markup_backtick_inline_code():
    # Backtick spans render in magenta (ESC[35m) with the backticks removed;
    # \` escapes a literal backtick.
    t = sc.Text.from_markup("run `make qa` or \\` now")
    raw = "\n".join(sc.capture.text(t).lines)
    assert "\x1b[35m" in raw
    plain = "\n".join(_plain(sc.capture.text(t)))
    assert "run make qa or ` now" in plain


def test_markup_backtick_custom_code_style():
    # code_style overrides the default magenta (cyan = ESC[36m).
    t = sc.Text.from_markup(
        "see `code`", code_style=sc.Style(fg=sc.Color.CYAN)
    )
    raw = "\n".join(sc.capture.text(t).lines)
    assert "\x1b[36m" in raw
    assert "\x1b[35m" not in raw


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


def test_xdg_paths(tmp_path, monkeypatch):
    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "cfg"))
    monkeypatch.setenv("XDG_STATE_HOME", str(tmp_path / "state"))

    cfg = sc.config_dir("pytest-app")
    assert cfg == tmp_path / "cfg" / "pytest-app"
    assert cfg.is_dir()

    log = sc.app_file(sc.PathKind.STATE, "pytest-app", "logs/run.log")
    assert log == tmp_path / "state" / "pytest-app" / "logs" / "run.log"
    assert log.parent.is_dir() and not log.exists()

    # Invalid names raise instead of touching the filesystem
    import pytest
    with pytest.raises(sc.SparcliError):
        sc.config_dir("evil/../name")
    with pytest.raises(sc.SparcliError):
        sc.app_file(sc.PathKind.CONFIG, "app", "../escape")


def test_pager_noop_off_terminal():
    # SPARCLI_NO_TTY (conftest.py / make python-test) forces the no-op
    # session even when the suite runs from an interactive terminal.
    with sc.Pager() as pager:
        sc.println("paged content")
    assert pager.exit_status == 0


def test_error_report():
    import pytest

    report = sc.ErrorReport("boom").cause("a reason").hint("a hint").code(7)
    assert report.exit_code == 7

    # die() raises SystemExit with the configured code (never calls C exit)
    with pytest.raises(SystemExit) as exc_info:
        report.die()
    assert exc_info.value.code == 7


def test_logger_file_sink(tmp_path):
    log_file = tmp_path / "test.log"

    logger = sc.Logger(hide_timestamps=True)
    assert logger.add_file(log_file, sc.LogLevel.DEBUG)
    logger.info("python record with 100% data semantics")
    logger.debug("debug detail")
    logger.close()  # flushes + closes the file sink

    content = log_file.read_text()
    assert "INFO" in content
    # The "%" must arrive literally (message is data, not a format string)
    assert "python record with 100% data semantics" in content
    assert "debug detail" in content
    assert "\x1b" not in content  # file sinks are plain text


def test_global_log_level_roundtrip():
    sc.log_set_level(sc.LogLevel.ERROR)
    assert sc.log_level() == sc.LogLevel.ERROR
    sc.log_reset()
    assert sc.log_level() == sc.LogLevel.INFO


def test_columns_capture_runs():
    kv = sc.Kv()
    kv.add("k", "v")
    cols = sc.Columns(sc.ColumnsOpts(gap=2))
    cols.add_rendered(sc.capture.kv(kv)).add_str("side")
    out = _plain(sc.capture.columns(cols))
    assert any("k" in line and "v" in line for line in out)


# ── no-TTY input behaviour ────────────────────────────────────────
def test_input_unavailable_without_tty():
    # conftest.py / make python-test set SPARCLI_NO_TTY=1, so the library
    # reports "no terminal" even when the suite runs from an interactive one.
    assert sc.input_available() is False
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.confirm("ok?")
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.text_input("name")
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.decimal_input("amount", sc.NumberOpts(decimals=2, decimal_sep=","))
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.Select().add("a").run_one()


def test_calc_eval():
    # Pure expression evaluator behind the number input's calculator mode.
    assert sc.calc_eval("1+2*3") == 7.0
    assert sc.calc_eval("1,5+2*3") == 7.5      # comma decimal separator
    assert sc.calc_eval("(1+2)*3") == 9.0
    assert sc.calc_eval("2*-3") == -6.0
    assert sc.calc_eval("1/0") is None         # division by zero
    assert sc.calc_eval("1++2") is None        # typo
    assert sc.calc_eval("garbage") is None


def test_number_opts_calculator_accepted():
    # The calculator opts must reach the C struct (cdef fields + _fill);
    # without a TTY the prompt still raises cleanly.
    opts = sc.NumberOpts(decimals=2, decimal_sep=",", start_empty=True,
                         calculator=True, calc_store_rounded=True,
                         calc_show_precise=True,
                         error_style=sc.Style(fg=sc.Color.RED),
                         calc_warn_text="Anzeigewert wird gespeichert",
                         calc_warn_style=sc.Style(fg=sc.Color.YELLOW))
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.decimal_input("amount", opts)


def test_number_opts_start_empty_accepted():
    # start_empty must reach the C struct (cdef field + _fill); without a
    # TTY the prompt still raises cleanly instead of erroring on the field.
    opts = sc.NumberOpts(decimals=2, decimal_sep=",", start_empty=True)
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.decimal_input("amount", opts)


def test_text_input_suggest_dropdown_opts_accepted():
    # The dropdown SuggestOpts must reach the C struct (cdef fields + _fill);
    # without a TTY the prompt still raises cleanly instead of erroring on
    # the new fields.
    opts = sc.TextInputOpts(
        suggestions=["commit", "checkout", "cherry-pick"],
        suggest=sc.SuggestOpts(
            mode=sc.SuggestMode.DROPDOWN,
            match=sc.SuggestMatch.FUZZY,
            max_visible=3,
            border=sc.BorderStyle(type=sc.BorderType.ROUNDED),
            selected_style=sc.Style(fg=sc.Color.BLACK, bg=sc.Color.CYAN),
            cursor_marker="> ",
        ),
    )
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.text_input("cmd", opts)


def test_box_style_reaches_widgets():
    # BoxStyle must flow into the C struct for both a migrated text widget and
    # a newly-boxed list widget (cdef field + apply_box). Without a TTY the
    # prompts/run still raise/return cleanly instead of erroring on the field.
    box = sc.BoxStyle(
        enabled=True,
        border=sc.BorderStyle(type=sc.BorderType.DOUBLE, color=sc.Color.CYAN),
        bg=sc.Color.BLUE,
        padding=sc.Edges(left=2, right=2),
        margin=sc.Edges(top=1),
        width=40,
    )
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.text_input("name", sc.TextInputOpts(box=box))

    # Width modes + background extent (the new list-widget controls) must also
    # reach the C struct cleanly.
    wbox = sc.BoxStyle(
        bg=sc.Color.BLACK,
        width_mode=sc.WidthMode.CONTENT,
        min_width=20,
        max_width=50,
        bg_extent=sc.BgExtent.TEXT,
    )
    sel = sc.Select(sc.SelectOpts(prompt="pick", box=wbox))
    sel.add("a").add("b")
    with pytest.raises(sc.SparcliInputUnavailable):
        sel.run()

    sel = sc.Select(sc.SelectOpts(prompt="pick", box=box))
    sel.add("a").add("b")
    with pytest.raises(sc.SparcliInputUnavailable):
        sel.run()

    # Themeable box defaults: set + reset must not raise.
    sc.set_theme(sc.Theme(box=sc.BoxStyle(
        border=sc.BorderStyle(type=sc.BorderType.ROUNDED))))
    sc.set_theme(None)


def test_history_stores_and_recalls():
    # Input-history storage semantics: copies, consecutive-duplicate dedupe,
    # cap eviction, and list-style access.
    history = sc.History()
    history.add("first").add("first").add("second")
    assert len(history) == 2
    assert history[0] == "first"
    assert history[-1] == "second"
    assert history.entries() == ["first", "second"]
    with pytest.raises(IndexError):
        history[5]

    capped = sc.History(max_entries=2)
    capped.add("one").add("two").add("three")
    assert capped.entries() == ["two", "three"]

    kept = sc.History(keep_duplicates=True)
    kept.add("x").add("x")
    assert len(kept) == 2


def test_history_persists_to_file(tmp_path):
    # The context manager saves on exit; a fresh handle loads it back.
    path = str(tmp_path / "history")
    with sc.History(file=path) as history:
        history.add('add "Buy milk"')
        history.add("list --all")

    reloaded = sc.History(file=path)
    assert reloaded.entries() == ['add "Buy milk"', "list --all"]

    # Strings passed to the constructor are copied by the C side (the cffi
    # buffers in the arena may be GC'd right after construction).
    import gc

    gc.collect()
    assert reloaded.save() is True


def test_text_input_history_opts_accepted():
    # The history fields must reach the C struct (cdef fields + _fill);
    # without a TTY the prompt raises cleanly and never touches the history.
    history = sc.History()
    history.add("earlier command")
    opts = sc.TextInputOpts(history=history, no_history_add=True)
    with pytest.raises(sc.SparcliInputUnavailable):
        sc.text_input("repl>", opts)
    assert len(history) == 1


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


# ── live display ──────────────────────────────────────────────────
def test_live_buffers_frames_off_terminal(tmp_path):
    # Off-terminal, a live session buffers updates and prints only the final
    # frame when it ends; intermediate frames never reach the stream.
    path = tmp_path / "live.txt"
    with open(path, "w") as f, sc.ScopedOutput(f):
        with sc.Live() as live:
            live.update("frame one")
            live.update(sc.capture.string("frame two"))
            live.update("final frame")
    out = sc.strip_ansi(path.read_text())
    assert "frame one" not in out
    assert "frame two" not in out
    assert "final frame" in out


def test_live_transient_prints_nothing(tmp_path):
    path = tmp_path / "live.txt"
    with open(path, "w") as f, sc.ScopedOutput(f):
        with sc.Live(transient=True) as live:
            live.update("never shown")
    assert "never shown" not in path.read_text()


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
