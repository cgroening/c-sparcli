#include "sparcli.h"
#include "core/sanitize_internal.h"
#include "core/text_internal.h"
#include "internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


/** Maximum nesting depth of style frames during parsing. */
#define MAX_STACK_DEPTH 32

/** Prefix that flips the following token into a background color. */
#define BG_PREFIX "on "

/** Length of `BG_PREFIX` (excluding the terminator). */
#define BG_PREFIX_LENGTH 3

/** Prefix that introduces an RGB color tuple `rgb(r,g,b)`. */
#define RGB_PREFIX "rgb("

/** Length of `RGB_PREFIX`. */
#define RGB_PREFIX_LENGTH 4

/** Prefix that introduces an OSC-8 hyperlink tag `[link=URL]`. */
#define LINK_PREFIX "link="

/** Length of `LINK_PREFIX`. */
#define LINK_PREFIX_LENGTH 5




/** State carried through one parse pass. */
typedef struct Parser {
    /** Source string being parsed; never advanced. */
    const char *source;

    /** Current read position in `source`. */
    const char *cursor;

    /** Start of the current verbatim text segment. */
    const char *segment_start;

    /** When `true`, unrecognized tags are silently dropped. */
    bool strip_unknown;

    /** Resolved ANSI passthrough for raw escape bytes in text chunks. */
    bool allow_ansi;

    /** Output `ScText`; owned by caller after `parse_internal` returns. */
    ScText *text;

    /** Style frame stack; `stack[0]` is the implicit no-style frame. */
    ScTextStyle stack[MAX_STACK_DEPTH];

    /** Index of the top frame on the stack. */
    int depth;

    /**
     * Count of opening tags that could not be pushed because the stack was
     * full (`depth + 1 >= MAX_STACK_DEPTH`). A matching close decrements this
     * before popping a real frame, so the stack stays balanced past the limit
     * (`[/]` always closes the most-recent open).
     */
    int dropped_opens;

    /**
     * Scrubbed URL of the active `[link=…]` tag; `NULL` = no link active.
     * While a link is active the body is treated as literal text: only
     * `[/link]` and `[/]` are recognized, everything else (including
     * nested tags) is part of the link text.
     */
    char *link_url;

    /**
     * User-supplied style for backtick code spans (`ScMarkupOpts.code_style`,
     * stored verbatim). The magenta default for an unset foreground is
     * applied at merge time, not here.
     */
    ScTextStyle code_style;
} Parser;


// Forward declarations indented to reflect call hierarchy
static ScText *parse_internal(const char *markup, ScMarkupOpts opts);
    static void init_parser(
        Parser *self, const char *markup, ScMarkupOpts opts
    );
    static bool consume_literal_bracket(Parser *self);
        static void append_chunk(
            const Parser *self,
            const char *start, size_t length, ScTextStyle style
        );
        static ScTextStyle current_style(const Parser *self);
    static bool consume_literal_backtick(Parser *self);
    static bool consume_code_span(Parser *self);
        static const char *find_code_end(const char *body_start);
        static void append_code_chunk(
            Parser *self, const char *start, size_t length
        );
            static void unescape_backticks(char *str);
            static ScTextStyle merge_code_style(
                ScTextStyle frame, ScTextStyle code
            );
    static bool consume_tag(Parser *self);
        static const char *find_tag_end(const char *open_bracket);
        static void handle_closing_tag(
            Parser *self,
            const char *tag_content, size_t tag_length, const char *tag_end
        );
            static bool is_recognized_close_name(
                const char *name, size_t length
            );
                static bool lookup_color_name(
                    const char *name, size_t length, ScColor *out
                );
        static void handle_opening_tag(
            Parser *self,
            const char *tag_content, size_t tag_length, const char *tag_end
        );
            static bool try_open_link_tag(
                Parser *self,
                const char *tag_content, size_t tag_length,
                const char *tag_end
            );
            static bool parse_open_tag(
                const char *content, size_t length,
                ScTextStyle base, ScTextStyle *out
            );
                static const char *skip_spaces(const char *p, const char *end);
                static bool consume_rgb_value(
                    const char **cursor_ref, const char *end, ScColor *out
                );
                static bool consume_bare_token(
                    const char **cursor_ref, const char *end,
                    bool target_is_bg, ScTextStyle *out
                );
                    static bool apply_attribute_token(
                        const char *token, size_t length, ScTextStyle *out
                    );
        static void drop_unknown_tag(Parser *self, const char *tag_end);
    static bool consume_link_close(Parser *self);
        static void flush_link_segment(Parser *self);
            static void append_link_chunk(
                Parser *self, const char *start, size_t length
            );
    static void flush_segment(Parser *self);

