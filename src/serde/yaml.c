#include "serde_internal.h"
#include "serde/sparcli_yaml.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file yaml.c
 * @brief A documented YAML-subset reader and writer over `ScValue`.
 *
 * The reader is indentation-based: the source is split into logical lines
 * (comment-stripped) and a recursive descent over them builds block mappings
 * and sequences; flow collections and quoted/plain scalars are parsed within a
 * line. See sparcli_yaml.h for the exact supported subset.
 */


/** Maximum container nesting (recursion guard). */
#define YAML_MAX_DEPTH 100

/** Default indentation width for the writer. */
#define YAML_DEFAULT_INDENT 2

/** Scratch buffer for classifying/parsing a scalar token. */
#define YAML_SCALAR_BUFFER 64


/** One raw source line (without the trailing newline). */
typedef struct YamlLine {
    const char *text;
    size_t      len;
} YamlLine;

/** Parser over the array of source lines. */
typedef struct YamlParser {
    YamlLine     *lines;
    size_t        count;
    size_t        cursor;
    int           depth;
    bool          has_error;
    ScParseError *err;
    const char   *origin; /**< Start of the working copy (for error offsets). */
} YamlParser;


// Reader forward declarations, indented to reflect the call hierarchy.
static ScValue *parse_node(YamlParser *ps, size_t indent);
    static ScValue *parse_mapping(
        YamlParser *ps, size_t indent, const char *seed, size_t seed_len
    );
    static ScValue *parse_sequence(YamlParser *ps, size_t indent);
        static ScValue *parse_child_block(YamlParser *ps, size_t parent_indent);
        static bool mapping_add_entry(
            YamlParser *ps, ScValue *map, const char *content, size_t len,
            size_t indent
        );
            static ScValue *parse_inline_value(
                YamlParser *ps, const char *s, size_t len
            );
                static ScValue *parse_flow(
                    YamlParser *ps, const char **p, const char *end
                );
                static ScValue *parse_quoted(
                    YamlParser *ps, const char *s, size_t len, char quote
                );
                static ScValue *infer_scalar(const char *s, size_t len);
            static ScValue *parse_block_scalar(
                YamlParser *ps, size_t parent_indent, char style
            );
static void content_view(
    YamlLine line, const char **out, size_t *out_len, size_t *indent
);
    static long find_colon(const char *s, size_t len);
    static bool is_seq_indicator(const char *s, size_t len);
static void skip_blank(YamlParser *ps);
static bool fail(YamlParser *ps, const char *message);
static ScValue *fail_value(YamlParser *ps, const char *message);
static char *dup_n(const char *text, size_t len);
static ScValue *make_string(const char *s, size_t len);

// Writer forward declarations.
static bool write_block(
    ScSerdeBuf *buf, const ScValue *value, int indent,
    const ScYamlWriteOpts *opts
);
    static bool write_after_key(
        ScSerdeBuf *buf, const ScValue *child, int indent,
        const ScYamlWriteOpts *opts
    );
    static bool write_scalar(ScSerdeBuf *buf, const ScValue *value);
        static bool write_yaml_string(ScSerdeBuf *buf, const char *text);
        static bool is_plain_safe(const char *text);
    static bool write_indent(ScSerdeBuf *buf, int indent);
    static const char **sorted_keys(const ScValue *map, size_t *count_out);


/* ── Public reader ─────────────────────────────────────────────────────── */

