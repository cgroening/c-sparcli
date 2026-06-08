"""Key chords and custom shortcuts shared by every input widget."""

from __future__ import annotations

from dataclasses import dataclass

from ._ffi import apply_color, cstr, ffi, lib
from .color import Color

_MODE_RETURN, _MODE_CALLBACK = 0, 1


class KeyChord:
    """A key combination (Ctrl-letter, F-key, or Alt-letter). Build with
    :func:`key_ctrl`, :func:`key_fn` or :func:`key_alt`."""

    __slots__ = ("_h",)

    def __init__(self, holder) -> None:
        self._h = holder

    @property
    def value(self):
        """The underlying ``ScKeyChord`` cdata (by value)."""
        return self._h[0]

    def _with_mod(self, mod: int) -> "KeyChord":
        h = ffi.new("ScKeyChord *")
        h[0].key = self._h[0].key
        h[0].codepoint = self._h[0].codepoint
        h[0].mods = self._h[0].mods | mod
        return KeyChord(h)

    def shift(self) -> "KeyChord":
        """A copy with the Shift modifier added (for named keys, e.g.
        ``key_up().shift()``)."""
        return self._with_mod(lib.SC_MOD_SHIFT)

    def alt(self) -> "KeyChord":
        """A copy with the Alt/Meta modifier added, e.g. ``key_up().alt()``."""
        return self._with_mod(lib.SC_MOD_ALT)

    def ctrl(self) -> "KeyChord":
        """A copy with the Ctrl modifier added (for named keys, e.g.
        ``key_right().ctrl()``)."""
        return self._with_mod(lib.SC_MOD_CTRL)


def key_ctrl(letter: str) -> KeyChord:
    """A Ctrl+letter chord, e.g. ``key_ctrl('r')`` (case-insensitive)."""
    h = ffi.new("ScKeyChord *")
    h[0] = lib.sc_key_ctrl(letter.encode("ascii")[:1])
    return KeyChord(h)


def key_char(letter: str) -> KeyChord:
    """A bare character chord (no modifiers), e.g. ``key_char('c')``. Useful for
    the modal fuzzy finder's ``clear_key`` or bare-letter shortcuts."""
    h = ffi.new("ScKeyChord *")
    h[0].key = lib.SC_KEY_CHAR
    h[0].codepoint = ord(letter[:1])
    h[0].mods = 0
    return KeyChord(h)


def key_fn(n: int) -> KeyChord:
    """A function-key chord; ``n`` in 1..12 → F1..F12."""
    h = ffi.new("ScKeyChord *")
    h[0] = lib.sc_key_fn(int(n))
    return KeyChord(h)


def key_alt(letter: str) -> KeyChord:
    """An Alt/Meta+letter chord, e.g. ``key_alt('x')``."""
    h = ffi.new("ScKeyChord *")
    h[0] = lib.sc_key_alt(letter.encode("ascii")[:1])
    return KeyChord(h)


def key_special(key: int) -> KeyChord:
    """A chord for a named (non-character) key, e.g. ``key_special(sc.KeyType
    .DELETE)``. Add modifiers with ``.shift()`` / ``.alt()`` / ``.ctrl()``."""
    h = ffi.new("ScKeyChord *")
    h[0] = lib.sc_key_special(key)
    return KeyChord(h)


_key_special = key_special   # backward-compatible alias


def key_left() -> KeyChord:
    """Left-arrow chord (e.g. for back navigation)."""
    return _key_special(lib.SC_KEY_LEFT)


def key_right() -> KeyChord:
    """Right-arrow chord (e.g. for forward/select navigation)."""
    return _key_special(lib.SC_KEY_RIGHT)


def key_up() -> KeyChord:
    """Up-arrow chord."""
    return _key_special(lib.SC_KEY_UP)


def key_down() -> KeyChord:
    """Down-arrow chord."""
    return _key_special(lib.SC_KEY_DOWN)


def key_enter() -> KeyChord:
    """Enter-key chord."""
    return _key_special(lib.SC_KEY_ENTER)


def key_tab() -> KeyChord:
    """Tab-key chord."""
    return _key_special(lib.SC_KEY_TAB)


def key_backtab() -> KeyChord:
    """Shift-Tab (back-tab) chord."""
    return _key_special(lib.SC_KEY_BACKTAB)


def key_delete() -> KeyChord:
    """Delete-key chord."""
    return _key_special(lib.SC_KEY_DELETE)


def key_backspace() -> KeyChord:
    """Backspace-key chord."""
    return _key_special(lib.SC_KEY_BACKSPACE)


def key_home() -> KeyChord:
    """Home-key chord."""
    return _key_special(lib.SC_KEY_HOME)


def key_end() -> KeyChord:
    """End-key chord."""
    return _key_special(lib.SC_KEY_END)


def key_pageup() -> KeyChord:
    """Page-Up chord."""
    return _key_special(lib.SC_KEY_PAGEUP)


def key_pagedown() -> KeyChord:
    """Page-Down chord."""
    return _key_special(lib.SC_KEY_PAGEDOWN)


def key_esc() -> KeyChord:
    """Esc-key chord (bindable only where the widget captures Esc)."""
    return _key_special(lib.SC_KEY_ESC)


