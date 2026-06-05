#include "serde_internal.h"
#include "serde/sparcli_toml.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file toml.c
 * @brief A near-complete TOML 1.0 reader and writer over `ScValue`.
 *
 * The reader is a recursive-descent parser over a NUL-terminated working copy
 * (depth-limited for nested arrays/inline tables). Date/time values are kept
 * verbatim as `SC_VALUE_DATETIME` strings. The writer emits scalar keys first,
 * then `[table]` sections and `[[array of tables]]`, so the output re-parses.
 */


/** Maximum array / inline-table nesting (recursion guard). */
#define TOML_MAX_DEPTH 200

/** Scratch buffer size for formatting one number token. */
#define TOML_NUMBER_BUFFER 64


/** Mutable parsing cursor + the document being built. */
typedef struct TomlParser {
    const char   *start;
    const char   *p;
    const char   *end;
    int           depth;
    bool          has_error;
    ScParseError *err;
    ScValue      *root;
} TomlParser;


// Reader forward declarations, indented to reflect the call hierarchy.
static bool parse_table_header(TomlParser *ps, ScValue **current);
static bool parse_key_assignment(TomlParser *ps, ScValue *table);
    static char *parse_key_segment(TomlParser *ps);
    static ScValue *descend_table(TomlParser *ps, ScValue *parent, char *key);
    static ScValue *parse_value(TomlParser *ps);
        static ScValue *parse_array(TomlParser *ps);
        static ScValue *parse_inline_table(TomlParser *ps);
        static ScValue *parse_string_value(TomlParser *ps);
        static ScValue *parse_bool(TomlParser *ps);
        static ScValue *parse_datetime(TomlParser *ps);
        static ScValue *parse_number(TomlParser *ps);
            static char *read_basic_string(TomlParser *ps);
            static char *read_literal_string(TomlParser *ps);
                static bool decode_basic_escape(TomlParser *ps, ScSerdeBuf *buf,
                                                bool multiline);
                    static int read_hex_digits(TomlParser *ps, int count);
static void skip_blank(TomlParser *ps);
static void skip_inline_ws(TomlParser *ps);
static bool finish_line(TomlParser *ps);
static bool fail(TomlParser *ps, const char *message);
    static ScValue *fail_value(TomlParser *ps, const char *message);
    static void set_error(TomlParser *ps, const char *message);
static bool is_bare_key_char(char ch);
static bool looks_like_datetime(const char *p, const char *end);

// Writer forward declarations.
static bool write_table(
    ScSerdeBuf *buf, const ScValue *table, const char *header,
    const ScTomlWriteOpts *opts
);
    static bool write_inline_value(ScSerdeBuf *buf, const ScValue *value);
        static bool write_basic_string(ScSerdeBuf *buf, const char *text);
        static bool write_number_double(ScSerdeBuf *buf, double value);
    static bool write_key(ScSerdeBuf *buf, const char *key);
    static bool is_array_of_tables(const ScValue *value);
    static char *join_header(const char *header, const char *key);
    static const char **sorted_indices(const ScValue *table, size_t *count_out);


/* ── Public reader ─────────────────────────────────────────────────────── */

ScValue *sc_toml_parse(const char *src, size_t len, ScParseError *err) {
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

    ScValue *root = sc_value_object();
    if (!root) {
        free(copy);
        if (err) {
            snprintf(err->message, sizeof err->message, "out of memory");
        }
        return NULL;
    }

    TomlParser ps = {
        .start = copy, .p = copy, .end = copy + len,
        .depth = 0, .has_error = false, .err = err, .root = root,
    };
    ScValue *current = root;

    bool ok = true;
    for (;;) {
        skip_blank(&ps);
        if (ps.p >= ps.end) {
            break;
        }
        if (*ps.p == '[') {
            ok = parse_table_header(&ps, &current);
        } else {
            ok = parse_key_assignment(&ps, current);
        }
        if (!ok || !finish_line(&ps)) {
            ok = false;
            break;
        }
    }

    free(copy);
    if (!ok) {
        sc_value_free(root);
        return NULL;
    }
    return root;
}


/* ── Statement-level parsing ───────────────────────────────────────────── */

