#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_parse_scalars(void);
static void check_parse_structures(void);
static void check_numbers(void);
static void check_unicode_escapes(void);
static void check_round_trip(void);
static void check_pretty_and_sort(void);
static void check_write_escapes(void);
static void check_errors(void);
static void check_deep_nesting(void);
    static char *reformat(const char *json, ScJsonWriteOpts opts);
    static bool parse_fails(const char *json, ScParseError *err);


/** Exercises the JSON reader and writer end-to-end. */
void test_serde_json(void) {
    check_parse_scalars();
    check_parse_structures();
    check_numbers();
    check_unicode_escapes();
    check_round_trip();
    check_pretty_and_sort();
    check_write_escapes();
    check_errors();
    check_deep_nesting();
}


/* ── Parsing ─────────────────────────────────────────────────────────────── */

static void check_parse_scalars(void) {
    ScParseError err = { 0 };

    ScValue *null_value = sc_json_parse("null", 4, &err);
    CHECK(sc_value_type(null_value) == SC_VALUE_NULL, "parse: null");
    sc_value_free(null_value);

    bool flag = false;
    ScValue *boolean = sc_json_parse("true", 4, &err);
    CHECK(sc_value_as_bool(boolean, &flag) && flag, "parse: true");
    sc_value_free(boolean);

    ScValue *text = sc_json_parse("\"hi\\n\"", 6, &err);
    CHECK(sc_value_as_string(text) != NULL
              && strcmp(sc_value_as_string(text), "hi\n") == 0,
          "parse: string with escape");
    sc_value_free(text);
}

static void check_parse_structures(void) {
    const char *json = "{\"a\":[1,2,{\"b\":true}],\"c\":null}";
    ScParseError err = { 0 };
    ScValue *root = sc_json_parse(json, strlen(json), &err);

    CHECK(sc_value_type(root) == SC_VALUE_OBJECT, "parse: nested object root");

    ScValue *array = sc_value_get(root, "a");
    CHECK(sc_value_type(array) == SC_VALUE_ARRAY && sc_value_len(array) == 3,
          "parse: nested array length");

    ScValue *inner = sc_value_at(array, 2);
    bool inner_flag = false;
    CHECK(sc_value_as_bool(sc_value_get(inner, "b"), &inner_flag) && inner_flag,
          "parse: value inside nested object");

    sc_value_free(root);
}

static void check_numbers(void) {
    ScParseError err = { 0 };

    int64_t integer = 0;
    ScValue *int_value = sc_json_parse("-1234", 5, &err);
    CHECK(sc_value_type(int_value) == SC_VALUE_INT
              && sc_value_as_int(int_value, &integer) && integer == -1234,
          "parse: negative integer stays int");
    sc_value_free(int_value);

    double number = 0.0;
    ScValue *float_value = sc_json_parse("1.5e3", 5, &err);
    CHECK(sc_value_type(float_value) == SC_VALUE_FLOAT
              && sc_value_as_float(float_value, &number) && number == 1500.0,
          "parse: exponent becomes float");
    sc_value_free(float_value);

    ScValue *fraction = sc_json_parse("2.0", 3, &err);
    CHECK(sc_value_type(fraction) == SC_VALUE_FLOAT,
          "parse: trailing .0 stays float");
    sc_value_free(fraction);
}

static void check_unicode_escapes(void) {
    ScParseError err = { 0 };

    // U+20AC EURO SIGN via \u, and an astral code point via a surrogate pair.
    ScValue *euro = sc_json_parse("\"\\u20ac\"", 8, &err);
    CHECK(sc_value_as_string(euro) != NULL
              && strcmp(sc_value_as_string(euro), "\xe2\x82\xac") == 0,
          "parse: \\u BMP escape decodes to UTF-8");
    sc_value_free(euro);

    ScValue *emoji = sc_json_parse("\"\\ud83d\\ude00\"", 14, &err);
    CHECK(sc_value_as_string(emoji) != NULL
              && strcmp(sc_value_as_string(emoji), "\xf0\x9f\x98\x80") == 0,
          "parse: surrogate pair decodes to 4-byte UTF-8");
    sc_value_free(emoji);
}


