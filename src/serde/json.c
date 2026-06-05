#include "serde_internal.h"
#include "serde/sparcli_json.h"

#include <errno.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @file json.c
 * @brief Strict, depth-limited JSON reader (RFC 8259) and a round-trippable
 *        writer, both over the shared `ScValue` model.
 */


/** Maximum container nesting, guarding the recursive parser against a stack
 *  overflow on adversarial input. */
#define JSON_MAX_DEPTH 200

/** Scratch buffer size for formatting one number token. */
#define JSON_NUMBER_BUFFER 32


/** Mutable parsing cursor over a NUL-terminated working copy of the source. */
typedef struct JsonCtx {
    const char   *start;     /**< First byte of the working copy. */
    const char   *p;         /**< Current read position. */
    const char   *end;       /**< One past the last source byte. */
    int           depth;     /**< Current container nesting. */
    bool          has_error; /**< First error wins (outer frames don't clobber). */
    ScParseError *err;       /**< Optional error sink. */
} JsonCtx;


// Reader forward declarations, indented to reflect the call hierarchy.
static ScValue *parse_value(JsonCtx *ctx);
    static void skip_ws(JsonCtx *ctx);
    static ScValue *parse_object(JsonCtx *ctx);
    static ScValue *parse_array(JsonCtx *ctx);
    static ScValue *parse_string_value(JsonCtx *ctx);
        static char *parse_string_raw(JsonCtx *ctx);
            static bool decode_escape(JsonCtx *ctx, ScSerdeBuf *buf);
                static int read_hex4(JsonCtx *ctx);
                static bool append_utf8(ScSerdeBuf *buf, uint32_t codepoint);
    static ScValue *parse_number(JsonCtx *ctx);
    static ScValue *parse_keyword(JsonCtx *ctx);
        static bool match_keyword(JsonCtx *ctx, const char *keyword);
    static ScValue *fail(JsonCtx *ctx, const char *message);
        static void set_error(JsonCtx *ctx, const char *message);

// Writer forward declarations.
static bool write_value(
    ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
    int depth
);
    static bool write_object(
        ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
        int depth
    );
    static bool write_array(
        ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
        int depth
    );
    static bool write_string(ScSerdeBuf *buf, const char *text);
    static bool write_double(ScSerdeBuf *buf, double value);
    static bool write_indent(ScSerdeBuf *buf, int indent, int depth);


/* ── Public reader ─────────────────────────────────────────────────────── */

ScValue *sc_json_parse(const char *src, size_t len, ScParseError *err) {
    if (err) {
        *err = (ScParseError){ 0 };
    }
    if (!src) {
        len = 0;
    }

    // Work on a NUL-terminated copy so the number conversions and the NUL
    // sentinel stay in bounds regardless of how the caller framed `src`.
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

    JsonCtx ctx = {
        .start = copy,
        .p = copy,
        .end = copy + len,
        .depth = 0,
        .has_error = false,
        .err = err,
    };

    ScValue *root = parse_value(&ctx);
    if (root) {
        skip_ws(&ctx);
        if (ctx.p != ctx.end) {
            sc_value_free(root);
            root = NULL;
            set_error(&ctx, "trailing data after JSON value");
        }
    }

    free(copy);
    return root;
}


/* ── Reader internals ──────────────────────────────────────────────────── */

/** Dispatches on the next non-whitespace byte to the matching sub-parser. */
static ScValue *parse_value(JsonCtx *ctx) {
    skip_ws(ctx);
    if (ctx->p >= ctx->end) {
        return fail(ctx, "unexpected end of input");
    }

    char ch = *ctx->p;
    switch (ch) {
        case '{':
            return parse_object(ctx);
        case '[':
            return parse_array(ctx);
        case '"':
            return parse_string_value(ctx);
        case 't':
        case 'f':
        case 'n':
            return parse_keyword(ctx);
        default:
            if (ch == '-' || (ch >= '0' && ch <= '9')) {
                return parse_number(ctx);
            }
            return fail(ctx, "unexpected character");
    }
}

/** Advances past JSON insignificant whitespace. */
static void skip_ws(JsonCtx *ctx) {
    while (ctx->p < ctx->end) {
        char ch = *ctx->p;
        if (ch != ' ' && ch != '\t' && ch != '\n' && ch != '\r') {
            break;
        }
        ctx->p++;
    }
}

