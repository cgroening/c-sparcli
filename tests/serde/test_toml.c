#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_scalars(void);
static void check_tables_and_dotted_keys(void);
static void check_arrays_and_inline_tables(void);
static void check_array_of_tables(void);
static void check_strings(void);
static void check_datetime(void);
static void check_writer(void);
static void check_round_trip(void);
static void check_errors(void);
    static ScValue *parse(const char *toml);
    static bool parse_fails(const char *toml);


/** Exercises the TOML reader and writer end-to-end. */
void test_serde_toml(void) {
    check_scalars();
    check_tables_and_dotted_keys();
    check_arrays_and_inline_tables();
    check_array_of_tables();
    check_strings();
    check_datetime();
    check_writer();
    check_round_trip();
    check_errors();
}


/* ── Scalars ─────────────────────────────────────────────────────────────── */

static void check_scalars(void) {
    ScValue *root = parse(
        "title = \"sparcli\"\n"
        "count = 1_000\n"
        "hex = 0xff\n"
        "ratio = 3.14\n"
        "enabled = true\n"
    );

    int64_t count = 0;
    int64_t hex = 0;
    double ratio = 0.0;
    bool enabled = false;
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "title")), "sparcli")
              == 0,
          "scalar: string value");
    CHECK(sc_value_as_int(sc_value_get(root, "count"), &count) && count == 1000,
          "scalar: integer with underscores");
    CHECK(sc_value_as_int(sc_value_get(root, "hex"), &hex) && hex == 255,
          "scalar: hex integer");
    CHECK(sc_value_as_float(sc_value_get(root, "ratio"), &ratio)
              && ratio == 3.14,
          "scalar: float");
    CHECK(sc_value_as_bool(sc_value_get(root, "enabled"), &enabled) && enabled,
          "scalar: boolean");

    sc_value_free(root);
}

static void check_tables_and_dotted_keys(void) {
    ScValue *root = parse(
        "[server]\n"
        "host = \"localhost\"\n"
        "[server.tls]\n"
        "enabled = true\n"
        "[owner]\n"
        "name.first = \"Ada\"\n"
    );

    ScValue *server = sc_value_get(root, "server");
    CHECK(sc_value_type(server) == SC_VALUE_OBJECT, "table: [server] parsed");
    CHECK(strcmp(sc_value_as_string(sc_value_get(server, "host")), "localhost")
              == 0,
          "table: key inside table");

    ScValue *tls = sc_value_get(server, "tls");
    bool enabled = false;
    CHECK(sc_value_as_bool(sc_value_get(tls, "enabled"), &enabled) && enabled,
          "table: nested [server.tls]");

    ScValue *owner_name = sc_value_get(sc_value_get(root, "owner"), "name");
    CHECK(strcmp(sc_value_as_string(sc_value_get(owner_name, "first")), "Ada")
              == 0,
          "table: dotted key creates a sub-table");

    sc_value_free(root);
}

static void check_arrays_and_inline_tables(void) {
    ScValue *root = parse(
        "ports = [ 80, 443, 8080 ]\n"
        "matrix = [ [1, 2], [3, 4] ]\n"
        "point = { x = 1, y = 2 }\n"
    );

    ScValue *ports = sc_value_get(root, "ports");
    int64_t port = 0;
    CHECK(sc_value_len(ports) == 3
              && sc_value_as_int(sc_value_at(ports, 2), &port) && port == 8080,
          "array: integer array");

    ScValue *matrix = sc_value_get(root, "matrix");
    int64_t cell = 0;
    CHECK(sc_value_as_int(sc_value_at(sc_value_at(matrix, 1), 0), &cell)
              && cell == 3,
          "array: nested arrays");

    ScValue *point = sc_value_get(root, "point");
    int64_t y = 0;
    CHECK(sc_value_type(point) == SC_VALUE_OBJECT
              && sc_value_as_int(sc_value_get(point, "y"), &y) && y == 2,
          "inline table: key/value pairs");

    sc_value_free(root);
}

static void check_array_of_tables(void) {
    ScValue *root = parse(
        "[[product]]\n"
        "name = \"Hammer\"\n"
        "[[product]]\n"
        "name = \"Nail\"\n"
        "sku = 738\n"
    );

    ScValue *products = sc_value_get(root, "product");
    CHECK(sc_value_type(products) == SC_VALUE_ARRAY
              && sc_value_len(products) == 2,
          "array-of-tables: two entries");
    CHECK(strcmp(sc_value_as_string(
              sc_value_get(sc_value_at(products, 0), "name")), "Hammer") == 0,
          "array-of-tables: first entry field");
    int64_t sku = 0;
    CHECK(sc_value_as_int(sc_value_get(sc_value_at(products, 1), "sku"), &sku)
              && sku == 738,
          "array-of-tables: second entry field");

    sc_value_free(root);
}