ScValue *sc_yaml_parse(const char *src, size_t len, ScParseError *err) {
    if (err) {
        *err = (ScParseError){ 0 };
    }
    if (!src) {
        len = 0;
    }

    char *copy = malloc(len + 1);
    if (!copy) {
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, src, len);
    }
    copy[len] = '\0';

    // Split into lines (strip a trailing CR from CRLF endings).
    size_t capacity = 8;
    YamlLine *lines = malloc(capacity * sizeof *lines);
    size_t count = 0;
    bool alloc_ok = lines != NULL;
    const char *p = copy;
    const char *end = copy + len;
    while (alloc_ok && p <= end) {
        const char *line_end = p;
        while (line_end < end && *line_end != '\n') {
            line_end++;
        }
        size_t line_len = (size_t)(line_end - p);
        if (line_len > 0 && p[line_len - 1] == '\r') {
            line_len--;
        }
        if (count == capacity) {
            capacity *= 2;
            YamlLine *grown = realloc(lines, capacity * sizeof *lines);
            if (!grown) {
                alloc_ok = false;
                break;
            }
            lines = grown;
        }
        lines[count].text = p;
        lines[count].len = line_len;
        count++;
        if (line_end >= end) {
            break;
        }
        p = line_end + 1;
    }

    ScValue *root = NULL;
    if (alloc_ok) {
        YamlParser ps = {
            .lines = lines, .count = count, .cursor = 0, .depth = 0,
            .has_error = false, .err = err, .origin = copy,
        };
        // Skip a leading document marker (`---`).
        skip_blank(&ps);
        if (ps.cursor < ps.count) {
            const char *content;
            size_t clen;
            size_t indent;
            content_view(ps.lines[ps.cursor], &content, &clen, &indent);
            if (clen == 3 && memcmp(content, "---", 3) == 0) {
                ps.cursor++;
            }
        }
        skip_blank(&ps);
        if (ps.cursor >= ps.count) {
            root = sc_value_null();
        } else {
            const char *content;
            size_t clen;
            size_t indent;
            content_view(ps.lines[ps.cursor], &content, &clen, &indent);
            root = parse_node(&ps, indent);
        }
        if (root && err && err->message[0] != '\0') {
            // A nested error was recorded after a partial success.
            sc_value_free(root);
            root = NULL;
        }
    } else if (err) {
        snprintf(err->message, sizeof err->message, "out of memory");
    }

    free(lines);
    free(copy);
    return root;
}


/* ── Block structure ───────────────────────────────────────────────────── */

/** Parses the node beginning at the current line, known to sit at `indent`. */
static ScValue *parse_node(YamlParser *ps, size_t indent) {
    if (ps->depth + 1 > YAML_MAX_DEPTH) {
        return fail_value(ps, "maximum nesting depth exceeded");
    }

    const char *content;
    size_t clen;
    size_t line_indent;
    content_view(ps->lines[ps->cursor], &content, &clen, &line_indent);

    if (is_seq_indicator(content, clen)) {
        return parse_sequence(ps, indent);
    }
    if (find_colon(content, clen) >= 0) {
        return parse_mapping(ps, indent, NULL, 0);
    }
    // A bare scalar node (single line).
    ScValue *value = parse_inline_value(ps, content, clen);
    ps->cursor++;
    return value;
}

/**
 * Parses a block mapping at `indent`. When `seed` is non-NULL it is processed
 * as the first entry (used for the inline `- key: value` sequence-item form),
 * and the caller has already consumed that line.
 */
static ScValue *parse_mapping(
    YamlParser *ps, size_t indent, const char *seed, size_t seed_len
) {
    ps->depth++;
    ScValue *map = sc_value_object();
    if (!map) {
        ps->depth--;
        return fail_value(ps, "out of memory");
    }

    bool ok = true;
    if (seed) {
        ok = mapping_add_entry(ps, map, seed, seed_len, indent);
    }

    while (ok) {
        skip_blank(ps);
        if (ps->cursor >= ps->count) {
            break;
        }
        const char *content;
        size_t clen;
        size_t line_indent;
        content_view(ps->lines[ps->cursor], &content, &clen, &line_indent);
        if (line_indent != indent || is_seq_indicator(content, clen)
            || find_colon(content, clen) < 0) {
            break; // dedent, a sequence, or a non-mapping line ends the block
        }
        ps->cursor++;
        ok = mapping_add_entry(ps, map, content, clen, indent);
    }

    ps->depth--;
    if (!ok) {
        sc_value_free(map);
        return NULL;
    }
    return map;
}

/** Parses a block sequence at `indent`. */
static ScValue *parse_sequence(YamlParser *ps, size_t indent) {
    ps->depth++;
    ScValue *array = sc_value_array();
    if (!array) {
        ps->depth--;
        return fail_value(ps, "out of memory");
    }

    bool ok = true;
    while (ok) {
        skip_blank(ps);
        if (ps->cursor >= ps->count) {
            break;
        }
        const char *content;
        size_t clen;
        size_t line_indent;
        content_view(ps->lines[ps->cursor], &content, &clen, &line_indent);
        if (line_indent != indent || !is_seq_indicator(content, clen)) {
            break;
        }

        size_t offset = 1; // past the dash
        while (offset < clen && (content[offset] == ' '
                                 || content[offset] == '\t')) {
            offset++;
        }
        const char *rest = content + offset;
        size_t rest_len = clen - offset;
        size_t item_col = indent + offset;
        ps->cursor++;

        ScValue *item = NULL;
        if (rest_len == 0) {
            item = parse_child_block(ps, indent);
        } else if (find_colon(rest, rest_len) >= 0) {
            item = parse_mapping(ps, item_col, rest, rest_len);
        } else if (rest[0] == '|' || rest[0] == '>') {
            item = parse_block_scalar(ps, indent, rest[0]);
        } else {
            item = parse_inline_value(ps, rest, rest_len);
        }
        if (!item || !sc_value_push(array, item)) {
            sc_value_free(item);
            if (!ps->has_error) {
                fail(ps, "out of memory");
            }
            ok = false;
        }
    }

    ps->depth--;
    if (!ok) {
        sc_value_free(array);
        return NULL;
    }
    return array;
}