/** Parses a `[table]` or `[[array of tables]]` header and moves `current`. */
static bool parse_table_header(TomlParser *ps, ScValue **current) {
    ps->p++; // consume '['
    bool array = (ps->p < ps->end && *ps->p == '[');
    if (array) {
        ps->p++;
    }

    ScValue *table = ps->root;
    char *segment = NULL;
    for (;;) {
        skip_inline_ws(ps);
        segment = parse_key_segment(ps);
        if (!segment) {
            return false;
        }
        skip_inline_ws(ps);

        bool last = !(ps->p < ps->end && *ps->p == '.');
        if (!last) {
            ps->p++; // consume '.'
            ScValue *next = descend_table(ps, table, segment);
            free(segment);
            if (!next) {
                return false;
            }
            table = next;
            continue;
        }

        // Final segment: open or append the target table.
        if (array) {
            ScValue *existing = sc_value_get(table, segment);
            ScValue *list = existing;
            if (!list) {
                list = sc_value_array();
                if (!list || !sc_value_set(table, segment, list)) {
                    sc_value_free(list);
                    free(segment);
                    return fail(ps, "out of memory");
                }
            } else if (sc_value_type(list) != SC_VALUE_ARRAY) {
                free(segment);
                return fail(ps, "key is not an array of tables");
            }
            free(segment);
            ScValue *entry = sc_value_object();
            if (!entry || !sc_value_push(list, entry)) {
                sc_value_free(entry);
                return fail(ps, "out of memory");
            }
            *current = entry;
        } else {
            ScValue *next = descend_table(ps, table, segment);
            free(segment);
            if (!next) {
                return false;
            }
            *current = next;
        }
        break;
    }

    // Consume the closing bracket(s).
    skip_inline_ws(ps);
    if (ps->p >= ps->end || *ps->p != ']') {
        return fail(ps, "expected ']' in table header");
    }
    ps->p++;
    if (array) {
        if (ps->p >= ps->end || *ps->p != ']') {
            return fail(ps, "expected ']]' in array-of-tables header");
        }
        ps->p++;
    }
    return true;
}

/** Parses one `key = value` (with optional dotted key path) into `table`. */
static bool parse_key_assignment(TomlParser *ps, ScValue *table) {
    ScValue *target = table;
    for (;;) {
        skip_inline_ws(ps);
        char *segment = parse_key_segment(ps);
        if (!segment) {
            return false;
        }
        skip_inline_ws(ps);

        if (ps->p < ps->end && *ps->p == '.') {
            ps->p++; // dotted key: descend and continue
            ScValue *next = descend_table(ps, target, segment);
            free(segment);
            if (!next) {
                return false;
            }
            target = next;
            continue;
        }

        if (ps->p >= ps->end || *ps->p != '=') {
            free(segment);
            return fail(ps, "expected '=' after key");
        }
        ps->p++; // consume '='
        skip_inline_ws(ps);

        ScValue *value = parse_value(ps);
        if (!value) {
            free(segment);
            return false;
        }
        bool stored = sc_value_set(target, segment, value);
        free(segment);
        if (!stored) {
            sc_value_free(value);
            return fail(ps, "out of memory");
        }
        return true;
    }
}

/** Reads a bare or quoted key segment into an owned string. */
static char *parse_key_segment(TomlParser *ps) {
    if (ps->p >= ps->end) {
        set_error(ps, "expected a key");
        return NULL;
    }
    if (*ps->p == '"' || *ps->p == '\'') {
        return (*ps->p == '"') ? read_basic_string(ps)
                               : read_literal_string(ps);
    }

    const char *begin = ps->p;
    while (ps->p < ps->end && is_bare_key_char(*ps->p)) {
        ps->p++;
    }
    if (ps->p == begin) {
        set_error(ps, "expected a key");
        return NULL;
    }
    size_t length = (size_t)(ps->p - begin);
    char *key = malloc(length + 1);
    if (!key) {
        set_error(ps, "out of memory");
        return NULL;
    }
    memcpy(key, begin, length);
    key[length] = '\0';
    return key;
}

/** Returns the child table `parent[key]`, creating it when absent. The key is
 *  owned by the caller (this borrows it). */
static ScValue *descend_table(TomlParser *ps, ScValue *parent, char *key) {
    ScValue *existing = sc_value_get(parent, key);
    if (existing) {
        if (sc_value_type(existing) == SC_VALUE_OBJECT) {
            return existing;
        }
        if (sc_value_type(existing) == SC_VALUE_ARRAY
            && sc_value_len(existing) > 0) {
            // Dotted path through an array of tables: use the last element.
            ScValue *last = sc_value_at(existing, sc_value_len(existing) - 1);
            if (sc_value_type(last) == SC_VALUE_OBJECT) {
                return last;
            }
        }
        set_error(ps, "key path conflicts with a non-table value");
        return NULL;
    }

    ScValue *table = sc_value_object();
    if (!table || !sc_value_set(parent, key, table)) {
        sc_value_free(table);
        set_error(ps, "out of memory");
        return NULL;
    }
    return table;
}


