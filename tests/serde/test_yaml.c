#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_scalars(void);
static void check_nested_mapping(void);
static void check_block_sequence(void);
static void check_sequence_of_mappings(void);
static void check_flow(void);
static void check_quoted_and_comments(void);
static void check_block_scalars(void);
static void check_writer(void);
static void check_round_trip(void);
static void check_errors(void);
    static ScValue *parse(const char *yaml);


/** Exercises the YAML-subset reader and writer end-to-end. */
void test_serde_yaml(void) {
    check_scalars();
    check_nested_mapping();
    check_block_sequence();
    check_sequence_of_mappings();
    check_flow();
    check_quoted_and_comments();
    check_block_scalars();
    check_writer();
    check_round_trip();
    check_errors();
}


/* ── Scalars / mappings ──────────────────────────────────────────────────── */

static void check_scalars(void) {
    ScValue *root = parse(
        "name: sparcli\n"
        "count: 42\n"
        "ratio: 3.5\n"
        "enabled: true\n"
        "missing: null\n"
        "nothing: ~\n"
    );

    int64_t count = 0;
    double ratio = 0.0;
    bool enabled = false;
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "name")), "sparcli")
              == 0,
          "scalar: plain string");
    CHECK(sc_value_as_int(sc_value_get(root, "count"), &count) && count == 42,
          "scalar: integer");
    CHECK(sc_value_as_float(sc_value_get(root, "ratio"), &ratio)
              && ratio == 3.5,
          "scalar: float");
    CHECK(sc_value_as_bool(sc_value_get(root, "enabled"), &enabled) && enabled,
          "scalar: boolean");
    CHECK(sc_value_type(sc_value_get(root, "missing")) == SC_VALUE_NULL,
          "scalar: null keyword");
    CHECK(sc_value_type(sc_value_get(root, "nothing")) == SC_VALUE_NULL,
          "scalar: tilde null");

    sc_value_free(root);
}

static void check_nested_mapping(void) {
    ScValue *root = parse(
        "server:\n"
        "  host: localhost\n"
        "  ports:\n"
        "    http: 80\n"
        "    https: 443\n"
    );

    ScValue *server = sc_value_get(root, "server");
    CHECK(sc_value_type(server) == SC_VALUE_OBJECT,
          "nested: indented block becomes a mapping");
    CHECK(strcmp(sc_value_as_string(sc_value_get(server, "host")), "localhost")
              == 0,
          "nested: child key");
    int64_t https = 0;
    CHECK(sc_value_as_int(
              sc_value_get(sc_value_get(server, "ports"), "https"), &https)
              && https == 443,
          "nested: two levels deep");

    sc_value_free(root);
}

static void check_block_sequence(void) {
    ScValue *root = parse(
        "tags:\n"
        "  - alpha\n"
        "  - beta\n"
        "  - 3\n"
    );

    ScValue *tags = sc_value_get(root, "tags");
    int64_t third = 0;
    CHECK(sc_value_type(tags) == SC_VALUE_ARRAY && sc_value_len(tags) == 3,
          "sequence: block list length");
    CHECK(strcmp(sc_value_as_string(sc_value_at(tags, 0)), "alpha") == 0,
          "sequence: string item");
    CHECK(sc_value_as_int(sc_value_at(tags, 2), &third) && third == 3,
          "sequence: typed item");

    sc_value_free(root);
}

static void check_sequence_of_mappings(void) {
    ScValue *root = parse(
        "items:\n"
        "  - name: a\n"
        "    value: 1\n"
        "  - name: b\n"
        "    value: 2\n"
    );

    ScValue *items = sc_value_get(root, "items");
    CHECK(sc_value_len(items) == 2,
          "seq-of-maps: two entries");
    ScValue *second = sc_value_at(items, 1);
    int64_t value = 0;
    CHECK(sc_value_type(second) == SC_VALUE_OBJECT
              && strcmp(sc_value_as_string(sc_value_get(second, "name")), "b")
                     == 0
              && sc_value_as_int(sc_value_get(second, "value"), &value)
              && value == 2,
          "seq-of-maps: inline '- key: value' item parsed as a mapping");

    sc_value_free(root);
}

static void check_flow(void) {
    ScValue *root = parse(
        "list: [1, 2, 3]\n"
        "map: {x: 1, y: two}\n"
        "nested: [[a], {k: v}]\n"
    );

    ScValue *list = sc_value_get(root, "list");
    int64_t n = 0;
    CHECK(sc_value_len(list) == 3
              && sc_value_as_int(sc_value_at(list, 1), &n) && n == 2,
          "flow: sequence");

    ScValue *map = sc_value_get(root, "map");
    CHECK(sc_value_type(map) == SC_VALUE_OBJECT
              && strcmp(sc_value_as_string(sc_value_get(map, "y")), "two") == 0,
          "flow: mapping");

    ScValue *nested = sc_value_get(root, "nested");
    CHECK(sc_value_type(sc_value_at(nested, 0)) == SC_VALUE_ARRAY
              && sc_value_type(sc_value_at(nested, 1)) == SC_VALUE_OBJECT,
          "flow: nested collections");

    sc_value_free(root);
}