/** Parses an object: `{ "key": value, ... }`. */
static ScValue *parse_object(JsonCtx *ctx) {
    if (ctx->depth + 1 > JSON_MAX_DEPTH) {
        return fail(ctx, "maximum nesting depth exceeded");
    }
    ctx->depth++;

    ScValue *result = NULL;
    ScValue *object = sc_value_object();
    if (!object) {
        fail(ctx, "out of memory");
        goto done;
    }
    ctx->p++; // consume '{'

    skip_ws(ctx);
    if (ctx->p < ctx->end && *ctx->p == '}') {
        ctx->p++;
        result = object;
        object = NULL;
        goto done;
    }

    for (;;) {
        skip_ws(ctx);
        if (ctx->p >= ctx->end || *ctx->p != '"') {
            fail(ctx, "expected string key");
            goto done;
        }
        char *key = parse_string_raw(ctx);
        if (!key) {
            goto done;
        }

        skip_ws(ctx);
        if (ctx->p >= ctx->end || *ctx->p != ':') {
            free(key);
            fail(ctx, "expected ':' after object key");
            goto done;
        }
        ctx->p++; // consume ':'

        ScValue *child = parse_value(ctx);
        if (!child) {
            free(key);
            goto done;
        }
        bool stored = sc_value_set(object, key, child);
        free(key);
        if (!stored) {
            sc_value_free(child);
            fail(ctx, "out of memory");
            goto done;
        }

        skip_ws(ctx);
        if (ctx->p < ctx->end && *ctx->p == ',') {
            ctx->p++;
            continue;
        }
        if (ctx->p < ctx->end && *ctx->p == '}') {
            ctx->p++;
            result = object;
            object = NULL;
            goto done;
        }
        fail(ctx, "expected ',' or '}' in object");
        goto done;
    }

done:
    sc_value_free(object); // NULL once ownership moved to `result`
    ctx->depth--;
    return result;
}

/** Parses an array: `[ value, ... ]`. */
static ScValue *parse_array(JsonCtx *ctx) {
    if (ctx->depth + 1 > JSON_MAX_DEPTH) {
        return fail(ctx, "maximum nesting depth exceeded");
    }
    ctx->depth++;

    ScValue *result = NULL;
    ScValue *array = sc_value_array();
    if (!array) {
        fail(ctx, "out of memory");
        goto done;
    }
    ctx->p++; // consume '['

    skip_ws(ctx);
    if (ctx->p < ctx->end && *ctx->p == ']') {
        ctx->p++;
        result = array;
        array = NULL;
        goto done;
    }

    for (;;) {
        ScValue *child = parse_value(ctx);
        if (!child) {
            goto done;
        }
        if (!sc_value_push(array, child)) {
            sc_value_free(child);
            fail(ctx, "out of memory");
            goto done;
        }

        skip_ws(ctx);
        if (ctx->p < ctx->end && *ctx->p == ',') {
            ctx->p++;
            continue;
        }
        if (ctx->p < ctx->end && *ctx->p == ']') {
            ctx->p++;
            result = array;
            array = NULL;
            goto done;
        }
        fail(ctx, "expected ',' or ']' in array");
        goto done;
    }

done:
    sc_value_free(array);
    ctx->depth--;
    return result;
}

/** Parses a string value (the leading byte is a `"`). */
static ScValue *parse_string_value(JsonCtx *ctx) {
    char *text = parse_string_raw(ctx);
    if (!text) {
        return NULL;
    }
    ScValue *value = sc_value_string(text);
    free(text);
    if (!value) {
        return fail(ctx, "out of memory");
    }
    return value;
}

/**
 * Decodes a quoted JSON string into a freshly allocated, NUL-terminated byte
 * string (UTF-8). Returns `NULL` and sets the error on a malformed string.
 */
static char *parse_string_raw(JsonCtx *ctx) {
    ctx->p++; // consume opening quote

    ScSerdeBuf buf = { 0 };
    while (ctx->p < ctx->end) {
        char ch = *ctx->p;
        if (ch == '"') {
            ctx->p++;
            char *text = sc_serde_buf_finish(&buf);
            if (!text) {
                set_error(ctx, "out of memory");
            }
            return text;
        }
        if (ch == '\\') {
            ctx->p++;
            if (!decode_escape(ctx, &buf)) {
                sc_serde_buf_free(&buf);
                return NULL;
            }
            continue;
        }
        if ((unsigned char)ch < 0x20) {
            sc_serde_buf_free(&buf);
            set_error(ctx, "unescaped control character in string");
            return NULL;
        }
        sc_serde_buf_append_char(&buf, ch);
        ctx->p++;
    }

    sc_serde_buf_free(&buf);
    set_error(ctx, "unterminated string");
    return NULL;
}

