#pragma once

#include "core/sparcli_export.h"
#include "input/sparcli_term.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_shortcut.h
 * @brief Custom key shortcuts shared by every interactive widget.
 *
 * A shortcut binds a key chord (Ctrl-letter, function key, or Alt-letter) to
 * an application action. Bindings are passed to any widget through the
 * `shortcuts` / `n_shortcuts` fields of its `Sc*Opts`; the prompt engine
 * matches them uniformly, so the behavior is identical across confirm, text
 * input, select, fuzzy, and the rest.
 *
 * Two modes:
 * - `SC_SHORTCUT_RETURN` ends the prompt; the widget still returns its normal
 *   value, and the fired shortcut's `id` is written to `*out_shortcut_id`.
 * - `SC_SHORTCUT_CALLBACK` runs `on_fire` in place; the prompt stays open
 *   unless the callback returns `false`.
 *
 * Esc and Ctrl-C stay reserved for cancel and cannot be bound. Binding a key a
 * widget already uses (e.g. Ctrl-E in a text field) shadows that built-in use.
 */

/** A key combination to match against a decoded `ScKey`. */
typedef struct ScKeyChord {
    /** `SC_KEY_F1..F12`, a named `SC_KEY_CTRL_*`, or `SC_KEY_CHAR`. */
    ScKeyType key;

    /** Letter codepoint for `SC_KEY_CHAR` chords (Ctrl/Alt + letter). */
    uint32_t codepoint;

    /** Modifier bitmask (`ScKeyMods`). */
    uint8_t mods;
} ScKeyChord;

/** How a fired shortcut affects the running prompt. */
typedef enum ScShortcutMode {
    SC_SHORTCUT_RETURN = 0, /**< End the prompt and report the id. */
    SC_SHORTCUT_CALLBACK, /**< Run `on_fire`; stay open unless it returns
                               false. */
} ScShortcutMode;

/** One key binding. */
typedef struct ScShortcut {
    /** Key combination that triggers this shortcut. */
    ScKeyChord chord;

    /** Caller-defined id reported via `*out_shortcut_id` in RETURN mode
        (>= 0). */
    int id;

    /** Behavior when fired. */
    ScShortcutMode mode;

    /**
     * Callback for `SC_SHORTCUT_CALLBACK` mode; receives `id` and `user`.
     * Return `true` to keep the prompt open, `false` to close it. Ignored in
     * RETURN mode. Runs while the single prompt session is held, so it must
     * not start another prompt (that would fail with `SC_INPUT_ERROR`).
     */
    bool (*on_fire)(int id, void *user);

    /** Opaque pointer passed to `on_fire`. */
    void *user;

    /**
     * Optional short label shown in the widget's key-hint footer, e.g.
     * `"delete"` renders as `^X delete`. `NULL` hides this shortcut from the
     * footer (it still fires). The key name is derived from the chord.
     */
    const char *hint_label;
} ScShortcut;

/**
 * Builds a Ctrl-letter chord, e.g. `sc_key_ctrl('e')`.
 *
 * `letter` is case-insensitive. Ctrl-C (cancel), Ctrl-H (Backspace), Ctrl-I
 * (Tab), Ctrl-J/Ctrl-M (Enter) are not bindable – the terminal never delivers
 * them as a distinct Ctrl chord – and are rejected by a debug assert.
 */
SPARCLI_EXPORT ScKeyChord sc_key_ctrl(char letter);

/** Builds a function-key chord; `n` in `1..12` → F1..F12. */
SPARCLI_EXPORT ScKeyChord sc_key_fn(int n);

/** Builds an Alt/Meta + letter chord, e.g. `sc_key_alt('e')`. */
SPARCLI_EXPORT ScKeyChord sc_key_alt(char letter);

/**
 * Returns `true` when `key` matches `chord`. Normalizes the named
 * `SC_KEY_CTRL_*` keys against `SC_KEY_CHAR + SC_MOD_CTRL`, so
 * `sc_key_ctrl('e')` matches a decoded Ctrl-E however it was encoded.
 */
SPARCLI_EXPORT bool sc_key_chord_matches(ScKey key, ScKeyChord chord);

/**
 * Returns the first shortcut in `items[0..n)` whose chord matches `key`, or
 * `NULL` when none match.
 */
SPARCLI_EXPORT const ScShortcut *sc_shortcut_find(
    ScKey key, const ScShortcut *items, size_t n
);

/**
 * Writes a short human-readable name for `chord` into `buf` (e.g. `"F2"`,
 * `"^E"`, `"M-e"`). Always NUL-terminates when `cap > 0`. Used to build the
 * key-hint footer for labeled shortcuts.
 */
SPARCLI_EXPORT void sc_key_chord_name(ScKeyChord chord, char *buf, size_t cap);

SPARCLI_END_DECLS