static void append_parsed_spans(ScText *target, const ScText *source);


ScText *sc_markup_parse(const char *markup) {
    return parse_internal(markup, (ScMarkupOpts){ 0 });
}

ScText *sc_markup_parse_opts(const char *markup, ScMarkupOpts opts) {
    return parse_internal(markup, opts);
}

void sc_markup_append(ScText *text, const char *markup) {
    if (!text) { return; }
    ScText *parsed = parse_internal(markup, (ScMarkupOpts){ 0 });
    append_parsed_spans(text, parsed);
    sc_text_free(parsed);
}

void sc_markup_append_opts(
    ScText *text, const char *markup, ScMarkupOpts opts
) {
    if (!text) { return; }
    ScText *parsed = parse_internal(markup, opts);
    append_parsed_spans(text, parsed);
    sc_text_free(parsed);
}

void sc_markup_print(const char *markup) {
    ScText *parsed = parse_internal(markup, (ScMarkupOpts){ 0 });
    sc_print_text(parsed);
    sc_text_free(parsed);
}

void sc_markup_print_opts(const char *markup, ScMarkupOpts opts) {
    ScText *parsed = parse_internal(markup, opts);
    sc_print_text(parsed);
    sc_text_free(parsed);
}

void sc_markup_println(const char *markup) {
    sc_markup_print(markup);
    fputc('\n', sc_output_stream());
}

void sc_markup_println_opts(const char *markup, ScMarkupOpts opts) {
    sc_markup_print_opts(markup, opts);
    fputc('\n', sc_output_stream());
}


/**
 * Core parser entry point. Returns a heap-allocated `ScText`; the caller
 * owns it and must free with `sc_text_free`.
 */
static ScText *parse_internal(const char *markup, ScMarkupOpts opts) {
    if (!markup) { return sc_text_new(); }

    Parser self;
    init_parser(&self, markup, opts);

    while (*self.cursor) {
        // Inside a [link=…] body only the closing tags are recognized
        if (self.link_url) {
            if (consume_link_close(&self)) { continue; }
            self.cursor++;
            continue;
        }
        if (consume_literal_bracket(&self)) { continue; }
        if (consume_literal_backtick(&self)) { continue; }
        if (consume_code_span(&self)) { continue; }
        if (consume_tag(&self)) { continue; }
        self.cursor++;
    }
    flush_segment(&self);

    return self.text;
}

/** Initializes the parser state with an empty stack frame on top. */
static void init_parser(
    Parser *self, const char *markup, ScMarkupOpts opts
) {
    self->source = markup;
    self->cursor = markup;
    self->segment_start = markup;
    self->strip_unknown = opts.strip_unknown;
    self->allow_ansi = sc_ansi_mode_resolve(opts.ansi);
    self->text = sc_text_new();
    self->depth = 0;
    self->dropped_opens = 0;
    self->link_url = NULL;
    self->code_style = opts.code_style;
    self->stack[0] = (ScTextStyle){
        SC_TEXT_ATTR_NONE,
        SC_ANSI_COLOR_NONE,
        SC_ANSI_COLOR_NONE,
    };
}

/**
 * Detects `[[` at the cursor: emits the pending segment, writes a literal
 * `[` span, and advances the cursor past the second `[`. Returns `true`
 * when the case applied.
 */
static bool consume_literal_bracket(Parser *self) {
    if (!(self->cursor[0] == '[' && self->cursor[1] == '[')) {
        return false;
    }

    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
    sc_text_append_raw(self->text, "[", current_style(self));
    self->cursor += 2;
    self->segment_start = self->cursor;
    return true;
}

/**
 * Detects `\`` at the cursor: emits the pending segment, writes a literal
 * backtick span, and advances the cursor past the escape. Returns `true`
 * when the case applied.
 */
static bool consume_literal_backtick(Parser *self) {
    if (!(self->cursor[0] == '\\' && self->cursor[1] == '`')) {
        return false;
    }

    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
    sc_text_append_raw(self->text, "`", current_style(self));
    self->cursor += 2;
    self->segment_start = self->cursor;
    return true;
}