/** Decodes one backslash escape (the backslash is already consumed). */
static bool decode_escape(JsonCtx *ctx, ScSerdeBuf *buf) {
    if (ctx->p >= ctx->end) {
        set_error(ctx, "unterminated escape sequence");
        return false;
    }

    char esc = *ctx->p;
    ctx->p++;
    switch (esc) {
        case '"':  return sc_serde_buf_append_char(buf, '"');
        case '\\': return sc_serde_buf_append_char(buf, '\\');
        case '/':  return sc_serde_buf_append_char(buf, '/');
        case 'b':  return sc_serde_buf_append_char(buf, '\b');
        case 'f':  return sc_serde_buf_append_char(buf, '\f');
        case 'n':  return sc_serde_buf_append_char(buf, '\n');
        case 'r':  return sc_serde_buf_append_char(buf, '\r');
        case 't':  return sc_serde_buf_append_char(buf, '\t');
        case 'u':  break;
        default:
            set_error(ctx, "invalid escape sequence");
            return false;
    }

    int first = read_hex4(ctx);
    if (first < 0) {
        set_error(ctx, "invalid \\u escape");
        return false;
    }

    uint32_t codepoint = (uint32_t)first;
    if (first >= 0xd800 && first <= 0xdbff) {
        // High surrogate: a low surrogate must immediately follow.
        if ((size_t)(ctx->end - ctx->p) < 2
            || ctx->p[0] != '\\' || ctx->p[1] != 'u') {
            set_error(ctx, "unpaired surrogate in \\u escape");
            return false;
        }
        ctx->p += 2;
        int second = read_hex4(ctx);
        if (second < 0xdc00 || second > 0xdfff) {
            set_error(ctx, "invalid low surrogate in \\u escape");
            return false;
        }
        codepoint = 0x10000u
            + (((uint32_t)first - 0xd800u) << 10)
            + ((uint32_t)second - 0xdc00u);
    } else if (first >= 0xdc00 && first <= 0xdfff) {
        set_error(ctx, "unexpected low surrogate in \\u escape");
        return false;
    }

    return append_utf8(buf, codepoint);
}