/** Parses the more-indented block that follows a `key:`/`-` with no inline
 *  value; returns a null value when nothing is nested. */
static ScValue *parse_child_block(YamlParser *ps, size_t parent_indent) {
    skip_blank(ps);
    if (ps->cursor >= ps->count) {
        return sc_value_null();
    }
    const char *content;
    size_t clen;
    size_t indent;
    content_view(ps->lines[ps->cursor], &content, &clen, &indent);
    if (indent <= parent_indent) {
        return sc_value_null();
    }
    return parse_node(ps, indent);
}

/** Adds one `key: value` entry (from `content`) to `map`. */
static bool mapping_add_entry(
    YamlParser *ps, ScValue *map, const char *content, size_t len,
    size_t indent
) {
    long colon = find_colon(content, len);
    if (colon < 0) {
        return fail(ps, "expected a mapping key");
    }
    size_t key_len = (size_t)colon;
    while (key_len > 0 && (content[key_len - 1] == ' '
                           || content[key_len - 1] == '\t')) {
        key_len--;
    }

    // Decode the key (plain or quoted) into a NUL-terminated string.
    char *key = NULL;
    if (key_len >= 2 && (content[0] == '"' || content[0] == '\'')
        && content[key_len - 1] == content[0]) {
        ScValue *decoded = parse_quoted(ps, content, key_len, content[0]);
        if (!decoded) {
            return false;
        }
        const char *text = sc_value_as_string(decoded);
        key = dup_n(text ? text : "", text ? strlen(text) : 0);
        sc_value_free(decoded);
    } else {
        key = dup_n(content, key_len);
    }
    if (!key) {
        return fail(ps, "out of memory");
    }

    const char *rest = content + colon + 1;
    size_t rest_len = len - (size_t)colon - 1;
    while (rest_len > 0 && (*rest == ' ' || *rest == '\t')) {
        rest++;
        rest_len--;
    }

    ScValue *value = NULL;
    if (rest_len == 0) {
        value = parse_child_block(ps, indent);
    } else if (rest[0] == '|' || rest[0] == '>') {
        value = parse_block_scalar(ps, indent, rest[0]);
    } else {
        value = parse_inline_value(ps, rest, rest_len);
    }
    if (!value) {
        free(key);
        return false;
    }

    bool stored = sc_value_set(map, key, value);
    free(key);
    if (!stored) {
        sc_value_free(value);
        return fail(ps, "out of memory");
    }
    return true;
}


/* ── Inline values (scalars and flow collections) ──────────────────────── */

/** Parses an inline value: a flow collection, a quoted or a plain scalar. */
static ScValue *parse_inline_value(
    YamlParser *ps, const char *s, size_t len
) {
    while (len > 0 && (*s == ' ' || *s == '\t')) {
        s++;
        len--;
    }
    if (len == 0) {
        return sc_value_null();
    }
    if (s[0] == '[' || s[0] == '{') {
        const char *p = s;
        return parse_flow(ps, &p, s + len);
    }
    if (s[0] == '"' || s[0] == '\'') {
        return parse_quoted(ps, s, len, s[0]);
    }
    return infer_scalar(s, len);
}