static void check_strings(void) {
    ScValue *root = parse(
        "basic = \"a\\tb\\u00e9\"\n"
        "literal = 'c:\\temp\\new'\n"
        "multi = \"\"\"\nline1\nline2\"\"\"\n"
    );

    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "basic")),
                 "a\tb\xc3\xa9") == 0,
          "string: basic escapes incl \\u");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "literal")),
                 "c:\\temp\\new") == 0,
          "string: literal string keeps backslashes");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "multi")),
                 "line1\nline2") == 0,
          "string: multi-line trims the leading newline");

    sc_value_free(root);
}

static void check_datetime(void) {
    ScValue *root = parse(
        "odt = 1979-05-27T07:32:00Z\n"
        "ldt = 1979-05-27 07:32:00\n"
        "date = 1979-05-27\n"
        "time = 07:32:00\n"
    );

    CHECK(sc_value_type(sc_value_get(root, "odt")) == SC_VALUE_DATETIME
              && strcmp(sc_value_as_string(sc_value_get(root, "odt")),
                        "1979-05-27T07:32:00Z") == 0,
          "datetime: offset datetime kept verbatim");
    CHECK(strcmp(sc_value_as_string(sc_value_get(root, "ldt")),
                 "1979-05-27 07:32:00") == 0,
          "datetime: space-separated local datetime kept verbatim");
    CHECK(sc_value_type(sc_value_get(root, "date")) == SC_VALUE_DATETIME,
          "datetime: local date");
    CHECK(sc_value_type(sc_value_get(root, "time")) == SC_VALUE_DATETIME,
          "datetime: local time");

    sc_value_free(root);
}


/* ── Writing ─────────────────────────────────────────────────────────────── */

static void check_writer(void) {
    ScValue *root = sc_value_object();
    sc_value_set(root, "name", sc_value_string("sparcli"));
    sc_value_set(root, "count", sc_value_int(3));

    ScValue *server = sc_value_object();
    sc_value_set(server, "host", sc_value_string("localhost"));
    sc_value_set(root, "server", server);

    char *out = sc_toml_write(root, (ScTomlWriteOpts){ 0 });
    const char *expected =
        "name = \"sparcli\"\n"
        "count = 3\n"
        "\n[server]\n"
        "host = \"localhost\"\n";
    CHECK(out != NULL && strcmp(out, expected) == 0,
          "write: scalars first, then [table] section");
    free(out);
    sc_value_free(root);
}

static void check_round_trip(void) {
    const char *toml =
        "name = \"sparcli\"\n"
        "ports = [80, 443]\n"
        "\n[owner]\n"
        "active = true\n"
        "\n[[item]]\n"
        "id = 1\n"
        "\n[[item]]\n"
        "id = 2\n";

    ScValue *root = parse(toml);
    char *out = sc_toml_write(root, (ScTomlWriteOpts){ 0 });
    // Re-parse the output and compare a few values (formatting is normalized,
    // so compare semantically rather than byte-for-byte on the whole doc).
    ScValue *again = parse(out);
    int64_t id = 0;
    CHECK(again != NULL
              && sc_value_as_int(
                     sc_value_get(sc_value_at(sc_value_get(again, "item"), 1),
                                  "id"), &id)
              && id == 2,
          "round trip: parse -> write -> parse preserves array-of-tables");
    free(out);
    sc_value_free(again);
    sc_value_free(root);
}


/* ── Errors ──────────────────────────────────────────────────────────────── */

static void check_errors(void) {
    CHECK(parse_fails("key = "), "error: missing value is rejected");
    CHECK(parse_fails("key \"value\""), "error: missing '=' is rejected");
    CHECK(parse_fails("a = [1, 2"), "error: unterminated array is rejected");
    CHECK(parse_fails("s = \"oops"), "error: unterminated string is rejected");
    CHECK(parse_fails("[table"), "error: unterminated header is rejected");

    ScParseError err = { 0 };
    ScValue *root = sc_toml_parse("x = @bad", strlen("x = @bad"), &err);
    CHECK(root == NULL && err.line == 1 && err.message[0] != '\0',
          "error: parse error records line/message");
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

static ScValue *parse(const char *toml) {
    ScParseError err = { 0 };
    ScValue *root = sc_toml_parse(toml, strlen(toml), &err);
    if (!root) {
        printf("    (parse failed at line %d: %s)\n", err.line, err.message);
    }
    return root;
}

static bool parse_fails(const char *toml) {
    ScValue *root = sc_toml_parse(toml, strlen(toml), NULL);
    if (root) {
        sc_value_free(root);
        return false;
    }
    return true;
}