/* ── Value parsing ─────────────────────────────────────────────────────── */

/** Parses any TOML value at the cursor. */
static ScValue *parse_value(TomlParser *ps) {
    if (ps->p >= ps->end) {
        return fail_value(ps, "expected a value");
    }

    char ch = *ps->p;
    if (ch == '"' || ch == '\'') {
        return parse_string_value(ps);
    }
    if (ch == '[') {
        return parse_array(ps);
    }
    if (ch == '{') {
        return parse_inline_table(ps);
    }
    if (ch == 't' || ch == 'f') {
        return parse_bool(ps);
    }
    if (looks_like_datetime(ps->p, ps->end)) {
        return parse_datetime(ps);
    }
    return parse_number(ps);
}

/** Parses `[ value, ... ]` (newlines and comments allowed between items). */
static ScValue *parse_array(TomlParser *ps) {
    if (ps->depth + 1 > TOML_MAX_DEPTH) {
        return fail_value(ps, "maximum nesting depth exceeded");
    }
    ps->depth++;

    ScValue *result = NULL;
    ScValue *array = sc_value_array();
    if (!array) {
        fail(ps, "out of memory");
        goto done;
    }
    ps->p++; // consume '['

    for (;;) {
        skip_blank(ps);
        if (ps->p < ps->end && *ps->p == ']') {
            ps->p++;
            result = array;
            array = NULL;
            goto done;
        }
        ScValue *item = parse_value(ps);
        if (!item) {
            goto done;
        }
        if (!sc_value_push(array, item)) {
            sc_value_free(item);
            fail(ps, "out of memory");
            goto done;
        }
        skip_blank(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
            continue;
        }
        if (ps->p < ps->end && *ps->p == ']') {
            ps->p++;
            result = array;
            array = NULL;
            goto done;
        }
        fail(ps, "expected ',' or ']' in array");
        goto done;
    }

done:
    sc_value_free(array);
    ps->depth--;
    return result;
}

/** Parses `{ key = value, ... }` (single line, no newlines). */
static ScValue *parse_inline_table(TomlParser *ps) {
    if (ps->depth + 1 > TOML_MAX_DEPTH) {
        return fail_value(ps, "maximum nesting depth exceeded");
    }
    ps->depth++;

    ScValue *result = NULL;
    ScValue *table = sc_value_object();
    if (!table) {
        fail(ps, "out of memory");
        goto done;
    }
    ps->p++; // consume '{'

    skip_inline_ws(ps);
    if (ps->p < ps->end && *ps->p == '}') {
        ps->p++;
        result = table;
        table = NULL;
        goto done;
    }

    for (;;) {
        if (!parse_key_assignment(ps, table)) {
            goto done;
        }
        skip_inline_ws(ps);
        if (ps->p < ps->end && *ps->p == ',') {
            ps->p++;
            continue;
        }
        if (ps->p < ps->end && *ps->p == '}') {
            ps->p++;
            result = table;
            table = NULL;
            goto done;
        }
        fail(ps, "expected ',' or '}' in inline table");
        goto done;
    }

done:
    sc_value_free(table);
    ps->depth--;
    return result;
}

/** Parses a string value (basic or literal, single or multi-line). */
static ScValue *parse_string_value(TomlParser *ps) {
    char *text = (*ps->p == '"') ? read_basic_string(ps)
                                 : read_literal_string(ps);
    if (!text) {
        return NULL;
    }
    ScValue *value = sc_value_string(text);
    free(text);
    if (!value) {
        return fail_value(ps, "out of memory");
    }
    return value;
}

/** Parses a `true`/`false` literal. */
static ScValue *parse_bool(TomlParser *ps) {
    size_t remaining = (size_t)(ps->end - ps->p);
    if (remaining >= 4 && memcmp(ps->p, "true", 4) == 0) {
        ps->p += 4;
        ScValue *value = sc_value_bool(true);
        return value ? value : fail_value(ps, "out of memory");
    }
    if (remaining >= 5 && memcmp(ps->p, "false", 5) == 0) {
        ps->p += 5;
        ScValue *value = sc_value_bool(false);
        return value ? value : fail_value(ps, "out of memory");
    }
    return fail_value(ps, "invalid literal");
}

