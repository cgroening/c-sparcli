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


def test_fuzzy_sections_multi_and_styles():
    from sparcli.keys import key_ctrl

    fz = sc.Fuzzy(sc.FuzzyOpts(
        multi=True, section_counts=True,
        toggle_all_key=key_ctrl("a"),
        order=sc.FuzzyOrder.COLUMN, order_column=0))
    fz.add_section("Monday")
    fz.add("buy milk")
    fz.add("call bob")

    # Stable ids + label / set_label.
    fz.set_id(1, 42)
    assert fz.id_at(1) == 42
    assert fz.id_at(99) == 0
    assert fz.label(1) == "buy milk"
    fz.set_label(1, "BUY MILK")
    assert fz.label(1) == "BUY MILK"

    # Checked set.
    fz.set_checked(1)
    fz.set_checked(2)
    assert fz.checked_count() == 2 and fz.is_checked(1)
    fz.check_all(False)
    assert fz.checked_count() == 0

    # Disabling clears + blocks the check.
    fz.set_disabled(2, True)
    fz.set_checked(2)
    assert fz.is_checked(2) is False


def test_fuzzy_styled_sections():
    # FFI marshalling of add_section_styled (by-value style) and
    # add_section_text (borrowed Text pointer + fill). Build only (no TTY).
    fz = sc.Fuzzy(sc.FuzzyOpts(prompt="Tasks"))
    fz.add_section_styled(
        "OVERDUE", sc.Style(fg=sc.Color.WHITE, bg=sc.Color.RED))
    fz.add("Pay invoice")
    title = sc.Text.from_markup("[bold]Due[/] soon")
    fz.add_section_text(title, sc.Style(bg=sc.Palette.MAGENTA))
    fz.add("Finish slides")
    assert fz.cursor_id() == 0  # nothing selected before a run


def test_fuzzy_modal_opts():
    from sparcli.keys import key_char

    # Modal opts construct cleanly (including a bare-char clear key + custom
    # mode styling); no TTY so we only exercise the option plumbing.
    fz = sc.Fuzzy(sc.FuzzyOpts(
        modal=True, start_in_insert=True,
        clear_key=key_char("c"),
        normal_label="CMD", insert_label="EDIT",
        mode_normal_style=sc.Style(fg=sc.Color.WHITE, bg=sc.Color.BLUE),
        mode_insert_style=sc.Style(fg=sc.Color.BLACK, bg=sc.Color.GREEN)))
    fz.add("alpha").add("beta")
    assert fz.has_selection() is False


def test_key_chord_builders():
    # Chainable modifiers set the mod bits (SC_MOD_SHIFT=8, SC_MOD_ALT=2).
    assert sc.key_up().shift().value.mods & 8
    assert sc.key_up().alt().value.mods & 2
    assert sc.key_up().alt().shift().value.mods == (2 | 8)
    # Char chords preserve case (case-sensitive shortcuts: p != P).
    assert sc.key_char("p").value.codepoint == ord("p")
    assert sc.key_char("P").value.codepoint == ord("P")
    # The named-key builders all construct.
    for kc in (sc.key_delete(), sc.key_backspace(), sc.key_home(), sc.key_end(),
               sc.key_pageup(), sc.key_pagedown(), sc.key_backtab(),
               sc.key_esc(), sc.key_special(sc.key_up().value.key)):
        assert kc.value is not None


def test_fullscreen_opts_and_altscreen():
    # A pinned header (borrowed for the run); exercises the fullscreen/valign/
    # header _fill path on both widgets + the alt-screen context manager.
    header = sc.capture.string("== header ==")

    fz = sc.Fuzzy(sc.FuzzyOpts(
        prompt="Find", fullscreen=True, valign=sc.VAlign.MIDDLE, header=header))
    fz.add("alpha").add("beta")
    assert fz.has_selection() is False

    form = sc.Form(sc.FormOpts(
        fullscreen=True, valign=sc.VAlign.BOTTOM, header=header))
    form.row_begin()
    field = form.add_text("Title", "x")
    assert form.get_string(field) == "x"

    # No-op off a terminal (conftest sets SPARCLI_NO_TTY); just verify it runs.
    with sc.altscreen():
        pass


def test_fuzzy_rich_and_styled_rows():
    style = sc.Style(fg=sc.Color.RED)
    g = sc.Fuzzy(sc.FuzzyOpts(table=True, headers=["Status", "Task"]))
    g.add_row(["overdue", "pay invoice"], styles=[style, sc.Style()])
    assert g.label(0) == "overdue"

    badge = sc.Text()
    badge.append("HI")
    badge.append("GH", sc.Style(attr=sc.Attr.BOLD))
    r = sc.Fuzzy(sc.FuzzyOpts(table=True, headers=["x"]))
    r.add_row_rich([badge])
    assert r.label(0) == "HIGH"


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


def test_color_by_name():
    # ANSI names resolve to the named colors; palette names to RGB.
    assert sc.color_by_name("red") == sc.Color.RED
    accent = sc.color_by_name("accent")
    assert accent is not None and accent == sc.Palette.ACCENT
    # Hex is not a name, and unknown names return None.
    assert sc.color_by_name("#ff0000") is None
    assert sc.color_by_name("definitely-not-a-color") is None