/** Parses a flow collection / scalar between `*p` and `end`. */
static ScValue *parse_flow(YamlParser *ps, const char **p, const char *end) {
    if (ps->depth + 1 > YAML_MAX_DEPTH) {
        return fail_value(ps, "maximum nesting depth exceeded");
    }
    ps->depth++;

    ScValue *result = NULL;
    const char *cursor = *p;
    while (cursor < end && (*cursor == ' ' || *cursor == '\t')) {
        cursor++;
    }
    if (cursor >= end) {
        ps->depth--;
        *p = cursor;
        return sc_value_null();
    }

    if (*cursor == '[' || *cursor == '{') {
        bool is_map = (*cursor == '{');
        char close = is_map ? '}' : ']';
        cursor++;
        ScValue *container = is_map ? sc_value_object() : sc_value_array();
        if (!container) {
            ps->depth--;
            *p = cursor;
            return fail_value(ps, "out of memory");
        }
        bool ok = true;
        for (;;) {
            while (cursor < end && (*cursor == ' ' || *cursor == '\t'
                                    || *cursor == ',')) {
                cursor++;
            }
            if (cursor >= end) {
                fail(ps, "unterminated flow collection");
                ok = false;
                break;
            }
            if (*cursor == close) {
                cursor++;
                break;
            }
            if (is_map) {
                // Read "key: value" up to ',' or close.
                const char *key_start = cursor;
                while (cursor < end && *cursor != ':' && *cursor != ','
                       && *cursor != close) {
                    cursor++;
                }
                if (cursor >= end || *cursor != ':') {
                    fail(ps, "expected ':' in flow mapping");
                    ok = false;
                    break;
                }
                size_t key_len = (size_t)(cursor - key_start);
                while (key_len > 0 && (key_start[key_len - 1] == ' ')) {
                    key_len--;
                }
                cursor++; // past ':'
                ScValue *value = parse_flow(ps, &cursor, end);
                if (!value) {
                    ok = false;
                    break;
                }
                char *key = dup_n(key_start, key_len);
                if (!key || !sc_value_set(container, key, value)) {
                    free(key);
                    sc_value_free(value);
                    fail(ps, "out of memory");
                    ok = false;
                    break;
                }
                free(key);
            } else {
                const char *before = cursor;
                ScValue *item = parse_flow(ps, &cursor, end);
                if (!item) {
                    ok = false;
                    break;
                }
                // A stray closing bracket (e.g. `]` inside `{...}` or vice
                // versa) makes a plain scalar consume nothing; reject it rather
                // than loop forever pushing empty values.
                if (cursor == before) {
                    sc_value_free(item);
                    fail(ps, "unexpected character in flow sequence");
                    ok = false;
                    break;
                }
                if (!sc_value_push(container, item)) {
                    sc_value_free(item);
                    fail(ps, "out of memory");
                    ok = false;
                    break;
                }
            }
        }
        if (!ok) {
            sc_value_free(container);
            container = NULL;
        }
        result = container;
    } else if (*cursor == '"' || *cursor == '\'') {
        char quote = *cursor;
        const char *start = cursor;
        cursor++;
        while (cursor < end) {
            if (quote == '\'' && *cursor == '\'') {
                if (cursor + 1 < end && cursor[1] == '\'') {
                    cursor += 2;
                    continue;
                }
                cursor++;
                break;
            }
            if (quote == '"' && *cursor == '\\' && cursor + 1 < end) {
                cursor += 2;
                continue;
            }
            if (quote == '"' && *cursor == '"') {
                cursor++;
                break;
            }
            cursor++;
        }
        result = parse_quoted(ps, start, (size_t)(cursor - start), quote);
    } else {
        const char *start = cursor;
        while (cursor < end && *cursor != ',' && *cursor != ']'
               && *cursor != '}') {
            cursor++;
        }
        size_t scalar_len = (size_t)(cursor - start);
        while (scalar_len > 0 && (start[scalar_len - 1] == ' '
                                  || start[scalar_len - 1] == '\t')) {
            scalar_len--;
        }
        result = infer_scalar(start, scalar_len);
    }

    ps->depth--;
    *p = cursor;
    return result;
}

/** Decodes a quoted scalar (`s[0]` is the quote char). */
static ScValue *parse_quoted(
    YamlParser *ps, const char *s, size_t len, char quote
) {
    ScSerdeBuf buf = { 0 };
    size_t i = 1; // past opening quote
    bool closed = false;
    while (i < len) {
        char ch = s[i];
        if (quote == '\'') {
            if (ch == '\'') {
                if (i + 1 < len && s[i + 1] == '\'') {
                    sc_serde_buf_append_char(&buf, '\'');
                    i += 2;
                    continue;
                }
                closed = true;
                break;
            }
            sc_serde_buf_append_char(&buf, ch);
            i++;
            continue;
        }
        // Double quoted.
        if (ch == '"') {
            closed = true;
            break;
        }
        if (ch == '\\' && i + 1 < len) {
            char esc = s[i + 1];
            i += 2;
            switch (esc) {
                case 'n': sc_serde_buf_append_char(&buf, '\n'); break;
                case 't': sc_serde_buf_append_char(&buf, '\t'); break;
                case 'r': sc_serde_buf_append_char(&buf, '\r'); break;
                case '"': sc_serde_buf_append_char(&buf, '"'); break;
                case '\\': sc_serde_buf_append_char(&buf, '\\'); break;
                case '0': sc_serde_buf_append_char(&buf, '\0'); break;
                default: sc_serde_buf_append_char(&buf, esc); break;
            }
            continue;
        }
        sc_serde_buf_append_char(&buf, ch);
        i++;
    }

    if (!closed) {
        sc_serde_buf_free(&buf);
        return fail_value(ps, "unterminated quoted scalar");
    }
    char *text = sc_serde_buf_finish(&buf);
    if (!text) {
        return fail_value(ps, "out of memory");
    }
    ScValue *value = sc_value_string(text);
    free(text);
    return value ? value : fail_value(ps, "out of memory");
}