/* ── Writing / round trip ────────────────────────────────────────────────── */

static void check_round_trip(void) {
    const char *json = "{\"a\":[1,2,3],\"b\":\"x\",\"c\":true,\"d\":null}";
    char *out = reformat(json, (ScJsonWriteOpts){ 0 });
    CHECK(out != NULL && strcmp(out, json) == 0,
          "round trip: compact output is byte-identical");
    free(out);
}

static void check_pretty_and_sort(void) {
    const char *json = "{\"b\":1,\"a\":2}";

    char *pretty = reformat(json, (ScJsonWriteOpts){ .indent = 2 });
    const char *expected_pretty =
        "{\n  \"b\": 1,\n  \"a\": 2\n}";
    CHECK(pretty != NULL && strcmp(pretty, expected_pretty) == 0,
          "write: indentation matches expected layout");
    free(pretty);

    char *sorted = reformat(json, (ScJsonWriteOpts){ .sort_keys = true });
    CHECK(sorted != NULL && strcmp(sorted, "{\"a\":2,\"b\":1}") == 0,
          "write: sort_keys orders members ascending");
    free(sorted);

    char *with_newline = reformat(json,
        (ScJsonWriteOpts){ .trailing_newline = true });
    CHECK(with_newline != NULL
              && with_newline[strlen(with_newline) - 1] == '\n',
          "write: trailing_newline appends a newline");
    free(with_newline);
}

static void check_write_escapes(void) {
    ScValue *value = sc_value_string("tab\tquote\"slash\\bell\x07");
    char *out = sc_json_write(value, (ScJsonWriteOpts){ 0 });
    CHECK(out != NULL
              && strcmp(out, "\"tab\\tquote\\\"slash\\\\bell\\u0007\"") == 0,
          "write: control bytes and quotes are escaped");
    free(out);
    sc_value_free(value);

    char *null_out = sc_json_write(NULL, (ScJsonWriteOpts){ 0 });
    CHECK(null_out != NULL && strcmp(null_out, "null") == 0,
          "write: NULL value serializes to null");
    free(null_out);
}


/* ── Errors ──────────────────────────────────────────────────────────────── */

static void check_errors(void) {
    ScParseError err = { 0 };

    CHECK(parse_fails("{\"a\":}", &err),
          "error: missing value is rejected");
    CHECK(parse_fails("[1,2", &err), "error: unterminated array is rejected");
    CHECK(parse_fails("\"oops", &err), "error: unterminated string is rejected");
    CHECK(parse_fails("nul", &err), "error: truncated literal is rejected");
    CHECK(parse_fails("1 2", &err), "error: trailing data is rejected");

    // The error carries a usable location for the trailing-data case.
    CHECK(err.line == 1 && err.column > 0 && err.message[0] != '\0',
          "error: parse error records line/column/message");

    ScError *report = sc_parse_error_to_error(&err);
    CHECK(report != NULL && sc_error_code(report) == 2,
          "error: bridge produces an ScError with exit code 2");
    sc_error_free(report);
}

static void check_deep_nesting(void) {
    // Build a string of many '[' to overflow the depth guard; it must fail
    // cleanly (the sanitizer would catch a stack overflow otherwise).
    char deep[1024];
    memset(deep, '[', sizeof deep);
    ScParseError err = { 0 };
    ScValue *root = sc_json_parse(deep, sizeof deep, &err);
    CHECK(root == NULL && strstr(err.message, "depth") != NULL,
          "error: excessive nesting is rejected by the depth guard");
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** Parses `json` then re-serializes it with `opts`; returns heap output. */
static char *reformat(const char *json, ScJsonWriteOpts opts) {
    ScParseError err = { 0 };
    ScValue *root = sc_json_parse(json, strlen(json), &err);
    if (!root) {
        return NULL;
    }
    char *out = sc_json_write(root, opts);
    sc_value_free(root);
    return out;
}

/** Returns true when parsing `json` fails; `err` receives the detail. */
static bool parse_fails(const char *json, ScParseError *err) {
    ScValue *root = sc_json_parse(json, strlen(json), err);
    if (root) {
        sc_value_free(root);
        return false;
    }
    return true;
}
