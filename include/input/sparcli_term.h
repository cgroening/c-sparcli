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
typedef enum {
    SC_INPUT_OK = 0,    /**< User confirmed a value (Enter / selection). */
    SC_INPUT_CANCELLED, /**< User aborted (Esc or Ctrl-C). */
    SC_INPUT_ERROR,     /**< Not a TTY, read error, or allocation failure. */
} ScInputStatus;

/**
 * Logical key identity produced by the escape-sequence decoder.
 *
 * Printable input (including multi-byte UTF-8) is reported as
 * `SC_KEY_CHAR`; everything else maps to a named control key.
 */
typedef enum {
    SC_KEY_NONE = 0,
    SC_KEY_CHAR,       /**< Printable codepoint; see `ScKey.codepoint`/`bytes`. */
    SC_KEY_ENTER,
    SC_KEY_ESC,
    SC_KEY_TAB,
    SC_KEY_BACKTAB,    /**< Shift-Tab. */
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
    SC_KEY_CTRL_A,
    SC_KEY_CTRL_C,
    SC_KEY_CTRL_D,
    SC_KEY_CTRL_E,
    SC_KEY_CTRL_K,
    SC_KEY_CTRL_U,
    SC_KEY_CTRL_W,
} ScKeyType;

/**
 * A single decoded key event.
 */
typedef struct {
    ScKeyType type;       /**< Logical key. */
    uint32_t  codepoint;  /**< Unicode codepoint; valid for `SC_KEY_CHAR`. */
    char      bytes[8];   /**< UTF-8 bytes of the char, NUL-terminated. */
} ScKey;

/**
 * Decodes a single key event from the front of `buf`.
 *
 * Pure function with no terminal dependency, so it is unit-testable: feed
 * it a byte sequence and inspect the resulting `ScKey`. Handles control
 * bytes, CSI/SS3 escape sequences (arrows, Home/End, Delete, PageUp/Down,
 * Shift-Tab) and UTF-8 multi-byte characters.
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