/** Infers a plain scalar's type (null / bool / int / float / string). */
static ScValue *infer_scalar(const char *s, size_t len) {
    if (len == 0) {
        return sc_value_null();
    }
    if (len == 1 && s[0] == '~') {
        return sc_value_null();
    }

    static const char *const nulls[] = { "null", "Null", "NULL" };
    for (size_t i = 0; i < sizeof nulls / sizeof nulls[0]; i++) {
        if (strlen(nulls[i]) == len && memcmp(s, nulls[i], len) == 0) {
            return sc_value_null();
        }
    }
    static const char *const trues[] = { "true", "True", "TRUE" };
    static const char *const falses[] = { "false", "False", "FALSE" };
    for (size_t i = 0; i < sizeof trues / sizeof trues[0]; i++) {
        if (strlen(trues[i]) == len && memcmp(s, trues[i], len) == 0) {
            return sc_value_bool(true);
        }
        if (strlen(falses[i]) == len && memcmp(s, falses[i], len) == 0) {
            return sc_value_bool(false);
        }
    }

    if (len < YAML_SCALAR_BUFFER) {
        char tmp[YAML_SCALAR_BUFFER];
        memcpy(tmp, s, len);
        tmp[len] = '\0';

        if (strcmp(tmp, ".inf") == 0 || strcmp(tmp, "+.inf") == 0) {
            return sc_value_float(INFINITY);
        }
        if (strcmp(tmp, "-.inf") == 0) {
            return sc_value_float(-INFINITY);
        }
        if (strcmp(tmp, ".nan") == 0) {
            return sc_value_float(NAN);
        }

        // Integer?
        char *endp = NULL;
        errno = 0;
        long long integer = strtoll(tmp, &endp, 10);
        if (endp == tmp + len && *endp == '\0' && errno != ERANGE) {
            return sc_value_int((int64_t)integer);
        }
        // Float? (only when the token actually looks numeric)
        if (strpbrk(tmp, ".eE") != NULL && (tmp[0] == '-' || tmp[0] == '+'
            || tmp[0] == '.' || (tmp[0] >= '0' && tmp[0] <= '9'))) {
            errno = 0;
            double number = strtod(tmp, &endp);
            if (endp == tmp + len && *endp == '\0') {
                return sc_value_float(number);
            }
        }
    }

    return make_string(s, len);
}

/** Collects a `|`/`>` block scalar from the following indented lines. */
static ScValue *parse_block_scalar(
    YamlParser *ps, size_t parent_indent, char style
) {
    bool literal = (style == '|');
    ScSerdeBuf buf = { 0 };
    bool have_base = false;
    size_t base = 0;
    bool first = true;
    size_t pending_blanks = 0; // buffered so trailing blanks are chomped

    while (ps->cursor < ps->count) {
        YamlLine line = ps->lines[ps->cursor];
        size_t indent = 0;
        while (indent < line.len && line.text[indent] == ' ') {
            indent++;
        }
        bool blank = (indent == line.len);
        if (!blank && indent <= parent_indent) {
            break;
        }
        if (blank) {
            pending_blanks++;
            ps->cursor++;
            continue;
        }
        if (!have_base) {
            base = indent;
            have_base = true;
        }

        // Separate from the previous content line. A blank between lines folds
        // to a newline in both styles; otherwise literal uses a newline and
        // folded a single space.
        if (!first) {
            if (pending_blanks > 0) {
                if (literal) {
                    sc_serde_buf_append_char(&buf, '\n');
                }
                for (size_t b = 0; b < pending_blanks; b++) {
                    sc_serde_buf_append_char(&buf, '\n');
                }
            } else {
                sc_serde_buf_append_char(&buf, literal ? '\n' : ' ');
            }
        }
        pending_blanks = 0;

        size_t start = base <= line.len ? base : line.len;
        sc_serde_buf_append(&buf, line.text + start, line.len - start);
        first = false;
        ps->cursor++;
    }

    char *text = sc_serde_buf_finish(&buf);
    if (!text) {
        return fail_value(ps, "out of memory");
    }
    ScValue *value = sc_value_string(text);
    free(text);
    return value ? value : fail_value(ps, "out of memory");
}