/**
 * Detects a backtick-delimited `inline code` span at the cursor: both
 * backticks are removed and the body is emitted as one literal span (no
 * tag parsing) with the code style merged onto the current frame. Returns
 * `false` when no closing backtick exists, so the opening backtick stays
 * plain text (caller advances past it).
 */
static bool consume_code_span(Parser *self) {
    if (self->cursor[0] != '`') { return false; }

    const char *close = find_code_end(self->cursor + 1);
    if (!close) { return false; }

    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
    append_code_chunk(
        self, self->cursor + 1, (size_t)(close - self->cursor - 1)
    );
    self->cursor = close + 1;
    self->segment_start = self->cursor;
    return true;
}

/**
 * Returns a pointer to the closing backtick of a code span whose body
 * starts at `body_start`, or `NULL` when none exists. Escaped backticks
 * (`\``) inside the body do not close the span.
 */
static const char *find_code_end(const char *body_start) {
    const char *p = body_start;
    while (*p) {
        if (p[0] == '\\' && p[1] == '`') {
            p += 2;
            continue;
        }
        if (*p == '`') { return p; }
        p++;
    }
    return NULL;
}

/**
 * Appends the literal body of a code span: sanitizes it, collapses `\``
 * escapes into plain backticks, and emits one span styled with the code
 * style merged onto the current frame; does nothing when `length == 0`.
 */
static void append_code_chunk(
    Parser *self, const char *start, size_t length
) {
    if (length == 0) { return; }
    char *chunk = strndup(start, length);
    if (!chunk) { return; }

    // The code body is literal text: raw escape bytes are always stripped
    char *clean = sc_sanitize_copy(chunk, false);
    free(chunk);
    if (!clean) { return; }

    unescape_backticks(clean);
    sc_text_append_raw(
        self->text, clean,
        merge_code_style(current_style(self), self->code_style)
    );
    free(clean);
}

/** Collapses every `\`` escape in `str` to a plain backtick, in place. */
static void unescape_backticks(char *str) {
    char *write = str;
    for (const char *read = str; *read; read++) {
        if (read[0] == '\\' && read[1] == '`') {
            continue;   // skip the backslash; the backtick is copied next
        }
        *write++ = *read;
    }
    *write = '\0';
}

/**
 * Merges the user-supplied code style onto the surrounding style frame:
 * non-zero fields of `code` override `frame`; the foreground is always
 * replaced (default magenta when `code.fg` is unset).
 */
static ScTextStyle merge_code_style(ScTextStyle frame, ScTextStyle code) {
    ScTextStyle merged = frame;
    if (code.attr != SC_TEXT_ATTR_NONE) {
        merged.attr = code.attr;
    }
    merged.fg = sc_color_is_active(code.fg) ? code.fg : SC_ANSI_COLOR_MAGENTA;
    if (sc_color_is_active(code.bg)) {
        merged.bg = code.bg;
    }
    return merged;
}

/**
 * Detects an opening or closing tag at the cursor and dispatches to the
 * matching handler. Returns `true` when a tag was consumed, `false`
 * otherwise (caller advances past plain text).
 */
static bool consume_tag(Parser *self) {
    if (self->cursor[0] != '[') { return false; }

    const char *tag_end = find_tag_end(self->cursor);
    if (!tag_end) {
        self->cursor++;
        return true;
    }

    const char *tag_content = self->cursor + 1;
    size_t tag_length = (size_t)(tag_end - tag_content);

    if (tag_length == 0) {
        self->cursor = tag_end + 1;
        return true;
    }

    if (tag_content[0] == '/') {
        handle_closing_tag(self, tag_content, tag_length, tag_end);
    } else {
        handle_opening_tag(self, tag_content, tag_length, tag_end);
    }
    return true;
}

/** Returns a pointer to the closing `]` of the tag, or `NULL` when missing. */
static const char *find_tag_end(const char *open_bracket) {
    return strchr(open_bracket + 1, ']');
}

/**
 * Handles `[/]` or `[/name]`. Pops the top style frame when the close is
 * recognized; otherwise treats it as unknown (verbatim or stripped).
 */
