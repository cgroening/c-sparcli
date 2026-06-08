#pragma once

#include "core/sparcli_export.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_editor.h
 * @brief Launch an external editor on an existing file (tty-inheriting).
 *
 * Like the rest of the input side, this is *tty-oriented*: the editor inherits
 * the controlling terminal directly rather than going through the redirectable
 * `sc_output_stream()`. Unlike the internal temp-file editor used by the text
 * widgets, this edits an existing file in place and captures nothing - the
 * caller re-reads the file afterwards.
 */

/**
 * Opens an external editor on an existing file, inheriting the controlling
 * terminal, and waits for it to exit. The file is edited in place (no temp
 * file, no output capture).
 *
 * Call this only when no sparcli prompt / alternate-screen session is active
 * (i.e. with the terminal in its normal cooked state) - e.g. between finder
 * runs. It respects `SPARCLI_NO_TTY` / `sc_input_available()` and returns `-1`
 * when no controlling terminal is available, so callers can fall back.
 *
 * @param cmd  Editor command (whitespace-split into an argv, no shell). `NULL`
 *             or empty resolves `$VISUAL`, then `$EDITOR`, then a platform
 *             default (`nvim`/`vi` on POSIX, `notepad` on Windows).
 * @param path File to edit; must be non-empty.
 * @return     The editor's exit code (`0` = clean), `127` when the editor
 *             command was not found, or `-1` on a spawn/wait failure or when no
 *             controlling terminal is available.
 */
SPARCLI_EXPORT int sc_edit_file(const char *cmd, const char *path);

SPARCLI_END_DECLS