/* ── Line helpers / errors ─────────────────────────────────────────────── */

/** Splits a line into its indentation and comment-stripped, trimmed content. */
static void content_view(
    YamlLine line, const char **out, size_t *out_len, size_t *indent
) {
    size_t i = 0;
    while (i < line.len && line.text[i] == ' ') {
        i++;
    }
    *indent = i;
    const char *content = line.text + i;
    size_t n = line.len - i;

    size_t cut = n;
    bool in_single = false;
    bool in_double = false;
    int flow = 0;
    for (size_t k = 0; k < n; k++) {
        char ch = content[k];
        if (in_double) {
            if (ch == '\\') {
                k++;
            } else if (ch == '"') {
                in_double = false;
            }
            continue;
        }
        if (in_single) {
            if (ch == '\'') {
                in_single = false;
            }
            continue;
        }
        if (ch == '"') {
            in_double = true;
        } else if (ch == '\'') {
            in_single = true;
        } else if (ch == '[' || ch == '{') {
            flow++;
        } else if ((ch == ']' || ch == '}') && flow > 0) {
            flow--;
        } else if (ch == '#' && flow == 0
                   && (k == 0 || content[k - 1] == ' '
                       || content[k - 1] == '\t')) {
            cut = k;
            break;
        }
    }
    while (cut > 0 && (content[cut - 1] == ' ' || content[cut - 1] == '\t')) {
        cut--;
    }
    *out = content;
    *out_len = cut;
}

/** Returns the index of the mapping `:` (followed by space or end), or -1. */
static long find_colon(const char *s, size_t len) {
    bool in_single = false;
    bool in_double = false;
    int flow = 0;
    for (size_t k = 0; k < len; k++) {
        char ch = s[k];
        if (in_double) {
            if (ch == '\\') {
                k++;
            } else if (ch == '"') {
                in_double = false;
            }
            continue;
        }
        if (in_single) {
            if (ch == '\'') {
                in_single = false;
            }
            continue;
        }
        if (ch == '"') {
            in_double = true;
        } else if (ch == '\'') {
            in_single = true;
        } else if (ch == '[' || ch == '{') {
            flow++;
        } else if ((ch == ']' || ch == '}') && flow > 0) {
            flow--;
        } else if (ch == ':' && flow == 0
                   && (k + 1 == len || s[k + 1] == ' ' || s[k + 1] == '\t')) {
            return (long)k;
        }
    }
    return -1;
}

/** True when a line is a block-sequence item (`- ...` or a bare `-`). */
static bool is_seq_indicator(const char *s, size_t len) {
    return len >= 1 && s[0] == '-'
        && (len == 1 || s[1] == ' ' || s[1] == '\t');
}

/** Advances over blank and comment-only lines. */
static void skip_blank(YamlParser *ps) {
    while (ps->cursor < ps->count) {
        const char *content;
        size_t clen;
        size_t indent;
        content_view(ps->lines[ps->cursor], &content, &clen, &indent);
        if (clen != 0) {
            break;
        }
        ps->cursor++;
    }
}

/** Records an error (first wins) and returns `false`. */
static bool fail(YamlParser *ps, const char *message) {
    if (ps->has_error) {
        return false;
    }
    ps->has_error = true;
    if (ps->err) {
        size_t line = ps->cursor < ps->count ? ps->cursor : ps->count;
        ps->err->line = (int)line + 1;
        snprintf(ps->err->message, sizeof ps->err->message, "%s", message);
        if (ps->cursor < ps->count) {
            YamlLine target = ps->lines[ps->cursor];
            size_t snippet = target.len;
            if (snippet >= sizeof ps->err->snippet) {
                snippet = sizeof ps->err->snippet - 1;
            }
            memcpy(ps->err->snippet, target.text, snippet);
            ps->err->snippet[snippet] = '\0';
        }
    }
    return false;
}

/** Records an error and returns `NULL`. */
static ScValue *fail_value(YamlParser *ps, const char *message) {
    fail(ps, message);
    return NULL;
}

/** Copies `len` bytes into a fresh NUL-terminated string (calloc: defined
 *  bytes for the static analyzer). */