/** Captures a date/time token verbatim as a `SC_VALUE_DATETIME` string. */
static ScValue *parse_datetime(TomlParser *ps) {
    const char *begin = ps->p;
    while (ps->p < ps->end) {
        char ch = *ps->p;
        bool ok = (ch >= '0' && ch <= '9') || ch == '-' || ch == ':'
            || ch == '+' || ch == '.' || ch == 'T' || ch == 't'
            || ch == 'Z' || ch == 'z';
        if (!ok) {
            break;
        }
        ps->p++;
    }
    // Allow a single space separating a date from a time (e.g. `D HH:MM:SS`).
    if (ps->p < ps->end && *ps->p == ' ' && ps->p + 1 < ps->end
        && ps->p[1] >= '0' && ps->p[1] <= '9') {
        ps->p++;
        while (ps->p < ps->end) {
            char ch = *ps->p;
            bool ok = (ch >= '0' && ch <= '9') || ch == ':' || ch == '.'
                || ch == '+' || ch == '-' || ch == 'Z' || ch == 'z';
            if (!ok) {
                break;
            }
            ps->p++;
        }
    }

    size_t length = (size_t)(ps->p - begin);
    char *token = malloc(length + 1);
    if (!token) {
        return fail_value(ps, "out of memory");
    }
    memcpy(token, begin, length);
    token[length] = '\0';
    ScValue *value = sc_value_string_typed(SC_VALUE_DATETIME, token);
    free(token);
    return value ? value : fail_value(ps, "out of memory");
}

/** Parses an integer or float token. */
static ScValue *parse_number(TomlParser *ps) {
    const char *begin = ps->p;
    while (ps->p < ps->end) {
        char ch = *ps->p;
        bool ok = (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'z')
            || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '.'
            || ch == '+' || ch == '-';
        if (!ok) {
            break;
        }
        ps->p++;
    }
    size_t raw_len = (size_t)(ps->p - begin);
    if (raw_len == 0) {
        return fail_value(ps, "expected a value");
    }

    // Copy without underscores (TOML allows them as digit separators).
    char clean[TOML_NUMBER_BUFFER];
    if (raw_len >= sizeof clean) {
        return fail_value(ps, "number too long");
    }
    size_t n = 0;
    for (size_t i = 0; i < raw_len; i++) {
        if (begin[i] != '_') {
            clean[n++] = begin[i];
        }
    }
    clean[n] = '\0';

    // inf / nan (with optional sign).
    const char *digits = clean;
    if (*digits == '+' || *digits == '-') {
        digits++;
    }
    if (strcmp(digits, "inf") == 0 || strcmp(digits, "nan") == 0) {
        double base = (digits[0] == 'i') ? INFINITY : NAN;
        double number = (clean[0] == '-') ? -base : base;
        ScValue *value = sc_value_float(number);
        return value ? value : fail_value(ps, "out of memory");
    }

    // Prefixed integers: hex / octal / binary (no sign per TOML).
    if (clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'o'
                            || clean[1] == 'b')) {
        int base = (clean[1] == 'x') ? 16 : (clean[1] == 'o') ? 8 : 2;
        errno = 0;
        char *endp = NULL;
        long long integer = strtoll(clean + 2, &endp, base);
        if (errno == ERANGE || endp == clean + 2 || *endp != '\0') {
            return fail_value(ps, "invalid integer");
        }
        ScValue *value = sc_value_int((int64_t)integer);
        return value ? value : fail_value(ps, "out of memory");
    }

    bool is_float = strpbrk(clean, ".eE") != NULL;
    if (!is_float) {
        errno = 0;
        char *endp = NULL;
        long long integer = strtoll(clean, &endp, 10);
        if (endp != clean && *endp == '\0' && errno != ERANGE) {
            ScValue *value = sc_value_int((int64_t)integer);
            return value ? value : fail_value(ps, "out of memory");
        }
    }

    errno = 0;
    char *endp = NULL;
    double number = strtod(clean, &endp);
    if (endp == clean || *endp != '\0') {
        return fail_value(ps, "invalid number");
    }
    ScValue *value = sc_value_float(number);
    return value ? value : fail_value(ps, "out of memory");
}


/* ── String decoding ───────────────────────────────────────────────────── */