static void handle_closing_tag(
    Parser *self,
    const char *tag_content, size_t tag_length, const char *tag_end
) {
    const char *close_name = tag_content + 1;
    size_t close_name_length = tag_length - 1;

    bool is_known = close_name_length == 0
        || is_recognized_close_name(close_name, close_name_length);

    if (is_known) {
        append_chunk(
            self, self->segment_start,
            (size_t)(self->cursor - self->segment_start),
            current_style(self)
        );
        if (self->dropped_opens > 0) {
            self->dropped_opens--;   // close an open that never fit the stack
        } else if (self->depth > 0) {
            self->depth--;
        }
        self->cursor = tag_end + 1;
        self->segment_start = self->cursor;
        return;
    }

    drop_unknown_tag(self, tag_end);
}

/**
 * Returns `true` when the bare `name` matches any known attribute keyword
 * or named color. Used to decide whether `[/name]` may close a frame.
 */
static bool is_recognized_close_name(const char *name, size_t length) {
    static const struct {
        const char *keyword;
        size_t length;
    } attributes[] = {
        { "bold",      4 },
        { "italic",    6 },
        { "underline", 9 },
        { "u",         1 },
        { "dim",       3 },
        { NULL,        0 },
    };

    for (int i = 0; attributes[i].keyword; i++) {
        if (attributes[i].length == length
            && memcmp(name, attributes[i].keyword, length) == 0) {
            return true;
        }
    }

    ScColor discarded;
    return lookup_color_name(name, length, &discarded);
}

/**
 * Returns `true` and writes the matching `ScColor` into `*out` when
 * `(name, length)` equals a known color name; returns `false` otherwise.
 *
 * Resolves both the ANSI color names and the named RGB palette via the shared
 * @ref sc_color_by_name_n, so markup tags like `[accent]` or `[error]` work.
 */
static bool lookup_color_name(
    const char *name, size_t length, ScColor *out
) {
    return sc_color_by_name_n(name, length, out);
}

/**
 * Handles an opening tag `[name attr...]`. Pushes a new style frame on
 * success; falls back to `drop_unknown_tag` when any token is unknown.
 */
static void handle_opening_tag(
    Parser *self,
    const char *tag_content, size_t tag_length, const char *tag_end
) {
    if (try_open_link_tag(self, tag_content, tag_length, tag_end)) {
        return;
    }

    ScTextStyle new_style;
    bool recognized = parse_open_tag(
        tag_content, tag_length, current_style(self), &new_style
    );

    if (!recognized) {
        drop_unknown_tag(self, tag_end);
        return;
    }

    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
    if (self->depth + 1 < MAX_STACK_DEPTH) {
        self->stack[++self->depth] = new_style;
    } else {
        self->dropped_opens++;   // remember it so its close stays balanced
    }
    self->cursor = tag_end + 1;
    self->segment_start = self->cursor;
}

/**
 * Detects `[link=URL]`: flushes the pending segment, scrubs and stores
 * the URL, and switches the parser into link-body mode. Returns `true`
 * when the tag was consumed (also when the URL is empty after scrubbing,
 * which falls back to unknown-tag handling).
 */
static bool try_open_link_tag(
    Parser *self,
    const char *tag_content, size_t tag_length, const char *tag_end
) {
    if (tag_length <= LINK_PREFIX_LENGTH
        || memcmp(tag_content, LINK_PREFIX, LINK_PREFIX_LENGTH) != 0) {
        return false;
    }

    char *url = strndup(
        tag_content + LINK_PREFIX_LENGTH, tag_length - LINK_PREFIX_LENGTH
    );
    if (!url) { return false; }
    char *scrubbed = sc_osc8_scrub_url(url);
    free(url);

    // A URL with no printable bytes cannot form a link: treat as unknown
    if (!scrubbed || scrubbed[0] == '\0') {
        free(scrubbed);
        drop_unknown_tag(self, tag_end);
        return true;
    }

    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
    self->link_url = scrubbed;
    self->cursor = tag_end + 1;
    self->segment_start = self->cursor;
    return true;
}

/**
 * Parses the space-separated tokens inside an opening tag. Inherits
 * `base` and writes the combined style into `*out`. Returns `true` when
 * all tokens were recognized.
 */