static char *dup_n(const char *text, size_t len) {
    char *copy = calloc(len + 1, 1);
    if (!copy) {
        return NULL;
    }
    if (len > 0) {
        memcpy(copy, text, len);
    }
    return copy;
}

/** Builds a string value from a non-NUL-terminated span. */
static ScValue *make_string(const char *s, size_t len) {
    char *copy = dup_n(s, len);
    if (!copy) {
        return NULL;
    }
    ScValue *value = sc_value_string(copy);
    free(copy);
    return value;
}


/* ── Public writer ─────────────────────────────────────────────────────── */

char *sc_yaml_write(const ScValue *value, ScYamlWriteOpts opts) {
    if (opts.indent <= 0) {
        opts.indent = YAML_DEFAULT_INDENT;
    }
    ScSerdeBuf buf = { 0 };

    ScValueType type = sc_value_type(value);
    if (type == SC_VALUE_OBJECT && sc_value_len(value) == 0) {
        sc_serde_buf_append_str(&buf, "{}\n");
    } else if (type == SC_VALUE_ARRAY && sc_value_len(value) == 0) {
        sc_serde_buf_append_str(&buf, "[]\n");
    } else if (type == SC_VALUE_OBJECT || type == SC_VALUE_ARRAY) {
        write_block(&buf, value, 0, &opts);
    } else {
        write_scalar(&buf, value);
        sc_serde_buf_append_char(&buf, '\n');
    }
    return sc_serde_buf_finish(&buf);
}


/* ── Writer internals ──────────────────────────────────────────────────── */

/** Writes a mapping or sequence in block style at `indent`. */
static bool write_block(
    ScSerdeBuf *buf, const ScValue *value, int indent,
    const ScYamlWriteOpts *opts
) {
    size_t count = sc_value_len(value);
    if (sc_value_type(value) == SC_VALUE_OBJECT) {
        const char **order = NULL;
        if (opts->sort_keys) {
            order = sorted_keys(value, &count);
            if (!order && count > 0) {
                buf->failed = true;
                return false;
            }
        }
        for (size_t i = 0; i < count; i++) {
            const char *key = order ? order[i] : sc_value_key_at(value, i);
            write_indent(buf, indent);
            write_yaml_string(buf, key);
            sc_serde_buf_append_char(buf, ':');
            write_after_key(buf, sc_value_get(value, key), indent, opts);
        }
        free(order);
        return !buf->failed;
    }

    for (size_t i = 0; i < count; i++) {
        const ScValue *child = sc_value_at(value, i);
        write_indent(buf, indent);
        sc_serde_buf_append_char(buf, '-');
        write_after_key(buf, child, indent, opts);
    }
    return !buf->failed;
}

/** Writes the value following a `key:` or `-`, choosing inline vs block. */
static bool write_after_key(
    ScSerdeBuf *buf, const ScValue *child, int indent,
    const ScYamlWriteOpts *opts
) {
    ScValueType type = sc_value_type(child);
    if (type == SC_VALUE_OBJECT && sc_value_len(child) > 0) {
        sc_serde_buf_append_char(buf, '\n');
        return write_block(buf, child, indent + opts->indent, opts);
    }
    if (type == SC_VALUE_ARRAY && sc_value_len(child) > 0) {
        sc_serde_buf_append_char(buf, '\n');
        return write_block(buf, child, indent + opts->indent, opts);
    }
    sc_serde_buf_append_char(buf, ' ');
    if (type == SC_VALUE_OBJECT) {
        sc_serde_buf_append_str(buf, "{}");
    } else if (type == SC_VALUE_ARRAY) {
        sc_serde_buf_append_str(buf, "[]");
    } else {
        write_scalar(buf, child);
    }
    return sc_serde_buf_append_char(buf, '\n');
}