/** Decodes a basic (`"`) string, single- or multi-line, into an owned string. */
static char *read_basic_string(TomlParser *ps) {
    bool multiline = (ps->p + 2 < ps->end && ps->p[1] == '"' && ps->p[2] == '"');
    ps->p += multiline ? 3 : 1;

    ScSerdeBuf buf = { 0 };
    if (multiline && ps->p < ps->end && *ps->p == '\n') {
        ps->p++; // trim one immediate leading newline
    } else if (multiline && ps->p + 1 < ps->end && ps->p[0] == '\r'
               && ps->p[1] == '\n') {
        ps->p += 2;
    }

    while (ps->p < ps->end) {
        char ch = *ps->p;
        if (multiline) {
            if (ps->p + 2 < ps->end + 1 && ch == '"' && ps->p[1] == '"'
                && ps->p[2] == '"') {
                ps->p += 3;
                char *text = sc_serde_buf_finish(&buf);
                if (!text) {
                    set_error(ps, "out of memory");
                }
                return text;
            }
        } else if (ch == '"') {
            ps->p++;
            char *text = sc_serde_buf_finish(&buf);
            if (!text) {
                set_error(ps, "out of memory");
            }
            return text;
        }

        if (ch == '\\') {
            if (!decode_basic_escape(ps, &buf, multiline)) {
                sc_serde_buf_free(&buf);
                return NULL;
            }
            continue;
        }
        if (!multiline && (ch == '\n' || ch == '\r')) {
            sc_serde_buf_free(&buf);
            set_error(ps, "newline in single-line string");
            return NULL;
        }
        sc_serde_buf_append_char(&buf, ch);
        ps->p++;
    }

    sc_serde_buf_free(&buf);
    set_error(ps, "unterminated string");
    return NULL;
}

/** Decodes a literal (`'`) string, single- or multi-line. */
static char *read_literal_string(TomlParser *ps) {
    bool multiline = (ps->p + 2 < ps->end && ps->p[1] == '\''
                      && ps->p[2] == '\'');
    ps->p += multiline ? 3 : 1;

    ScSerdeBuf buf = { 0 };
    if (multiline && ps->p < ps->end && *ps->p == '\n') {
        ps->p++;
    } else if (multiline && ps->p + 1 < ps->end && ps->p[0] == '\r'
               && ps->p[1] == '\n') {
        ps->p += 2;
    }

    while (ps->p < ps->end) {
        char ch = *ps->p;
        if (multiline) {
            if (ps->p + 2 < ps->end + 1 && ch == '\'' && ps->p[1] == '\''
                && ps->p[2] == '\'') {
                ps->p += 3;
                char *text = sc_serde_buf_finish(&buf);
                if (!text) {
                    set_error(ps, "out of memory");
                }
                return text;
            }
        } else if (ch == '\'') {
            ps->p++;
            char *text = sc_serde_buf_finish(&buf);
            if (!text) {
                set_error(ps, "out of memory");
            }
            return text;
        }
        if (!multiline && (ch == '\n' || ch == '\r')) {
            sc_serde_buf_free(&buf);
            set_error(ps, "newline in single-line string");
            return NULL;
        }
        sc_serde_buf_append_char(&buf, ch);
        ps->p++;
    }

    sc_serde_buf_free(&buf);
    set_error(ps, "unterminated string");
    return NULL;
}

/** Decodes one escape in a basic string (the backslash is at the cursor). */
static bool decode_basic_escape(
    TomlParser *ps, ScSerdeBuf *buf, bool multiline
) {
    ps->p++; // consume backslash
    if (ps->p >= ps->end) {
        set_error(ps, "unterminated escape");
        return false;
    }

    char esc = *ps->p;
    switch (esc) {
        case '"':  ps->p++; return sc_serde_buf_append_char(buf, '"');
        case '\\': ps->p++; return sc_serde_buf_append_char(buf, '\\');
        case 'b':  ps->p++; return sc_serde_buf_append_char(buf, '\b');
        case 'f':  ps->p++; return sc_serde_buf_append_char(buf, '\f');
        case 'n':  ps->p++; return sc_serde_buf_append_char(buf, '\n');
        case 'r':  ps->p++; return sc_serde_buf_append_char(buf, '\r');
        case 't':  ps->p++; return sc_serde_buf_append_char(buf, '\t');
        case 'u':  ps->p++; {
            int cp = read_hex_digits(ps, 4);
            if (cp < 0) {
                set_error(ps, "invalid \\u escape");
                return false;
            }
            return sc_serde_append_utf8(buf, (uint32_t)cp);
        }
        case 'U':  ps->p++; {
            int cp = read_hex_digits(ps, 8);
            if (cp < 0) {
                set_error(ps, "invalid \\U escape");
                return false;
            }
            return sc_serde_append_utf8(buf, (uint32_t)cp);
        }
        default:
            break;
    }

    // Multi-line line-ending backslash: trim trailing whitespace + newlines.
    if (multiline && (esc == ' ' || esc == '\t' || esc == '\n'
                      || esc == '\r')) {
        while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t'
                                   || *ps->p == '\n' || *ps->p == '\r')) {
            ps->p++;
        }
        return true;
    }

    set_error(ps, "invalid escape sequence");
    return false;
}

