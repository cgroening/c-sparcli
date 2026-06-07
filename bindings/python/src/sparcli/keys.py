"""Key chords and custom shortcuts shared by every input widget."""

from __future__ import annotations

from ._ffi import cstr, ffi, lib

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

    __slots__ = ("_specs", "_keep", "_out")

    def __init__(self) -> None:
        self._specs: list = []
        self._keep: list = []
        self._out = ffi.new("int *")
        self._out[0] = -1

    def on_return(self, chord: KeyChord, id: int,  # noqa: A002
                  name: str | None = None) -> "Shortcuts":
        """Bind ``chord`` to end the prompt and report ``id``; see
        :meth:`fired`."""
        self._specs.append(("return", chord, int(id), None, name))
        return self

    def on_callback(self, chord: KeyChord, fn,
                    name: str | None = None) -> "Shortcuts":
        """Bind ``chord`` to run ``fn()`` in place. ``fn`` returning ``False``
        closes the prompt; ``None``/``True`` keeps it open."""
        self._specs.append(("callback", chord, 0, fn, name))
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
        for i, (mode, chord, id_, fn, name) in enumerate(self._specs):
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
        self._keep.append(arr)
        return arr, n, self._out
