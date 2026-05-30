#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"


SPARCLI_BEGIN_DECLS

/**
 * @file sparcli_text_input.h
 * @brief Single-line text entry and masked password entry.
 *
 * Both widgets share one line-editing core (cursor movement, insert,
 * backspace/delete, word-delete, Home/End). The password variant differs
 * only in that each character is rendered as a mask glyph.
 */

/**
 * Validation callback. Return `true` when `value` is acceptable; otherwise
 * return `false` and optionally set `*err_out` to a static/owned message
 * that the widget displays beneath the field (the prompt stays open).
 */
typedef bool (*ScValidateFn)(const char *value, void *ctx, const char **err_out);

/**
 * Per-character input filter. Return `true` to accept the typed codepoint,
 * `false` to reject it (the keystroke is ignored). Use it to constrain input
 * to a format – digits only, no spaces, etc. Built-in filters below.
 */
typedef bool (*ScCharFilter)(uint32_t codepoint, void *ctx);

/** Accepts ASCII digits `0`–`9` only. */
SPARCLI_EXPORT bool sc_filter_digits(uint32_t codepoint, void *ctx);
/** Accepts digits, a leading sign and a decimal point (`0-9`, `-`, `+`, `.`). */
SPARCLI_EXPORT bool sc_filter_decimal(uint32_t codepoint, void *ctx);
/** Accepts ASCII letters only. */
SPARCLI_EXPORT bool sc_filter_alpha(uint32_t codepoint, void *ctx);
/** Accepts ASCII letters and digits only. */
SPARCLI_EXPORT bool sc_filter_alnum(uint32_t codepoint, void *ctx);
/** Rejects whitespace; accepts everything else. */
SPARCLI_EXPORT bool sc_filter_no_space(uint32_t codepoint, void *ctx);

/** Options for `sc_text_input`. */
typedef struct ScTextInputOpts {
    /** Pre-filled value; `NULL` = empty. */
    const char *initial;

    /** Dim hint shown while empty; may be `NULL`. */
    const char *placeholder;

    /** Style for the prompt label. */
    ScTextStyle prompt_style;

    /** Style for the entered value. */
    ScTextStyle value_style;

    /** Style of the cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Style of the validation error line; zero-init = red. */
    ScTextStyle error_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-entry summary line. */
    bool hide_summary;

    /**
     * Max characters; `0` = unlimited. When `> 0` the input is capped and the
     * counter shows `count/max`.
     */
    int max_chars;

    /** Suppress the character counter (shown by default below the field). */
    bool hide_char_count;

    /** Style of the character counter; zero-init = dim. */
    ScTextStyle count_style;

    /**
     * Render the field inside a bordered panel: prompt as the top title,
     * counter on the bottom-right border.
     */
    bool boxed;

    /** Box border (boxed mode); zero-init type = rounded. */
    ScBorderStyle border;

    /**
     * Field width in columns; `0` = terminal width. Long values scroll
     * horizontally. In boxed mode this is the panel width.
     */
    int width;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Suppress the key-hint footer. */
    bool hide_hint;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Optional per-character input filter; may be `NULL`. */
    ScCharFilter char_filter;

    /** Opaque pointer passed to `char_filter`. */
    void *char_filter_ctx;

    /**
     * Autocomplete word list; may be `NULL`. The first case-insensitive prefix
     * match is shown as dim ghost text; Tab accepts it.
     */
    const char *const *suggestions;

    /** Number of entries in `suggestions`. */
    size_t n_suggestions;

    /** Optional validator; may be `NULL`. */
    ScValidateFn validate;

    /** Opaque pointer passed to `validate`. */
    void *validate_ctx;
} ScTextInputOpts;

/**
 * Prompts for a single line of text.
 *
 * On `SC_INPUT_OK`, `*out` receives a heap-allocated string the caller must
 * `free()`. `*out` is not modified on cancel/error.
 *
 * @param prompt  Label shown before the field. Must not be `NULL`.
 * @param out     Receives the entered string (heap; caller frees).
 * @param opts    Rendering and validation options.
 */
SPARCLI_EXPORT ScInputStatus sc_text_input(
    const char *prompt, char **out, ScTextInputOpts opts
);

/** Options for `sc_password_input`. */
typedef struct ScPasswordOpts {
    /** Dim hint shown while empty; may be `NULL`. */
    const char *placeholder;

    /** Glyph per character; `NULL` = "*". Empty string ("") hides the length. */
    const char *mask;

    /** Style for the prompt label. */
    ScTextStyle prompt_style;

    /** Style of the cursor cell; zero-init = black-on-white. */
    ScTextStyle cursor_style;

    /** Style of the validation error line; zero-init = red. */
    ScTextStyle error_style;

    /** Style of the persistent summary line. */
    ScTextStyle summary_style;

    /** Suppress the post-entry summary line. */
    bool hide_summary;

    /** Max characters; `0` = unlimited. */
    int max_chars;

    /** Suppress the character counter. */
    bool hide_char_count;

    /** Style of the character counter; zero-init = dim. */
    ScTextStyle count_style;

    /** Render the field inside a bordered panel. */
    bool boxed;

    /** Box border (boxed mode); zero-init type = rounded. */
    ScBorderStyle border;

    /** Field width; `0` = terminal width (boxed: panel width). */
    int width;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Suppress the key-hint footer. */
    bool hide_hint;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Optional per-character input filter; may be `NULL`. */
    ScCharFilter char_filter;

    /** Opaque pointer passed to `char_filter`. */
    void *char_filter_ctx;

    /** Optional validator; may be `NULL`. */
    ScValidateFn validate;

    /** Opaque pointer passed to `validate`. */
    void *validate_ctx;
} ScPasswordOpts;

/**
 * Prompts for a secret, rendering each character as a mask glyph.
 *
 * On `SC_INPUT_OK`, `*out` receives a heap-allocated string the caller must
 * `free()`.
 *
 * @param prompt  Label shown before the field. Must not be `NULL`.
 * @param out     Receives the entered secret (heap; caller frees).
 * @param opts    Rendering and validation options.
 */
SPARCLI_EXPORT ScInputStatus sc_password_input(
    const char *prompt, char **out, ScPasswordOpts opts
);

SPARCLI_END_DECLS