/** Reads exactly `count` hex digits as a value, or `-1` on failure. */
static int read_hex_digits(TomlParser *ps, int count) {
    if ((size_t)(ps->end - ps->p) < (size_t)count) {
        return -1;
    }
    int value = 0;
    for (int i = 0; i < count; i++) {
        char ch = ps->p[i];
        int digit;
        if (ch >= '0' && ch <= '9') {
            digit = ch - '0';
        } else if (ch >= 'a' && ch <= 'f') {
            digit = ch - 'a' + 10;
        } else if (ch >= 'A' && ch <= 'F') {
            digit = ch - 'A' + 10;
        } else {
            return -1;
        }
        value = value * 16 + digit;
    }
    ps->p += count;
    return value;
}


/* ── Whitespace / errors ───────────────────────────────────────────────── */

/** Skips inline whitespace, newlines and whole comment lines. */
static void skip_blank(TomlParser *ps) {
    while (ps->p < ps->end) {
        char ch = *ps->p;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
            ps->p++;
        } else if (ch == '#') {
            while (ps->p < ps->end && *ps->p != '\n') {
                ps->p++;
            }
        } else {
            break;
        }
    }
}

/** Skips spaces and tabs only (not newlines). */
static void skip_inline_ws(TomlParser *ps) {
    while (ps->p < ps->end && (*ps->p == ' ' || *ps->p == '\t')) {
        ps->p++;
    }
}

/** Consumes trailing whitespace + optional comment, then requires EOL/EOF. */
static bool finish_line(TomlParser *ps) {
    skip_inline_ws(ps);
    if (ps->p < ps->end && *ps->p == '#') {
        while (ps->p < ps->end && *ps->p != '\n') {
            ps->p++;
        }
    }
    if (ps->p >= ps->end) {
        return true;
    }
    if (*ps->p == '\n' || *ps->p == '\r') {
        return true; // the next skip_blank consumes it
    }
    return fail(ps, "expected end of line");
}

/** Records an error (first wins) and returns `false`. */
static bool fail(TomlParser *ps, const char *message) {
    set_error(ps, message);
    return false;
}

/** Records an error and returns `NULL` (for value-returning callers). */
static ScValue *fail_value(TomlParser *ps, const char *message) {
    set_error(ps, message);
    return NULL;
}

/** Fills the error sink with the current location and a line snippet. */
static void set_error(TomlParser *ps, const char *message) {
    if (ps->has_error) {
        return;
    }
    ps->has_error = true;
    if (!ps->err) {
        return;
    }

    int line = 1;
    const char *line_start = ps->start;
    for (const char *scan = ps->start; scan < ps->p; scan++) {
        if (*scan == '\n') {
            line++;
            line_start = scan + 1;
        }
    }
    ps->err->line = line;
    ps->err->column = (int)(ps->p - line_start) + 1;
    snprintf(ps->err->message, sizeof ps->err->message, "%s", message);

    const char *line_end = line_start;
    while (line_end < ps->end && *line_end != '\n') {
        line_end++;
    }
    size_t snippet_len = (size_t)(line_end - line_start);
    if (snippet_len >= sizeof ps->err->snippet) {
        snippet_len = sizeof ps->err->snippet - 1;
    }
    memcpy(ps->err->snippet, line_start, snippet_len);
    ps->err->snippet[snippet_len] = '\0';
}

/** True for the characters allowed in a bare key. */
static bool is_bare_key_char(char ch) {
    return (ch >= 'A' && ch <= 'Z') || (ch >= 'a' && ch <= 'z')
        || (ch >= '0' && ch <= '9') || ch == '_' || ch == '-';
}

/** Heuristic: does the cursor start a date or time literal? */
static bool looks_like_datetime(const char *p, const char *end) {
    size_t remaining = (size_t)(end - p);
    // Local time: HH:MM...
    if (remaining >= 3 && p[0] >= '0' && p[0] <= '9' && p[1] >= '0'
        && p[1] <= '9' && p[2] == ':') {
        return true;
    }
    // Date / datetime: YYYY-MM...
    if (remaining >= 5 && p[0] >= '0' && p[0] <= '9' && p[1] >= '0'
        && p[1] <= '9' && p[2] >= '0' && p[2] <= '9' && p[3] >= '0'
        && p[3] <= '9' && p[4] == '-') {
        return true;
    }
    return false;
}


/* ── Public writer ─────────────────────────────────────────────────────── */

char *sc_toml_write(const ScValue *value, ScTomlWriteOpts opts) {
    ScSerdeBuf buf = { 0 };
    if (value && sc_value_type(value) == SC_VALUE_OBJECT) {
        write_table(&buf, value, "", &opts);
    }
    return sc_serde_buf_finish(&buf);
}


/* ── File convenience ──────────────────────────────────────────────────── */