static bool parse_open_tag(
    const char *content, size_t length,
    ScTextStyle base, ScTextStyle *out
) {
    *out = base;
    const char *cursor = content;
    const char *end = content + length;

    while (cursor < end) {
        cursor = skip_spaces(cursor, end);
        if (cursor >= end) { break; }

        bool target_is_bg = false;
        if ((size_t)(end - cursor) >= BG_PREFIX_LENGTH
            && memcmp(cursor, BG_PREFIX, BG_PREFIX_LENGTH) == 0) {
            target_is_bg = true;
            cursor = skip_spaces(cursor + BG_PREFIX_LENGTH, end);
            if (cursor >= end) { return false; }
        }

        if ((size_t)(end - cursor) >= RGB_PREFIX_LENGTH
            && memcmp(cursor, RGB_PREFIX, RGB_PREFIX_LENGTH) == 0) {
            ScColor color;
            if (!consume_rgb_value(&cursor, end, &color)) { return false; }
            if (target_is_bg) { out->bg = color; } else { out->fg = color; }
            continue;
        }

        if (!consume_bare_token(&cursor, end, target_is_bg, out)) {
            return false;
        }
    }
    return true;
}

/** Advances past consecutive spaces. */
static const char *skip_spaces(const char *cursor, const char *end) {
    while (cursor < end && *cursor == ' ') { cursor++; }
    return cursor;
}

/**
 * Parses a single non-negative integer with `strtol`, advancing `*cursor`
 * past the digits. Returns `false` when no digit was consumed, when the
 * value would overflow `int`, or when the parsed value is out of the
 * `[0, 255]` channel range.
 *
 * Locale-independent (`strtol` is `LC_NUMERIC`-safe for plain decimal
 * integers - unlike `sscanf("%d", …)` which can be affected on some
 * implementations).
 */
static bool parse_rgb_channel(const char **cursor, int *out_value) {
    const char *start = *cursor;
    long value = 0;
    bool any_digit = false;
    while (**cursor >= '0' && **cursor <= '9') {
        value = value * 10 + (**cursor - '0');
        if (value > 255) { return false; }
        (*cursor)++;
        any_digit = true;
    }
    if (!any_digit) { return false; }
    (void)start;
    *out_value = (int)value;
    return true;
}

/**
 * Parses `rgb(r,g,b)` at `*cursor_ref`, validates the channel ranges and
 * advances `*cursor_ref` past the closing `)`. Returns `true` on success.
 */
static bool consume_rgb_value(
    const char **cursor_ref, const char *end, ScColor *out
) {
    const char *cursor = *cursor_ref + RGB_PREFIX_LENGTH;
    int red, green, blue;

    if (!parse_rgb_channel(&cursor, &red)) { return false; }
    if (*cursor != ',') { return false; }
    cursor++;
    if (!parse_rgb_channel(&cursor, &green)) { return false; }
    if (*cursor != ',') { return false; }
    cursor++;
    if (!parse_rgb_channel(&cursor, &blue)) { return false; }
    if (cursor >= end || *cursor != ')') { return false; }

    *out = sc_color_from_rgb(
        (uint8_t)red, (uint8_t)green, (uint8_t)blue
    );
    *cursor_ref = cursor + 1;
    return true;
}

/**
 * Parses one bare token (`bold`, `red`, etc.) at `*cursor_ref` into `out`
 * and advances the cursor past it. Foreground colors and attributes only
 * apply when `target_is_bg` is `false`; otherwise the token must be a
 * color name.
 */
static bool consume_bare_token(
    const char **cursor_ref, const char *end,
    bool target_is_bg, ScTextStyle *out
) {
    const char *cursor = *cursor_ref;
    const char *token = cursor;
    while (cursor < end && *cursor != ' ') { cursor++; }
    size_t token_length = (size_t)(cursor - token);
    *cursor_ref = cursor;

    if (target_is_bg) {
        ScColor color;
        if (!lookup_color_name(token, token_length, &color)) {
            return false;
        }
        out->bg = color;
        return true;
    }

    if (apply_attribute_token(token, token_length, out)) { return true; }

    ScColor color;
    if (lookup_color_name(token, token_length, &color)) {
        out->fg = color;
        return true;
    }
    return false;
}

/**
 * Applies `bold`/`italic`/`underline`/`u`/`dim` to `out->attr`. Returns
 * `true` when the token matched an attribute keyword.
 */