def test_palette_runtime_override():
    try:
        assert sc.Palette.set("accent", sc.Color.RED)
        assert sc.color_by_name("accent") == sc.Color.RED
        assert sc.Palette.get("accent") == sc.Color.RED
        # Unknown names are rejected.
        assert not sc.Palette.set("definitely-not-a-color", sc.Color.RED)
    finally:
        sc.Palette.reset()
    assert sc.color_by_name("accent") == sc.Palette.ACCENT


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


def test_strike_attribute():
    # SC_TEXT_ATTR_STRIKE = 1 << 4; renders the ANSI strike code (ESC[9m),
    # via the Attr flag, the Style.strike() shortcut, and the [strike] markup.
    assert int(sc.Attr.STRIKE) == 16
    t = sc.Text()
    t.append("done", sc.Style.strike())
    assert "\x1b[9m" in "\n".join(sc.capture.text(t).lines)
    m = sc.Text.from_markup("[strike]x[/]  [s]y[/]")
    assert "\x1b[9m" in "\n".join(sc.capture.text(m).lines)


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


def test_live_update_text_and_table(tmp_path):
    # The Text/Table overloads of update() (and update_table) capture and
    # redraw; off-terminal only the final frame is printed.
    path = tmp_path / "live.txt"
    with open(path, "w") as f, sc.ScopedOutput(f):
        with sc.Live(prompt_rows=0) as live:
            live.update(sc.Text.from_str("text frame"))
            t = sc.Table()
            t.column("C")
            t.row(["final cell"])
            live.update_table(t, sc.TableOpts(header_row=True))
    out = sc.strip_ansi(path.read_text())
    assert "text frame" not in out
    assert "final cell" in out


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


# ── humanize / diff / multiprogress ───────────────────────────────────────────
def test_humanize():
    assert sc.humanize.bytes(1536) == "1.5 KB"
    assert sc.humanize.bytes(1536, sc.ByteUnit.IEC) == "1.5 KiB"
    assert sc.humanize.number(1234567) == "1,234,567"
    assert sc.humanize.compact(12400) == "12.4k"
    assert sc.humanize.percent(0.42) == "42%"
    assert sc.humanize.duration(93) == "1m 33s"
    assert sc.humanize.duration_clock(3725) == "01:02:05"
    de = sc.HumanizeOpts(decimals=2, decimal_sep=",", group_sep=".")
    assert sc.humanize.number(1234567.89, de) == "1.234.567,89"
    now = 1_000_000_000
    assert sc.humanize.relative(now - 3 * 3600, now) == "3 hours ago"


def test_diff_render():
    r = sc.diff_rendered("a\nb\nc\n", "a\nB\nc\n", sc.DiffOpts(context=0))
    plain = [sc.strip_ansi(line) for line in r.lines]
    assert any(line == "-b" for line in plain)
    assert any(line == "+B" for line in plain)


def test_multiprogress():
    with sc.MultiProgress() as mp:
        a = mp.add("a", sc.ProgressBarOpts(show_percent=True))
        b = mp.add("b", sc.ProgressBarOpts(show_percent=True))
        assert (a, b) == (0, 1)
        mp.update(a, 100, 100)
        mp.update(b, 50, 100)
        mp.set_label(b, "b2")


def test_form_construction_and_getters():
    import datetime
    f = sc.Form(sc.FormOpts(title="Contact"))
    f.row_begin()
    name = f.add_text("Name", "Ada",
                      sc.FieldOpts(width_mode=sc.FieldWidthMode.PCT, width=50))
    qty = f.add_number("Qty", 7)
    tier = f.add_select("Tier", ["Bronze", "Silver", "Gold"], 2)
    tags = f.add_multiselect("Tags", ["a", "b", "c"], [0, 2])
    ok = f.add_bool("OK", True)
    when = f.add_date("When", datetime.date(2026, 5, 15))

    assert (name, qty, tier, tags, ok, when) == (0, 1, 2, 3, 4, 5)
    assert f.get_string(name) == "Ada"
    assert f.get_number(qty) == 7.0
    assert f.get_bool(ok) is True
    assert f.get_choice(tier) == 2
    assert f.get_checked(tags) == [0, 2]
    assert f.get_date(when) == datetime.date(2026, 5, 15)
    assert f.get_date(name) is None
    f.close()


def test_form_multiline_keeps_newlines():
    with sc.Form(sc.FormOpts(editor="true")) as f:
        f.add_text("Notes", "x\ny", sc.FieldOpts(multiline=True))
        assert f.get_string(0) == "x\ny"


def test_form_autoedit_opt():
    # autoedit (open the first field's editor at start) is exposed and builds.
    with sc.Form(sc.FormOpts(autoedit=True)) as f:
        f.add_text("Title", "draft")
        assert f.get_string(0) == "draft"


def test_form_run_unavailable_without_tty():
    # Off a TTY (SPARCLI_NO_TTY) run() returns False, never grabbing the keyboard.
    with sc.Form() as f:
        f.add_text("Name", "x")
        assert f.run() is False