class Shortcuts:
    """A set of custom key bindings to attach to a widget via
    ``opts.shortcuts``.

    ``on_return`` ends the prompt and records an id (read afterwards with
    :meth:`fired`); ``on_callback`` runs a Python callable in place and keeps
    the prompt open unless the callable returns ``False``.
    """

    __slots__ = ("_specs", "_keep", "_out", "_help_rows", "_cur_section")

    def __init__(self) -> None:
        self._specs: list = []
        self._keep: list = []
        self._out = ffi.new("int *")
        self._out[0] = -1
        # Help-screen rows as (section, key_display, desc) py-string tuples,
        # in author order; built into cdata lazily in show_shortcuts.
        self._help_rows: list[tuple[str | None, str | None, str | None]] = []
        self._cur_section: str | None = None

    @staticmethod
    def _key_name(chord: KeyChord) -> str:
        buf = ffi.new("char[32]")
        lib.sc_key_chord_name(chord.value, buf, 32)
        return ffi.string(buf).decode("utf-8", "replace")

    def on_return(self, chord: KeyChord, id: int,  # noqa: A002
                  name: str | None = None, *, help: str | None = None,  # noqa: A002
                  in_footer: bool = True) -> "Shortcuts":
        """Bind ``chord`` to end the prompt and report ``id`` (see
        :meth:`fired`). ``name`` is the footer text; ``help`` is the
        help-screen description (defaults to ``name``); ``in_footer=False``
        keeps the binding but hides it from the footer."""
        self._specs.append(
            ("return", chord, int(id), None, name, help, in_footer,
             self._cur_section))
        self._help_rows.append(
            (None, self._key_name(chord), help if help is not None else name))
        return self

    def on_callback(self, chord: KeyChord, fn,
                    name: str | None = None, *, help: str | None = None,  # noqa: A002
                    in_footer: bool = True) -> "Shortcuts":
        """Bind ``chord`` to run ``fn()`` in place. ``fn`` returning ``False``
        closes the prompt; ``None``/``True`` keeps it open. ``name``/``help``/
        ``in_footer`` control the footer and help-screen display."""
        self._specs.append(
            ("callback", chord, 0, fn, name, help, in_footer,
             self._cur_section))
        self._help_rows.append(
            (None, self._key_name(chord), help if help is not None else name))
        return self

    def section(self, title: str) -> "Shortcuts":
        """Open a help-screen section: entries added afterwards group under
        ``title`` until the next :meth:`section`."""
        self._cur_section = title
        self._help_rows.append((title, None, None))
        return self

    def help_row(self, key_display: str, description: str) -> "Shortcuts":
        """Add a help-screen-only row (no binding), e.g. to document a built-in
        widget key like ``↑/↓`` "move cursor"."""
        self._help_rows.append((None, key_display, description))
        return self

    def fired(self) -> int:
        """Id of the RETURN-mode shortcut that ended the last run, else -1."""
        return self._out[0]

    @staticmethod
    def _make_cb(fn):
        @ffi.callback("bool(int, void*)")
        def cb(_id, _user):  # noqa: ANN001
            try:
                r = fn()
            except Exception:  # never let a Python error unwind into C
                return False
            return True if r is None else bool(r)
        return cb

    def _build(self, arena: list):
        """Returns ``(ScShortcut*, n, out_ptr)`` for the upcoming call."""
        self._keep = []
        n = len(self._specs)
        if n == 0:
            return ffi.NULL, 0, ffi.NULL
        arr = ffi.new("ScShortcut[]", n)
        for i, (mode, chord, id_, fn, name, help_, in_footer, section) \
                in enumerate(self._specs):
            arr[i].chord = chord.value
            arr[i].id = id_
            if mode == "return":
                arr[i].mode = _MODE_RETURN
            else:
                arr[i].mode = _MODE_CALLBACK
                cb = self._make_cb(fn)
                self._keep.append(cb)
                arr[i].on_fire = cb
            if name is not None:
                arr[i].hint_label = cstr(arena, name)
            if help_ is not None:
                arr[i].help_text = cstr(arena, help_)
            if section is not None:
                arr[i].section = cstr(arena, section)
            arr[i].hide_in_footer = not in_footer
        self._keep.append(arr)
        return arr, n, self._out


@dataclass
class ShortcutHelpOpts:
    """Options for :func:`show_shortcuts`."""

    title: str | None = None          #: None => "Keyboard shortcuts"
    accent: Color = Color.NONE        #: None/NONE => yellow (resolved in C)
    footer_hint: str | None = None    #: None => "type to filter · esc to close"
    #: True when the caller already holds an alternate screen (a long-running
    #: TUI): render full-screen without opening a second one. False (default)
    #: spans its own alternate screen for the duration.
    in_alt_screen: bool = False


def show_shortcuts(shortcuts: "Shortcuts",
                   opts: ShortcutHelpOpts | None = None) -> None:
    """Show the modal, scrollable keyboard-shortcut help screen built from a
    :class:`Shortcuts` set (sections, derived key names, descriptions and any
    :meth:`Shortcuts.help_row` lines, in author order). Blocks until Esc/Enter;
    a no-op without a TTY."""
    opts = opts or ShortcutHelpOpts()
    arena: list = []
    rows = shortcuts._help_rows
    n = len(rows)
    arr = ffi.new("ScShortcutHelpRow[]", n) if n else ffi.NULL
    for i, (section, key, desc) in enumerate(rows):
        if section is not None:
            arr[i].section = cstr(arena, section)
        if key is not None:
            arr[i].key_display = cstr(arena, key)
        if desc is not None:
            arr[i].desc = cstr(arena, desc)
    copts = ffi.new("ScShortcutHelpOpts *")
    if opts.title is not None:
        copts.title = cstr(arena, opts.title)
    apply_color(copts.accent, opts.accent)
    if opts.footer_hint is not None:
        copts.footer_hint = cstr(arena, opts.footer_hint)
    copts.in_alt_screen = opts.in_alt_screen
    lib.sc_shortcut_help_show(arr, n, copts)
    _ = arena  # keep strings alive across the call