/** Writes a scalar value. */
static bool write_scalar(ScSerdeBuf *buf, const ScValue *value) {
    switch (sc_value_type(value)) {
        case SC_VALUE_NULL:
            return sc_serde_buf_append_str(buf, "null");
        case SC_VALUE_BOOL: {
            bool flag = false;
            sc_value_as_bool(value, &flag);
            return sc_serde_buf_append_str(buf, flag ? "true" : "false");
        }
        case SC_VALUE_INT: {
            int64_t integer = 0;
            sc_value_as_int(value, &integer);
            char tmp[YAML_SCALAR_BUFFER];
            snprintf(tmp, sizeof tmp, "%lld", (long long)integer);
            return sc_serde_buf_append_str(buf, tmp);
        }
        case SC_VALUE_FLOAT: {
            double number = 0.0;
            sc_value_as_float(value, &number);
            if (isnan(number)) {
                return sc_serde_buf_append_str(buf, ".nan");
            }
            if (isinf(number)) {
                return sc_serde_buf_append_str(buf, number < 0 ? "-.inf"
                                                               : ".inf");
            }
            char tmp[YAML_SCALAR_BUFFER];
            snprintf(tmp, sizeof tmp, "%.17g", number);
            bool looks_float = false;
            for (const char *scan = tmp; *scan; scan++) {
                if (*scan == '.' || *scan == 'e' || *scan == 'E') {
                    looks_float = true;
                    break;
                }
            }
            sc_serde_buf_append_str(buf, tmp);
            if (!looks_float) {
                sc_serde_buf_append_str(buf, ".0");
            }
            return !buf->failed;
        }
        case SC_VALUE_DATETIME:
            return sc_serde_buf_append_str(buf, sc_value_as_string(value));
        case SC_VALUE_STRING:
            return write_yaml_string(buf, sc_value_as_string(value));
        case SC_VALUE_ARRAY:
        case SC_VALUE_OBJECT:
            return false; // handled by write_after_key
    }
    return false;
}

/** Writes a string plain when safe, otherwise double-quoted. */
static bool write_yaml_string(ScSerdeBuf *buf, const char *text) {
    if (is_plain_safe(text)) {
        return sc_serde_buf_append_str(buf, text);
    }
    sc_serde_buf_append_char(buf, '"');
    for (const char *cursor = text ? text : ""; *cursor; cursor++) {
        unsigned char ch = (unsigned char)*cursor;
        switch (ch) {
            case '"':  sc_serde_buf_append_str(buf, "\\\""); continue;
            case '\\': sc_serde_buf_append_str(buf, "\\\\"); continue;
            case '\n': sc_serde_buf_append_str(buf, "\\n");  continue;
            case '\t': sc_serde_buf_append_str(buf, "\\t");  continue;
            case '\r': sc_serde_buf_append_str(buf, "\\r");  continue;
            default:
                break;
        }
        if (ch < 0x20) {
            char escape[8];
            snprintf(escape, sizeof escape, "\\x%02x", ch);
            sc_serde_buf_append_str(buf, escape);
        } else {
            sc_serde_buf_append_char(buf, (char)ch);
        }
    }
    return sc_serde_buf_append_char(buf, '"');
}

/** True when a string can be emitted as a plain (unquoted) scalar. */
static bool is_plain_safe(const char *text) {
    if (!text || text[0] == '\0') {
        return false; // empty must be quoted to differ from null
    }
    // Reserved words / number-looking strings must be quoted to round-trip.
    size_t len = strlen(text);
    ScValue *probe = infer_scalar(text, len);
    bool is_string = sc_value_type(probe) == SC_VALUE_STRING;
    sc_value_free(probe);
    if (!is_string) {
        return false;
    }

    char first = text[0];
    if (strchr("-?:,[]{}#&*!|>'\"%@`", first) || first == ' ') {
        return false;
    }
    if (text[len - 1] == ' ') {
        return false;
    }
    for (size_t i = 0; text[i]; i++) {
        unsigned char ch = (unsigned char)text[i];
        if (ch < 0x20) {
            return false;
        }
        if (ch == '#' && i > 0 && text[i - 1] == ' ') {
            return false;
        }
        if (ch == ':' && (text[i + 1] == ' ' || text[i + 1] == '\0')) {
            return false;
        }
    }
    return true;
}

/** Emits `indent` spaces. */
static bool write_indent(ScSerdeBuf *buf, int indent) {
    for (int i = 0; i < indent; i++) {
        if (!sc_serde_buf_append_char(buf, ' ')) {
            return false;
        }
    }
    return true;
}

/** qsort comparator over `const char *` keys. */
static int key_compare(const void *lhs, const void *rhs) {
    const char *const *left = lhs;
    const char *const *right = rhs;
    return strcmp(*left, *right);
}

/** Returns the mapping's keys sorted ascending (caller frees the array). */
static const char **sorted_keys(const ScValue *map, size_t *count_out) {
    size_t count = sc_value_len(map);
    *count_out = count;
    if (count == 0) {
        return NULL;
    }
    const char **keys = malloc(count * sizeof *keys);
    if (!keys) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        keys[i] = sc_value_key_at(map, i);
    }
    qsort(keys, count, sizeof *keys, key_compare);
    return keys;
}
