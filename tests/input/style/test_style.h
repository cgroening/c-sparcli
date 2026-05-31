#pragma once

#include "sparcli.h"

/**
 * Non-interactive style snapshot suite.
 *
 * Each `style_*` renders its widget in several styles via the internal frame
 * builders (declared in `src/input/input_internal.h`) and prints the captured
 * frames. No raw mode, no keystrokes - safe to run anywhere, including CI.
 */

/** Prints a dim caption then the frame (indented 2), and frees the frame. */
void style_show(const char *caption, ScRendered *frame);

/** Like `style_show` but flush-left (no indent) - for full-width frames that
 *  already span the whole terminal and must not be pushed past its edge. */
void style_show_flush(const char *caption, ScRendered *frame);

void style_confirm(void);
void style_text(void);
void style_number(void);
void style_textarea(void);
void style_select(void);
void style_fuzzy(void);
void style_datepicker(void);
