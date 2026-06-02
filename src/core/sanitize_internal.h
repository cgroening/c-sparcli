#pragma once

#include "sparcli.h"

#include <stdbool.h>
#include <stddef.h>


/**
 * @file sanitize_internal.h
 * @brief Internal ANSI/control-byte sanitizer shared by all widgets.
 *
 * Every user-supplied string crosses the library trust boundary exactly
 * once, through one of these helpers. Library-rendered content
 * (`ScRendered`, captured columns output, …) is inside the boundary and
 * must never be re-sanitized.
 */


/**
 * Resolves a widget's tri-state `ScAnsiMode` against the global setting.
 *
 * `SC_ANSI_MODE_DEFAULT` returns `sc_allow_ansi()`; the explicit values
 * override it.
 */
bool sc_ansi_mode_resolve(ScAnsiMode mode);

/**
 * Returns a heap-allocated copy of `str` with control bytes removed.
 *
 * Always removes C0 control bytes (except `\n` and `\t`) and DEL (0x7F).
 * When `allow_ansi` is `false`, well-formed ANSI escape sequences are
 * removed as well; when `true` they are copied through verbatim. A lone
 * or malformed ESC byte is dropped in both modes.
 *
 * Returns `NULL` when `str` is `NULL` or allocation fails; never returns
 * the input pointer. Caller owns the result.
 */
char *sc_sanitize_copy(const char *str, bool allow_ansi);

/** Convenience wrapper: resolve `mode`, then `sc_sanitize_copy`. */
char *sc_sanitize_copy_mode(const char *str, ScAnsiMode mode);

/**
 * Returns a heap-allocated copy of `url` reduced to printable ASCII
 * (0x20-0x7E).
 *
 * Control bytes (including ESC and BEL) and bytes >= 0x7F are removed so
 * the result can be embedded in an OSC-8 hyperlink sequence without
 * terminating it early or injecting a nested escape sequence.
 *
 * Returns `NULL` when `url` is `NULL` or allocation fails. Caller owns
 * the result.
 */
char *sc_osc8_scrub_url(const char *url);

/**
 * Builds the OSC-8 hyperlink byte sequence for `text` linking to `url`.
 *
 * Produces `ESC ] 8 ; ; url ST text ESC ] 8 ; ; ST` as one heap string.
 * Both inputs must already be sanitized/scrubbed (trusted side); the
 * result is meant for `sc_text_append_raw`.
 *
 * Returns `NULL` when either input is `NULL` or allocation fails. Caller
 * owns the result.
 */
char *sc_osc8_wrap(const char *text, const char *url);

/**
 * Returns the pointer just past the well-formed ANSI escape sequence
 * starting at `p` (where `*p` must be ESC), or `p` itself when the bytes
 * do not form a valid, complete sequence.
 *
 * Handled sequence kinds (ECMA-48):
 * - CSI:  `ESC [` parameter/intermediate bytes, final byte 0x40-0x7E
 * - OSC:  `ESC ]` … terminated by BEL or ST (`ESC \`)
 * - DCS/SOS/PM/APC: `ESC P` / `ESC X` / `ESC ^` / `ESC _` … ST
 * - Other escape sequences: `ESC` intermediates (0x20-0x2F), final
 *   byte 0x30-0x7E (e.g. `ESC c`, `ESC ( B`)
 *
 * Bounds-checked against the NUL terminator: an unterminated sequence
 * returns `p` (treated as malformed).
 */
const char *sc_ansi_skip_seq(const char *p);
