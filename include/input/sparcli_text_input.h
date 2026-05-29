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
 * to a format — digits only, no spaces, etc. Built-in filters below.
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
typedef struct {
    const char  *initial;       /**< Pre-filled value; `NULL` = empty. */
    const char  *placeholder;   /**< Dim hint shown while empty; may be `NULL`. */
    ScTextStyle  prompt_style;  /**< Style for the prompt label. */
    ScTextStyle  value_style;   /**< Style for the entered value. */
    ScTextStyle  cursor_style;  /**< Style of the cursor cell; zero-init =
                                     black-on-white. */
    ScTextStyle  error_style;   /**< Style of the validation error line; zero-init = red. */
    ScTextStyle  summary_style; /**< Style of the persistent summary line. */
    bool         hide_summary;  /**< Suppress the post-entry summary line. */
    int          max_chars;     /**< Max characters; 0 = unlimited (default).
                                     When > 0 the input is capped and the counter
                                     shows `count/max`. */
    bool         hide_char_count;/**< Suppress the character counter (shown by
                                     default below the field). */
    ScTextStyle  count_style;   /**< Style of the character counter; zero-init = dim. */
    bool         boxed;         /**< Render the field inside a bordered panel:
                                     prompt as the top title, counter on the
                                     bottom-right border. */
    ScBorderStyle border;       /**< Box border (boxed mode); zero-init type =
                                     rounded. */
    int          width;         /**< Field width in columns; 0 = terminal width.
                                     Long values scroll horizontally. In boxed
                                     mode this is the panel width. */
    const char  *hint;          /**< Key-hint footer; `NULL` = sensible default. */
    bool         hide_hint;     /**< Suppress the key-hint footer. */
    ScTextStyle  hint_style;    /**< Style of the footer; zero-init = dim. */
    ScCharFilter char_filter;   /**< Optional per-character input filter; may be `NULL`. */
    void        *char_filter_ctx;/**< Opaque pointer passed to `char_filter`. */
    const char *const *suggestions; /**< Autocomplete word list; may be `NULL`. The
                                     first case-insensitive prefix match is shown as
                                     dim ghost text; Tab accepts it. */
    size_t       n_suggestions; /**< Number of entries in `suggestions`. */
    ScValidateFn validate;      /**< Optional validator; may be `NULL`. */
    void        *validate_ctx;  /**< Opaque pointer passed to `validate`. */
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
typedef struct {
    const char  *placeholder;   /**< Dim hint shown while empty; may be `NULL`. */
    const char  *mask;          /**< Glyph per character; `NULL` = "*". Empty
                                     string ("") hides the length entirely. */
    ScTextStyle  prompt_style;  /**< Style for the prompt label. */
    ScTextStyle  cursor_style;  /**< Style of the cursor cell; zero-init =
                                     black-on-white. */
    ScTextStyle  error_style;   /**< Style of the validation error line; zero-init = red. */
    ScTextStyle  summary_style; /**< Style of the persistent summary line. */
    bool         hide_summary;  /**< Suppress the post-entry summary line. */
    int          max_chars;     /**< Max characters; 0 = unlimited (default). */
    bool         hide_char_count;/**< Suppress the character counter. */
    ScTextStyle  count_style;   /**< Style of the character counter; zero-init = dim. */
    bool         boxed;         /**< Render the field inside a bordered panel. */
    ScBorderStyle border;       /**< Box border (boxed mode); zero-init type = rounded. */
    int          width;         /**< Field width; 0 = terminal width (boxed: panel width). */
    const char  *hint;          /**< Key-hint footer; `NULL` = sensible default. */
    bool         hide_hint;     /**< Suppress the key-hint footer. */
    ScTextStyle  hint_style;    /**< Style of the footer; zero-init = dim. */
    ScCharFilter char_filter;   /**< Optional per-character input filter; may be `NULL`. */
    void        *char_filter_ctx;/**< Opaque pointer passed to `char_filter`. */
    ScValidateFn validate;      /**< Optional validator; may be `NULL`. */
    void        *validate_ctx;  /**< Opaque pointer passed to `validate`. */
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