static void check_quoted_and_comments(void) {
    ScValue *root = parse(
        "# a comment\n"
        "dq: \"line\\tbreak\"  # trailing comment\n"
        "sq: 'it''s here'\n"
        "hash: \"a # b\"\n"
    );

    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "dq")), "line\tbreak")
              == 0,
          "quoted: double-quoted escapes, trailing comment stripped");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "sq")), "it's here")
              == 0,
          "quoted: single-quoted '' escape");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "hash")), "a # b") == 0,
          "comments: # inside quotes is not a comment");

    sc_value_free(root);
}

static void check_block_scalars(void) {
    ScValue *root = parse(
        "literal: |\n"
        "  line one\n"
        "  line two\n"
        "folded: >\n"
        "  word one\n"
        "  word two\n"
    );

    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "literal")),
                 "line one\nline two") == 0,
          "block scalar: literal joins with newlines");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "folded")),
                 "word one word two") == 0,
          "block scalar: folded joins with spaces");

    sc_value_free(root);
}


/* ── Writing / round trip ────────────────────────────────────────────────── */

static void check_writer(void) {
    ScValue *root = sc_value_object();
    sc_value_set(root, "name", sc_value_string("sparcli"));
    ScValue *tags = sc_value_array();
    sc_value_push(tags, sc_value_string("a"));
    sc_value_push(tags, sc_value_int(2));
    sc_value_set(root, "tags", tags);

    char *out = sc_yaml_write(root, (ScYamlWriteOpts){ 0 });
    const char *expected =
        "name: sparcli\n"
        "tags:\n"
        "  - a\n"
        "  - 2\n";
    CHECK(out != NULL && strcmp(out, expected) == 0,
          "write: mapping with a block sequence");
    free(out);

    // A string that looks like a bool/number must be quoted to round-trip.
    ScValue *tricky = sc_value_object();
    sc_value_set(tricky, "k", sc_value_string("true"));
    char *q = sc_yaml_write(tricky, (ScYamlWriteOpts){ 0 });
    CHECK(q != NULL && strcmp(q, "k: \"true\"\n") == 0,
          "write: a string 'true' is quoted");
    free(q);
    sc_value_free(tricky);
    sc_value_free(root);
}

static void check_round_trip(void) {
    const char *yaml =
        "title: Doc\n"
        "server:\n"
        "  host: localhost\n"
        "  port: 8080\n"
        "tags:\n"
        "  - a\n"
        "  - b\n";
    ScValue *root = parse(yaml);
    char *out = sc_yaml_write(root, (ScYamlWriteOpts){ 0 });
    ScValue *again = parse(out);

    int64_t port = 0;
    CHECK(again != NULL
              && sc_value_as_int(
                     sc_value_get(sc_value_get(again, "server"), "port"), &port)
              && port == 8080
              && strcmp(sc_value_as_string(sc_value_at(
                            sc_value_get(again, "tags"), 1)), "b") == 0,
          "round trip: parse -> write -> parse preserves structure");

    free(out);
    sc_value_free(again);
    sc_value_free(root);
}


/* ── Errors ──────────────────────────────────────────────────────────────── */

static void check_errors(void) {
    ScParseError err = { 0 };
    ScValue *bad_flow = sc_yaml_parse("a: [1, 2", strlen("a: [1, 2"), &err);
    CHECK(bad_flow == NULL && err.message[0] != '\0',
          "error: unterminated flow collection is rejected");

    ScParseError err2 = { 0 };
    ScValue *bad_quote = sc_yaml_parse("a: \"oops", strlen("a: \"oops"), &err2);
    CHECK(bad_quote == NULL,
          "error: unterminated quoted scalar is rejected");

    // An empty document parses to null (not an error).
    ScValue *empty = sc_yaml_parse("", 0, NULL);
    CHECK(sc_value_type(empty) == SC_VALUE_NULL,
          "empty: an empty document yields null");
    sc_value_free(empty);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

static ScValue *parse(const char *yaml) {
    ScParseError err = { 0 };
    ScValue *root = sc_yaml_parse(yaml, strlen(yaml), &err);
    if (!root) {
        printf("    (parse failed at line %d: %s)\n", err.line, err.message);
    }
    return root;
}