static bool apply_attribute_token(
    const char *token, size_t length, ScTextStyle *out
) {
    if (length == 4 && memcmp(token, "bold", 4) == 0) {
        out->attr |= SC_TEXT_ATTR_BOLD;
        return true;
    }
    if (length == 6 && memcmp(token, "italic", 6) == 0) {
        out->attr |= SC_TEXT_ATTR_ITALIC;
        return true;
    }
    if (length == 9 && memcmp(token, "underline", 9) == 0) {
        out->attr |= SC_TEXT_ATTR_UNDER;
        return true;
    }
    if (length == 1 && memcmp(token, "u", 1) == 0) {
        out->attr |= SC_TEXT_ATTR_UNDER;
        return true;
    }
    if (length == 3 && memcmp(token, "dim", 3) == 0) {
        out->attr |= SC_TEXT_ATTR_DIM;
        return true;
    }
    return false;
}

/**
 * Handles an unrecognized tag: when `strip_unknown` is set, flushes the
 * verbatim segment and skips the tag; otherwise leaves the tag in the
 * output stream so it is emitted as plain text on the next flush.
 */
static void drop_unknown_tag(Parser *self, const char *tag_end) {
    if (self->strip_unknown) {
        append_chunk(
            self, self->segment_start,
            (size_t)(self->cursor - self->segment_start),
            current_style(self)
        );
        self->segment_start = tag_end + 1;
    }
    self->cursor = tag_end + 1;
}

/**
 * Detects `[/link]` or `[/]` while a link is active: flushes the link
 * body wrapped in OSC-8 escape sequences and leaves link-body mode.
 * Returns `true` when a closing tag was consumed.
 */
static bool consume_link_close(Parser *self) {
    static const char *const close_tags[] = { "[/link]", "[/]", NULL };

    for (int i = 0; close_tags[i]; i++) {
        size_t tag_length = strlen(close_tags[i]);
        if (strncmp(self->cursor, close_tags[i], tag_length) != 0) {
            continue;
        }
        flush_link_segment(self);
        self->cursor += tag_length;
        self->segment_start = self->cursor;
        return true;
    }
    return false;
}

/**
 * Flushes the pending link body as one OSC-8 wrapped span and clears the
 * link state.
 */
static void flush_link_segment(Parser *self) {
    append_link_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start)
    );
    free(self->link_url);
    self->link_url = NULL;
}

/**
 * Sanitizes `[start, start+length)`, wraps it in OSC-8 hyperlink escape
 * sequences pointing at the parser's active link URL, and appends it as
 * one span with the current style; does nothing when `length == 0`.
 */
static void append_link_chunk(
    Parser *self, const char *start, size_t length
) {
    if (length == 0) { return; }
    char *chunk = strndup(start, length);
    if (!chunk) { return; }

    // The link body is literal text: raw escape bytes are always stripped
    char *clean = sc_sanitize_copy(chunk, false);
    free(chunk);
    if (!clean) { return; }

    char *wrapped = sc_osc8_wrap(clean, self->link_url);
    if (wrapped) {
        sc_text_append_raw(self->text, wrapped, current_style(self));
        free(wrapped);
    }
    free(clean);
}

/**
 * Flushes the remaining verbatim segment at end of parsing. An
 * unterminated `[link=…]` body is still emitted as a link.
 */
static void flush_segment(Parser *self) {
    if (self->link_url) {
        flush_link_segment(self);
        return;
    }
    append_chunk(
        self, self->segment_start,
        (size_t)(self->cursor - self->segment_start),
        current_style(self)
    );
}

/** Returns the style frame currently on top of the parser stack. */
static ScTextStyle current_style(const Parser *self) {
    return self->stack[self->depth];
}

/**
 * Appends `[start, start+length)` to the parser's output with `style`,
 * sanitizing it per the parser's resolved ANSI mode; does nothing when
 * `length == 0`.
 */
static void append_chunk(
    const Parser *self, const char *start, size_t length, ScTextStyle style
) {
    if (length == 0) { return; }
    char *chunk = strndup(start, length);
    if (!chunk) { return; }
    char *clean = sc_sanitize_copy(chunk, self->allow_ansi);
    if (clean) {
        sc_text_append_raw(self->text, clean, style);
        free(clean);
    }
    free(chunk);
}

/** Copies every span from `source` into `target` (already sanitized). */
static void append_parsed_spans(ScText *target, const ScText *source) {
    for (size_t i = 0; i < source->count; i++) {
        sc_text_append_raw(
            target, source->spans[i].raw_str, source->spans[i].style
        );
    }
}