ScValue *sc_toml_parse_file(const char *path, ScParseError *err) {
    size_t len = 0;
    char *data = sc_serde_read_file(path, &len, err);
    if (!data) {
        return NULL;
    }
    ScValue *root = sc_toml_parse(data, len, err);
    free(data);
    return root;
}

bool sc_toml_write_file(
    const ScValue *value, const char *path, ScTomlWriteOpts opts
) {
    char *text = sc_toml_write(value, opts);
    if (!text) {
        return false;
    }
    bool ok = sc_serde_write_file(path, text);
    free(text);
    return ok;
}


/* ── Writer internals ──────────────────────────────────────────────────── */

/** Emits a table: scalar keys first, then nested tables / arrays of tables. */
static bool write_table(
    ScSerdeBuf *buf, const ScValue *table, const char *header,
    const ScTomlWriteOpts *opts
) {
    size_t count = sc_value_len(table);
    const char **order = NULL;
    if (opts->sort_keys) {
        order = sorted_indices(table, &count);
        if (!order && count > 0) {
            buf->failed = true;
            return false;
        }
    }

    // Pass 1: scalar / inline values.
    for (size_t i = 0; i < count; i++) {
        const char *key = order ? order[i] : sc_value_key_at(table, i);
        const ScValue *child = sc_value_get(table, key);
        ScValueType type = sc_value_type(child);
        if (type == SC_VALUE_OBJECT || is_array_of_tables(child)
            || type == SC_VALUE_NULL) {
            continue; // handled in pass 2 (or skipped: TOML has no null)
        }
        write_key(buf, key);
        sc_serde_buf_append_str(buf, " = ");
        write_inline_value(buf, child);
        sc_serde_buf_append_char(buf, '\n');
    }

    // Pass 2: sub-tables and arrays of tables.
    for (size_t i = 0; i < count; i++) {
        const char *key = order ? order[i] : sc_value_key_at(table, i);
        const ScValue *child = sc_value_get(table, key);
        ScValueType type = sc_value_type(child);

        if (type == SC_VALUE_OBJECT) {
            char *path = join_header(header, key);
            if (!path) {
                buf->failed = true;
                break;
            }
            sc_serde_buf_append_str(buf, "\n[");
            sc_serde_buf_append_str(buf, path);
            sc_serde_buf_append_str(buf, "]\n");
            write_table(buf, child, path, opts);
            free(path);
        } else if (is_array_of_tables(child)) {
            char *path = join_header(header, key);
            if (!path) {
                buf->failed = true;
                break;
            }
            for (size_t e = 0; e < sc_value_len(child); e++) {
                sc_serde_buf_append_str(buf, "\n[[");
                sc_serde_buf_append_str(buf, path);
                sc_serde_buf_append_str(buf, "]]\n");
                write_table(buf, sc_value_at(child, e), path, opts);
            }
            free(path);
        }
    }

    free(order);
    return !buf->failed;
}

/** Writes a value in inline form (scalars, inline arrays, inline tables). */
static bool write_inline_value(ScSerdeBuf *buf, const ScValue *value) {
    switch (sc_value_type(value)) {
        case SC_VALUE_NULL:
            return write_basic_string(buf, "");
        case SC_VALUE_BOOL: {
            bool flag = false;
            sc_value_as_bool(value, &flag);
            return sc_serde_buf_append_str(buf, flag ? "true" : "false");
        }
        case SC_VALUE_INT: {
            int64_t integer = 0;
            sc_value_as_int(value, &integer);
            char tmp[TOML_NUMBER_BUFFER];
            snprintf(tmp, sizeof tmp, "%lld", (long long)integer);
            return sc_serde_buf_append_str(buf, tmp);
        }
        case SC_VALUE_FLOAT: {
            double number = 0.0;
            sc_value_as_float(value, &number);
            return write_number_double(buf, number);
        }
        case SC_VALUE_STRING:
            return write_basic_string(buf, sc_value_as_string(value));
        case SC_VALUE_DATETIME:
            return sc_serde_buf_append_str(buf, sc_value_as_string(value));
        case SC_VALUE_ARRAY: {
            sc_serde_buf_append_char(buf, '[');
            for (size_t i = 0; i < sc_value_len(value); i++) {
                if (i > 0) {
                    sc_serde_buf_append_str(buf, ", ");
                }
                write_inline_value(buf, sc_value_at(value, i));
            }
            return sc_serde_buf_append_char(buf, ']');
        }
        case SC_VALUE_OBJECT: {
            sc_serde_buf_append_str(buf, "{ ");
            size_t members = sc_value_len(value);
            for (size_t i = 0; i < members; i++) {
                if (i > 0) {
                    sc_serde_buf_append_str(buf, ", ");
                }
                const char *key = sc_value_key_at(value, i);
                write_key(buf, key);
                sc_serde_buf_append_str(buf, " = ");
                write_inline_value(buf, sc_value_get(value, key));
            }
            return sc_serde_buf_append_str(buf, " }");
        }
    }
    return false;
}

