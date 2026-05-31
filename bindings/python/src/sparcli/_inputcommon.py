"""Shared helpers for the interactive input widgets."""

from __future__ import annotations

from ._ffi import apply_style, cstr, ffi, lib
from .errors import SparcliInputUnavailable

_OK, _CANCELLED, _ERROR = 0, 1, 2


def result(status: int, value_fn):
    """Map an ``ScInputStatus`` to value / ``None`` / raised exception."""
    if status == _OK:
        return value_fn()
    if status == _CANCELLED:
        return None
    raise SparcliInputUnavailable("no controlling terminal or read error")


def take_cstr(out) -> str:
    """Decode a heap ``char**`` out-param into ``str`` and free the C buffer."""
    s = ffi.string(out[0]).decode("utf-8", "replace")
    lib.free(out[0])
    return s


def fill_hint(c, hint, layout, pos, style, arena: list) -> None:
    c.hint = cstr(arena, hint)
    c.hint_layout = int(layout)
    c.hint_pos = int(pos)
    apply_style(c.hint_style, style)


def fill_prompt_text(c, prompt_text, prompt_markup) -> None:
    if prompt_text is not None:
        c.prompt_text = prompt_text._ptr
    c.prompt_markup = bool(prompt_markup)


def fill_shortcuts(c, shortcuts, arena: list) -> None:
    if shortcuts is None:
        return
    arr, n, out = shortcuts._build(arena)
    c.shortcuts = arr
    c.n_shortcuts = n
    if out != ffi.NULL:
        c.out_shortcut_id = out


def fill_char_filter(c, cf, arena: list) -> None:
    if cf is None:
        return
    if isinstance(cf, ffi.CData):  # a built-in sc_filter_* function pointer
        c.char_filter = cf
        return

    @ffi.callback("bool(uint32_t, void*)")
    def cb(codepoint, _ctx):  # noqa: ANN001
        try:
            return bool(cf(chr(codepoint)))
        except Exception:
            return True

    arena.append(cb)
    c.char_filter = cb


def fill_validate(c, fn, arena: list) -> None:
    if fn is None:
        return

    @ffi.callback("bool(const char*, void*, const char**)")
    def cb(value, _ctx, err_out):  # noqa: ANN001
        try:
            res = fn(ffi.string(value).decode("utf-8", "replace"))
        except Exception:
            res = None
        if res:
            buf = ffi.new("char[]", str(res).encode("utf-8"))
            arena.append(buf)
            err_out[0] = buf
            return False
        return True

    arena.append(cb)
    c.validate = cb


def fill_suggestions(c, suggestions, arena: list) -> None:
    if not suggestions:
        return
    bufs = [cstr(arena, s) for s in suggestions]
    arr = ffi.new("char *[]", bufs)
    arena.append(arr)
    c.suggestions = ffi.cast("const char *const *", arr)
    c.n_suggestions = len(bufs)