/** Reads exactly four hex digits as a value, or `-1` on failure. */
static int read_hex4(JsonCtx *ctx) {
    if ((size_t)(ctx->end - ctx->p) < 4) {
        return -1;
    }
    int value = 0;
    for (int i = 0; i < 4; i++) {
        char ch = ctx->p[i];
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
    ctx->p += 4;
    return value;
}

/** Encodes one Unicode code point as UTF-8 and appends it to `buf`. */
static bool append_utf8(ScSerdeBuf *buf, uint32_t codepoint) {
    char bytes[4];
    size_t count;
    if (codepoint <= 0x7f) {
        bytes[0] = (char)codepoint;
        count = 1;
    } else if (codepoint <= 0x7ff) {
        bytes[0] = (char)(0xc0 | (codepoint >> 6));
        bytes[1] = (char)(0x80 | (codepoint & 0x3f));
        count = 2;
    } else if (codepoint <= 0xffff) {
        bytes[0] = (char)(0xe0 | (codepoint >> 12));
        bytes[1] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[2] = (char)(0x80 | (codepoint & 0x3f));
        count = 3;
    } else {
        bytes[0] = (char)(0xf0 | (codepoint >> 18));
        bytes[1] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        bytes[2] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        bytes[3] = (char)(0x80 | (codepoint & 0x3f));
        count = 4;
    }
    return sc_serde_buf_append(buf, bytes, count);
}

/** Parses a number, choosing `SC_VALUE_INT` unless a fraction/exponent (or
 *  integer overflow) forces `SC_VALUE_FLOAT`. */
static ScValue *parse_number(JsonCtx *ctx) {
    const char *begin = ctx->p;
    bool is_float = false;

    if (ctx->p < ctx->end && *ctx->p == '-') {
        ctx->p++;
    }

    // Integer part: a lone 0 or a non-zero leading digit run.
    if (ctx->p < ctx->end && *ctx->p == '0') {
        ctx->p++;
    } else if (ctx->p < ctx->end && *ctx->p >= '1' && *ctx->p <= '9') {
        while (ctx->p < ctx->end && *ctx->p >= '0' && *ctx->p <= '9') {
            ctx->p++;
        }
    } else {
        return fail(ctx, "invalid number");
    }

    // Fraction.
    if (ctx->p < ctx->end && *ctx->p == '.') {
        is_float = true;
        ctx->p++;
        if (ctx->p >= ctx->end || *ctx->p < '0' || *ctx->p > '9') {
            return fail(ctx, "missing digits after decimal point");
        }
        while (ctx->p < ctx->end && *ctx->p >= '0' && *ctx->p <= '9') {
            ctx->p++;
        }
    }

    // Exponent.
    if (ctx->p < ctx->end && (*ctx->p == 'e' || *ctx->p == 'E')) {
        is_float = true;
        ctx->p++;
        if (ctx->p < ctx->end && (*ctx->p == '+' || *ctx->p == '-')) {
            ctx->p++;
        }
        if (ctx->p >= ctx->end || *ctx->p < '0' || *ctx->p > '9') {
            return fail(ctx, "missing digits in exponent");
        }
        while (ctx->p < ctx->end && *ctx->p >= '0' && *ctx->p <= '9') {
            ctx->p++;
        }
    }

    if (!is_float) {
        errno = 0;
        long long integer = strtoll(begin, NULL, 10);
        if (errno != ERANGE) {
            ScValue *value = sc_value_int((int64_t)integer);
            return value ? value : fail(ctx, "out of memory");
        }
        // Out of int64 range: keep it as a double instead of failing.
    }

    double number = strtod(begin, NULL);
    ScValue *value = sc_value_float(number);
    return value ? value : fail(ctx, "out of memory");
}

/** Parses one of the bare keywords `true`, `false`, `null`. */
static ScValue *parse_keyword(JsonCtx *ctx) {
    if (match_keyword(ctx, "true")) {
        ScValue *value = sc_value_bool(true);
        return value ? value : fail(ctx, "out of memory");
    }
    if (match_keyword(ctx, "false")) {
        ScValue *value = sc_value_bool(false);
        return value ? value : fail(ctx, "out of memory");
    }
    if (match_keyword(ctx, "null")) {
        ScValue *value = sc_value_null();
        return value ? value : fail(ctx, "out of memory");
    }
    return fail(ctx, "invalid literal");
}

/** Consumes `keyword` when it matches at the cursor. */
static bool match_keyword(JsonCtx *ctx, const char *keyword) {
    size_t length = strlen(keyword);
    if ((size_t)(ctx->end - ctx->p) < length) {
        return false;
    }
    if (memcmp(ctx->p, keyword, length) != 0) {
        return false;
    }
    ctx->p += length;
    return true;
}

/** Records an error (first one wins) and returns `NULL` for the caller. */
static ScValue *fail(JsonCtx *ctx, const char *message) {
    set_error(ctx, message);
    return NULL;
}

/** Fills the error sink with the current location, message and a line
 *  snippet. Only the first error in a parse is recorded. */
static void set_error(JsonCtx *ctx, const char *message) {
    if (ctx->has_error) {
        return;
    }
    ctx->has_error = true;
    if (!ctx->err) {
        return;
    }

    int line = 1;
    const char *line_start = ctx->start;
    for (const char *scan = ctx->start; scan < ctx->p; scan++) {
        if (*scan == '\n') {
            line++;
            line_start = scan + 1;
        }
    }

    ctx->err->line = line;
    ctx->err->column = (int)(ctx->p - line_start) + 1;
    snprintf(ctx->err->message, sizeof ctx->err->message, "%s", message);

    const char *line_end = line_start;
    while (line_end < ctx->end && *line_end != '\n') {
        line_end++;
    }
    size_t snippet_len = (size_t)(line_end - line_start);
    if (snippet_len >= sizeof ctx->err->snippet) {
        snippet_len = sizeof ctx->err->snippet - 1;
    }
    memcpy(ctx->err->snippet, line_start, snippet_len);
    ctx->err->snippet[snippet_len] = '\0';
}


/* ── Public writer ─────────────────────────────────────────────────────── */

char *sc_json_write(const ScValue *value, ScJsonWriteOpts opts) {
    ScSerdeBuf buf = { 0 };
    if (value) {
        write_value(&buf, value, &opts, 0);
    } else {
        sc_serde_buf_append_str(&buf, "null");
    }
    if (opts.trailing_newline) {
        sc_serde_buf_append_char(&buf, '\n');
    }
    return sc_serde_buf_finish(&buf);
}


/* ── Writer internals ──────────────────────────────────────────────────── */

/** Serializes one value (dispatching on its type). */
static bool write_value(
    ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
    int depth
) {
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
            char tmp[JSON_NUMBER_BUFFER];
            snprintf(tmp, sizeof tmp, "%lld", (long long)integer);
            return sc_serde_buf_append_str(buf, tmp);
        }
        case SC_VALUE_FLOAT: {
            double number = 0.0;
            sc_value_as_float(value, &number);
            return write_double(buf, number);
        }
        case SC_VALUE_STRING:
        case SC_VALUE_DATETIME:
            return write_string(buf, sc_value_as_string(value));
        case SC_VALUE_ARRAY:
            return write_array(buf, value, opts, depth);
        case SC_VALUE_OBJECT:
            return write_object(buf, value, opts, depth);
    }
    return false;
}

