#pragma once

#include "core/sparcli_export.h"
#include "core/sparcli_core.h"
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

    /**
     * Longer description shown in the auto-built help screen
     * (`sc_shortcut_help_show_from`). `NULL` falls back to `hint_label`, so a
     * single short label can serve both the footer and the help screen; set
     * this when the help screen needs more detail than the footer.
     */
    const char *help_text;

    /**
     * Section title that groups this shortcut in the help screen (e.g.
     * `"Navigation"`). Consecutive shortcuts sharing a section render under one
     * header, in insertion order. `NULL` groups the shortcut under `"Other"`.
     */
    const char *section;

    /**
     * Keep the binding active but omit it from the key-hint footer. Unlike a
     * `NULL` `hint_label` (which also drops it from the help screen), this hides
     * only the footer entry; the shortcut still fires and still appears in the
     * help screen. Zero-init `false` = shown in the footer when it has a label.
     */
    bool hide_in_footer;
} ScShortcut;

/**
 * Builds a Ctrl-letter chord, e.g. `sc_key_ctrl('e')`.
 *
 * `letter` is case-insensitive. Ctrl-C (cancel), Ctrl-H (Backspace), Ctrl-I
 * (Tab), Ctrl-J/Ctrl-M (Enter) are not bindable - the terminal never delivers
 * them as a distinct Ctrl chord - and are rejected by a debug assert.
 */
SPARCLI_EXPORT ScKeyChord sc_key_ctrl(char letter);

/** Builds a function-key chord; `n` in `1..12` → F1..F12. */
SPARCLI_EXPORT ScKeyChord sc_key_fn(int n);

/** Builds an Alt/Meta + letter chord, e.g. `sc_key_alt('e')`. */
SPARCLI_EXPORT ScKeyChord sc_key_alt(char letter);

/**
 * Builds a chord for a named (non-character) key, e.g.
 * `sc_key_special(SC_KEY_LEFT)`. Use it to bind arrows, Enter, Tab, etc. - the
 * letter builders above only cover Ctrl/Alt + character keys.
 *
 * @param key  The `ScKeyType` to match (`SC_KEY_LEFT`, `SC_KEY_ENTER`, …).
 * @return     A chord with no modifiers that matches that key.
 */
SPARCLI_EXPORT ScKeyChord sc_key_special(ScKeyType key);

/**
 * Builds a chord for a named key with modifiers, e.g.
 * `sc_key_mod(SC_KEY_UP, SC_MOD_ALT)` (Alt+Up) or
 * `sc_key_mod(SC_KEY_UP, SC_MOD_SHIFT | SC_MOD_ALT)`. The decoder reports these
 * from the xterm `ESC[1;<mod>X` sequences. Shift only applies to named keys —
 * terminals fold Shift into the character for letters (use the uppercase
 * codepoint there). `sc_key_special(key)` == `sc_key_mod(key, 0)`.
 *
 * @param key   The `ScKeyType` to match (`SC_KEY_UP`, `SC_KEY_DELETE`, …).
 * @param mods  A bitmask of `SC_MOD_SHIFT`/`SC_MOD_ALT`/`SC_MOD_CTRL`.
 */
SPARCLI_EXPORT ScKeyChord sc_key_mod(ScKeyType key, uint8_t mods);

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


/* ── Auto-built help screen ──────────────────────────────────────────────── */

/**
 * One row of the keyboard-shortcut help screen: either a **section header** or
 * a **key/description** line.
 *
 * - A row with `section != NULL` is a group header; `key_display` and `desc`
 *   are ignored.
 * - Otherwise it is a help line: `key_display` is the (free-form) key text
 *   shown in the left column (e.g. `"↑/↓ or j/k"`, `"^E"`) and `desc` the
 *   description on the right.
 *
 * Help-only behaviour (built-in widget keys that are *not* bound `ScShortcut`s,
 * e.g. arrow navigation) is expressed by adding such key/description rows with
 * no backing chord. All strings are borrowed for the duration of the call.
 */
typedef struct ScShortcutHelpRow {
    const char *section;      /**< Non-NULL => section header (key/desc ignored). */
    const char *key_display;  /**< Left-column key text, e.g. "^E" or "↑/↓". */
    const char *desc;         /**< Right-column description. */
} ScShortcutHelpRow;

/** Options for the shortcut help screen. Zero-init friendly. */
typedef struct ScShortcutHelpOpts {
    /** Heading / search-field label; `NULL` = "Keyboard shortcuts". */
    const char *title;

    /** Accent color (box border, section headers); zero-init/none = yellow. */
    ScColor accent;

    /** Footer hint; `NULL` = "type to filter \xc2\xb7 esc to close". */
    const char *footer_hint;
} ScShortcutHelpOpts;

/**
 * Shows a modal, scrollable keyboard-shortcut help screen built from `rows`
 * (an inline rounded box; section headers in `accent`, the key column in
 * bold cyan). Blocks until the user presses Esc or Enter, then erases itself.
 * No-op without a TTY. `opts` may be `NULL` for defaults.
 *
 * @param rows  Display rows (headers + key/description lines), in order.
 * @param n     Number of rows.
 * @param opts  Optional styling/labels; `NULL` = defaults.
 */
SPARCLI_EXPORT void sc_shortcut_help_show(
    const ScShortcutHelpRow *rows, size_t n, const ScShortcutHelpOpts *opts
);

/**
 * Convenience: builds the help rows directly from a bound shortcut set and
 * shows them. Shortcuts are grouped by their `section` (NULL => "Other") in
 * insertion order, the key column is derived from each chord
 * (`sc_key_chord_name`), and the description is `help_text` or, when that is
 * `NULL`, `hint_label`. Shortcuts with neither description are skipped. For a
 * screen that also documents built-in widget keys, build a
 * `ScShortcutHelpRow` array yourself and call `sc_shortcut_help_show`.
 *
 * @param items  Bound shortcuts (the same array passed to a widget).
 * @param n      Number of shortcuts.
 * @param opts   Optional styling/labels; `NULL` = defaults.
 */
SPARCLI_EXPORT void sc_shortcut_help_show_from(
    const ScShortcut *items, size_t n, const ScShortcutHelpOpts *opts
);

SPARCLI_END_DECLS