/** Writes a basic-quoted, escaped TOML string. */
static bool write_basic_string(ScSerdeBuf *buf, const char *text) {
    sc_serde_buf_append_char(buf, '"');
    for (const char *cursor = text ? text : ""; *cursor; cursor++) {
        unsigned char ch = (unsigned char)*cursor;
        switch (ch) {
            case '"':  sc_serde_buf_append_str(buf, "\\\""); continue;
            case '\\': sc_serde_buf_append_str(buf, "\\\\"); continue;
            case '\b': sc_serde_buf_append_str(buf, "\\b");  continue;
            case '\f': sc_serde_buf_append_str(buf, "\\f");  continue;
            case '\n': sc_serde_buf_append_str(buf, "\\n");  continue;
            case '\r': sc_serde_buf_append_str(buf, "\\r");  continue;
            case '\t': sc_serde_buf_append_str(buf, "\\t");  continue;
            default:
                break;
        }
        if (ch < 0x20) {
            char escape[8];
            snprintf(escape, sizeof escape, "\\u%04x", ch);
            sc_serde_buf_append_str(buf, escape);
        } else {
            sc_serde_buf_append_char(buf, (char)ch);
        }
    }
    return sc_serde_buf_append_char(buf, '"');
}

/** Writes a float so it round-trips (TOML `inf`/`nan` for non-finite). */
static bool write_number_double(ScSerdeBuf *buf, double value) {
    if (isnan(value)) {
        return sc_serde_buf_append_str(buf, "nan");
    }
    if (isinf(value)) {
        return sc_serde_buf_append_str(buf, value < 0 ? "-inf" : "inf");
    }

    char tmp[TOML_NUMBER_BUFFER];
    snprintf(tmp, sizeof tmp, "%.17g", value);
    bool looks_float = false;
    for (const char *scan = tmp; *scan; scan++) {
        if (*scan == '.' || *scan == 'e' || *scan == 'E') {
            looks_float = true;
            break;
        }
    }
    if (!sc_serde_buf_append_str(buf, tmp)) {
        return false;
    }
    if (!looks_float) {
        return sc_serde_buf_append_str(buf, ".0");
    }
    return true;
}

/** Writes a key bare when possible, otherwise basic-quoted. */
static bool write_key(ScSerdeBuf *buf, const char *key) {
    bool bare = key[0] != '\0';
    for (const char *scan = key; *scan; scan++) {
        if (!is_bare_key_char(*scan)) {
            bare = false;
            break;
        }
    }
    if (bare) {
        return sc_serde_buf_append_str(buf, key);
    }
    return write_basic_string(buf, key);
}

/** True when `value` is a non-empty array whose elements are all objects. */
static bool is_array_of_tables(const ScValue *value) {
    if (sc_value_type(value) != SC_VALUE_ARRAY) {
        return false;
    }
    size_t length = sc_value_len(value);
    if (length == 0) {
        return false;
    }
    for (size_t i = 0; i < length; i++) {
        if (sc_value_type(sc_value_at(value, i)) != SC_VALUE_OBJECT) {
            return false;
        }
    }
    return true;
}

/** Builds a dotted header path `header.key` (quoting `key` if needed). */
static char *join_header(const char *header, const char *key) {
    ScSerdeBuf buf = { 0 };
    if (header[0] != '\0') {
        sc_serde_buf_append_str(&buf, header);
        sc_serde_buf_append_char(&buf, '.');
    }
    write_key(&buf, key);
    return sc_serde_buf_finish(&buf);
}

/** qsort comparator over `const char *` keys. */
static int key_compare(const void *lhs, const void *rhs) {
    const char *const *left = lhs;
    const char *const *right = rhs;
    return strcmp(*left, *right);
}

/** Returns the table's keys sorted ascending (caller frees the array). */
static const char **sorted_indices(const ScValue *table, size_t *count_out) {
    size_t count = sc_value_len(table);
    *count_out = count;
    if (count == 0) {
        return NULL;
    }
    const char **keys = malloc(count * sizeof *keys);
    if (!keys) {
        return NULL;
    }
    for (size_t i = 0; i < count; i++) {
        keys[i] = sc_value_key_at(table, i);
    }
    qsort(keys, count, sizeof *keys, key_compare);
    return keys;
}