/** Pair view used to optionally sort object members by key. */
typedef struct JsonMember {
    const char    *key;
    const ScValue *value;
} JsonMember;

/** Orders members ascending by key bytes (for `sort_keys`). */
static int member_compare(const void *lhs, const void *rhs) {
    const JsonMember *left = lhs;
    const JsonMember *right = rhs;
    return strcmp(left->key, right->key);
}

/** Serializes an object, honoring indentation and `sort_keys`. */
static bool write_object(
    ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
    int depth
) {
    const ScValueObject *members = &value->as.object;
    if (!sc_serde_buf_append_char(buf, '{')) {
        return false;
    }
    if (members->count == 0) {
        return sc_serde_buf_append_char(buf, '}');
    }

    JsonMember *order = NULL;
    if (opts->sort_keys) {
        order = malloc(members->count * sizeof *order);
        if (!order) {
            buf->failed = true;
            return false;
        }
        for (size_t i = 0; i < members->count; i++) {
            order[i].key = members->keys[i];
            order[i].value = members->values[i];
        }
        qsort(order, members->count, sizeof *order, member_compare);
    }

    for (size_t i = 0; i < members->count; i++) {
        const char *key = order ? order[i].key : members->keys[i];
        const ScValue *child = order ? order[i].value : members->values[i];

        if (i > 0) {
            sc_serde_buf_append_char(buf, ',');
        }
        if (opts->indent > 0) {
            sc_serde_buf_append_char(buf, '\n');
            write_indent(buf, opts->indent, depth + 1);
        }
        write_string(buf, key);
        sc_serde_buf_append_char(buf, ':');
        if (opts->indent > 0) {
            sc_serde_buf_append_char(buf, ' ');
        }
        write_value(buf, child, opts, depth + 1);
    }

    free(order);
    if (opts->indent > 0) {
        sc_serde_buf_append_char(buf, '\n');
        write_indent(buf, opts->indent, depth);
    }
    return sc_serde_buf_append_char(buf, '}');
}

/** Serializes an array, honoring indentation. */
static bool write_array(
    ScSerdeBuf *buf, const ScValue *value, const ScJsonWriteOpts *opts,
    int depth
) {
    const ScValueArray *items = &value->as.array;
    if (!sc_serde_buf_append_char(buf, '[')) {
        return false;
    }
    if (items->count == 0) {
        return sc_serde_buf_append_char(buf, ']');
    }

    for (size_t i = 0; i < items->count; i++) {
        if (i > 0) {
            sc_serde_buf_append_char(buf, ',');
        }
        if (opts->indent > 0) {
            sc_serde_buf_append_char(buf, '\n');
            write_indent(buf, opts->indent, depth + 1);
        }
        write_value(buf, items->items[i], opts, depth + 1);
    }

    if (opts->indent > 0) {
        sc_serde_buf_append_char(buf, '\n');
        write_indent(buf, opts->indent, depth);
    }
    return sc_serde_buf_append_char(buf, ']');
}

/** Writes a quoted, escaped JSON string. */
static bool write_string(ScSerdeBuf *buf, const char *text) {
    if (!sc_serde_buf_append_char(buf, '"')) {
        return false;
    }
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
            char escape[7];
            snprintf(escape, sizeof escape, "\\u%04x", ch);
            sc_serde_buf_append_str(buf, escape);
        } else {
            sc_serde_buf_append_char(buf, (char)ch);
        }
    }
    return sc_serde_buf_append_char(buf, '"');
}

/** Writes a double so it always re-parses as a float (never as an int). */
static bool write_double(ScSerdeBuf *buf, double value) {
    if (!isfinite(value)) {
        // JSON cannot represent NaN/Infinity; emit null per common practice.
        return sc_serde_buf_append_str(buf, "null");
    }

    char tmp[JSON_NUMBER_BUFFER];
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

/** Emits `indent * depth` spaces. */
static bool write_indent(ScSerdeBuf *buf, int indent, int depth) {
    int total = indent * depth;
    for (int i = 0; i < total; i++) {
        if (!sc_serde_buf_append_char(buf, ' ')) {
            return false;
        }
    }
    return true;
}
