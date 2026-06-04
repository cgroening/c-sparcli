#pragma once

#include "core/sparcli_core.h"
#include "input/sparcli_term.h"
#include "input/sparcli_shortcut.h"
#include "input/sparcli_history.h"


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
typedef bool (*ScValidateFn)(
    const char *value, void *ctx, const char **err_out
);

/**
 * Per-character input filter. Return `true` to accept the typed codepoint,
 * `false` to reject it (the keystroke is ignored). Use it to constrain input
 * to a format - digits only, no spaces, etc. Built-in filters below.
 */
typedef bool (*ScCharFilter)(uint32_t codepoint, void *ctx);

/** Accepts ASCII digits `0`-`9` only. */
SPARCLI_EXPORT bool sc_filter_digits(uint32_t codepoint, void *ctx);
/** Accepts digits, a leading sign and a decimal separator (`0-9`, `-`, `+`,
    `.`, `,`). */
SPARCLI_EXPORT bool sc_filter_decimal(uint32_t codepoint, void *ctx);
/** Accepts ASCII letters only. */
SPARCLI_EXPORT bool sc_filter_alpha(uint32_t codepoint, void *ctx);
/** Accepts ASCII letters and digits only. */
SPARCLI_EXPORT bool sc_filter_alnum(uint32_t codepoint, void *ctx);
/** Rejects whitespace; accepts everything else. */
SPARCLI_EXPORT bool sc_filter_no_space(uint32_t codepoint, void *ctx);

/** How the autocomplete `suggestions` list is presented. */
typedef enum ScSuggestMode {
    SC_SUGGEST_GHOST = 0,  /**< Dim ghost text behind the cursor; Tab accepts. */
    SC_SUGGEST_DROPDOWN,   /**< Dropdown list below the field; arrows navigate. */
} ScSuggestMode;

/** How typed text is matched against the suggestion list (dropdown mode). */
typedef enum ScSuggestMatch {
    SC_SUGGEST_MATCH_PREFIX = 0,  /**< Case-insensitive prefix match. */
    SC_SUGGEST_MATCH_FUZZY,       /**< Fuzzy subsequence match, best first. */
} ScSuggestMatch;

/**
 * Presentation options for the autocomplete suggestion list.
 *
 * Zero-init keeps the classic ghost-text behavior. Set
 * `mode = SC_SUGGEST_DROPDOWN` for a navigable list below the field:
 * Up/Down move the highlight (Up past the first row returns to the field),
 * Tab accepts the highlighted row (or the best match when none is
 * highlighted), Enter accepts the highlighted row - or submits the typed
 * value when no row is highlighted.
 */
typedef struct ScSuggestOpts {
    /** Ghost text (zero-init) or dropdown list. */
    ScSuggestMode mode;

    /** Matching strategy; zero-init = case-insensitive prefix. */
    ScSuggestMatch match;

    /** Max dropdown rows shown at once; `0` = 5. More matches add a dim
        "… +N more" line. */
    int max_visible;

    /** Dropdown frame; zero-init type = plain list without a border. */
    ScBorderStyle border;

    /** Style of unselected dropdown rows; zero-init = no formatting. */
    ScTextStyle item_style;

    /** Style (incl. background) of the highlighted row; zero-init =
        bold cyan. */
    ScTextStyle selected_style;

    /** Style of the "… +N more" overflow line; zero-init = dim. */
    ScTextStyle more_style;

    /** Marker before the highlighted row; `NULL` = "‣ ". */
    const char *cursor_marker;

    /** Marker before unselected rows; `NULL` = two spaces. */
    const char *marker;
} ScSuggestOpts;

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
     * Optional frame: render the field inside a bordered panel (prompt as the
     * top title, counter on the bottom-right border) with a border, content
     * background, inner padding and outer margin. Zero-init = inline (no frame).
     * `box.width` is the field width in columns (`0` = terminal width); long
     * values scroll horizontally, and in boxed mode it is the panel width.
     * @see ScBoxStyle
     */
    ScBoxStyle box;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

    /** Style of the footer; zero-init = dim. */
    ScTextStyle hint_style;

    /** Optional per-character input filter; may be `NULL`. */
    ScCharFilter char_filter;

    /** Opaque pointer passed to `char_filter`. */
    void *char_filter_ctx;

    /**
     * Autocomplete word list; may be `NULL`. By default the first
     * case-insensitive prefix match is shown as dim ghost text and Tab
     * accepts it; see `suggest` for the dropdown presentation.
     */
    const char *const *suggestions;

    /** Number of entries in `suggestions`. */
    size_t n_suggestions;

    /** Presentation of `suggestions`: ghost text (zero-init) or dropdown. */
    ScSuggestOpts suggest;

    /** Optional validator; may be `NULL`. */
    ScValidateFn validate;

    /** Opaque pointer passed to `validate`. */
    void *validate_ctx;

    /** Custom key shortcuts; borrowed, must outlive the call.
        @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich prompt (mixed styles); overrides the string prompt.
        Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the string prompt as inline markup, e.g.
        "Rename [italic]x[/] to". */
    bool prompt_markup;

    /** Enable opening the value in an external editor (off by default). */
    bool external_editor;

    /** Editor command; `NULL`/empty = $VISUAL → $EDITOR → nvim → vi. */
    const char *editor;

    /** Key that opens the editor; zero-init = Ctrl-G.
        @see sparcli_shortcut.h */
    ScKeyChord editor_key;

    /**
     * Optional input history: Up/Down recall previous entries (newest
     * first); typing returns to the live line. While the suggestion
     * dropdown shows matches, it keeps priority over history on Up/Down.
     * Submitted lines are added automatically unless `no_history_add` (or
     * the handle's own `no_auto_add`) is set. Borrowed for the call.
     * @see sparcli_history.h
     */
    ScHistory *history;

    /** Do not auto-add the submitted line to `history` for this call. */
    bool no_history_add;
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

    /** Glyph per character; `NULL` = "*". Empty string ("") hides the
        length. */
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

    /** Optional frame: render the field inside a bordered panel with a border,
        content background, inner padding and outer margin. Zero-init = inline.
        `box.width` is the field width (`0` = terminal width; boxed = panel
        width). @see ScBoxStyle */
    ScBoxStyle box;

    /** Key-hint footer; `NULL` = sensible default. */
    const char *hint;

    /** Key-hint footer layout: inline (default) / stacked / hidden. */
    ScHintLayout hint_layout;

    /** Key-hint footer placement: top / bottom (default) / left / right. */
    ScHintPosition hint_pos;

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

    /** Custom key shortcuts; borrowed, must outlive the call.
        @see sparcli_shortcut.h */
    const ScShortcut *shortcuts;

    /** Number of entries in `shortcuts`. */
    size_t n_shortcuts;

    /** Optional: receives the fired shortcut id (RETURN mode), else `-1`. */
    int *out_shortcut_id;

    /** Optional rich prompt (mixed styles); overrides the string prompt.
        Borrowed. */
    const struct ScText *prompt_text;

    /** Parse the string prompt as inline markup. */
    bool prompt_markup;
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
