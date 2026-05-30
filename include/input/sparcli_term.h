#pragma once

#include "core/sparcli_export.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_term.h
 * @brief Low-level terminal-input primitives shared by all input widgets.
 *
 * Unlike the output side of sparcli (which writes to the redirectable
 * `sc_output_stream()`), input is inherently *tty-oriented*: it needs raw
 * mode, decoded keystrokes and direct control of a real terminal. These
 * primitives talk to the controlling terminal directly and are the
 * foundation the higher-level widgets (confirm, text input, select, …)
 * are built on.
 */


/**
 * Outcome of an interactive prompt.
 *
 * Returned by every `sc_*` input widget. A pointer/result is only valid
 * when the status is `SC_INPUT_OK`.
 */
typedef enum ScInputStatus {
    SC_INPUT_OK = 0,    /**< User confirmed a value (Enter / selection). */
    SC_INPUT_CANCELLED, /**< User aborted (Esc or Ctrl-C). */
    SC_INPUT_ERROR,     /**< Not a TTY, read error, or allocation failure. */
} ScInputStatus;

/**
 * Layout of the dim key-hint footer every widget shows (e.g.
 * `↑/↓ move · enter select · esc cancel`).
 *
 * The zero-init value `SC_HINT_LAYOUT_DEFAULT` means "unset": it inherits the
 * process-wide theme (`ScInputTheme.hint_layout`) and, if that too is unset,
 * resolves to `SC_HINT_INLINE`. This mirrors the `ScWeekStart` sentinel so a
 * plain `0` cannot be mistaken for an explicit choice.
 */
typedef enum ScHintLayout {
    SC_HINT_LAYOUT_DEFAULT = 0, /**< Unset: inherit theme, else inline. */
    SC_HINT_INLINE,             /**< One `·`-separated line (the default). */
    SC_HINT_STACKED,            /**< One hint per line. */
    SC_HINT_HIDDEN,             /**< No footer at all. */
} ScHintLayout;

/**
 * Placement of the key-hint footer relative to the widget body.
 *
 * Orthogonal to `ScHintLayout`: any position combines with inline or stacked.
 * Left/right place the hint beside the widget, top-aligned. The zero-init
 * `SC_HINT_POS_DEFAULT` inherits the theme, then resolves to bottom (the
 * historical placement).
 */
typedef enum ScHintPosition {
    SC_HINT_POS_DEFAULT = 0, /**< Unset: inherit theme, else bottom. */
    SC_HINT_POS_TOP,         /**< Above the widget. */
    SC_HINT_POS_BOTTOM,      /**< Below the widget (the default). */
    SC_HINT_POS_LEFT,        /**< Left of the widget, top-aligned. */
    SC_HINT_POS_RIGHT,       /**< Right of the widget, top-aligned. */
} ScHintPosition;

/**
 * Logical key identity produced by the escape-sequence decoder.
 *
 * Printable input (including multi-byte UTF-8) is reported as
 * `SC_KEY_CHAR`; everything else maps to a named control key.
 */
typedef enum ScKeyType {
    SC_KEY_NONE = 0,
    SC_KEY_CHAR, /**< Printable codepoint; see `ScKey.codepoint`/`bytes`. */
    SC_KEY_ENTER,
    SC_KEY_ESC,
    SC_KEY_TAB,
    SC_KEY_BACKTAB, /**< Shift-Tab. */
    SC_KEY_BACKSPACE,
    SC_KEY_DELETE,
    SC_KEY_UP,
    SC_KEY_DOWN,
    SC_KEY_LEFT,
    SC_KEY_RIGHT,
    SC_KEY_HOME,
    SC_KEY_END,
    SC_KEY_PAGEUP,
    SC_KEY_PAGEDOWN,
    SC_KEY_SHIFT_PAGEUP,   /**< Shift+PageUp. */
    SC_KEY_SHIFT_PAGEDOWN, /**< Shift+PageDown. */
    SC_KEY_CTRL_A,
    SC_KEY_CTRL_C,
    SC_KEY_CTRL_D,
    SC_KEY_CTRL_E,
    SC_KEY_CTRL_K,
    SC_KEY_CTRL_U,
    SC_KEY_CTRL_W,
    SC_KEY_RESIZE, /**< Terminal resized (SIGWINCH); repaint and continue. */
} ScKeyType;

/** A single decoded key event. */
typedef struct ScKey {
    /** Logical key. */
    ScKeyType type;

    /** Unicode codepoint; valid for `SC_KEY_CHAR`. */
    uint32_t codepoint;

    /** UTF-8 bytes of the char, NUL-terminated. */
    char bytes[8];
} ScKey;

/**
 * Decodes a single key event from the front of `buf`.
 *
 * Pure function with no terminal dependency, so it is unit-testable: feed
 * it a byte sequence and inspect the resulting `ScKey`. Handles control
 * bytes, CSI/SS3 escape sequences (arrows, Home/End, Delete, PageUp/Down,
 * Shift+PageUp/Down, Shift-Tab) and UTF-8 multi-byte characters.
 *
 * @param buf  Input bytes.
 * @param len  Number of valid bytes in `buf`.
 * @param out  Receives the decoded key; set to `SC_KEY_NONE` when no key
 *             can be decoded.
 * @return     Number of bytes consumed. Returns 0 when `len == 0` or when
 *             `buf` holds only the prefix of an incomplete sequence (a lone
 *             ESC, or a partial UTF-8/CSI sequence) and more bytes are
 *             needed; the caller should read more and retry.
 */
SPARCLI_EXPORT size_t sc_key_decode(const char *buf, size_t len, ScKey *out);

/**
 * Returns `true` when an interactive prompt can run, i.e. a controlling
 * terminal is available for both reading and writing.
 *
 * Widgets call this internally and return `SC_INPUT_ERROR` when it is
 * `false` (output redirected to a pipe/file, no TTY in CI, …). Callers can
 * use it to decide whether to fall back to a non-interactive default.
 */
SPARCLI_EXPORT bool sc_input_available(void);

SPARCLI_END_DECLS
