#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_scalars(void);
static void check_array(void);
static void check_object(void);
static void check_object_replace_keeps_order(void);
static void check_type_mismatches(void);
static void check_remove(void);
static void check_convenience_getters(void);
static void check_path(void);
static void check_clone(void);
static void check_merge(void);


/** Exercises the data model: scalars, containers, ownership and accessors. */
void test_serde_value(void) {
    check_scalars();
    check_array();
    check_object();
    check_object_replace_keeps_order();
    check_type_mismatches();
    check_remove();
    check_convenience_getters();
    check_path();
    check_clone();
    check_merge();
}


/* ── Scalars ─────────────────────────────────────────────────────────────── */

static void check_scalars(void) {
    ScValue *null_value = sc_value_null();
    ScValue *boolean = sc_value_bool(true);
    ScValue *integer = sc_value_int(-42);
    ScValue *number = sc_value_float(3.5);
    ScValue *text = sc_value_string("hello");

    CHECK(sc_value_type(null_value) == SC_VALUE_NULL, "null has null type");
    CHECK(sc_value_type(NULL) == SC_VALUE_NULL, "NULL reads as null type");

    bool flag = false;
    CHECK(sc_value_as_bool(boolean, &flag) && flag, "bool reads back true");

    int64_t got_int = 0;
    CHECK(sc_value_as_int(integer, &got_int) && got_int == -42,
          "int reads back exactly");

    double got_float = 0.0;
    CHECK(sc_value_as_float(number, &got_float) && got_float == 3.5,
          "float reads back exactly");
    CHECK(sc_value_as_float(integer, &got_float) && got_float == -42.0,
          "as_float promotes an int");

    CHECK(sc_value_as_string(text) != NULL
              && strcmp(sc_value_as_string(text), "hello") == 0,
          "string reads back its bytes");
    CHECK(sc_value_as_string(integer) == NULL,
          "as_string rejects a non-string");

    sc_value_free(null_value);
    sc_value_free(boolean);
    sc_value_free(integer);
    sc_value_free(number);
    sc_value_free(text);
}


/* ── Arrays ──────────────────────────────────────────────────────────────── */

static void check_array(void) {
    ScValue *array = sc_value_array();
    for (int i = 0; i < 10; i++) {
        CHECK(sc_value_push(array, sc_value_int(i)), "push grows the array");
    }

    CHECK(sc_value_len(array) == 10, "array length tracks pushes");

    int64_t third = 0;
    CHECK(sc_value_as_int(sc_value_at(array, 2), &third) && third == 2,
          "array element is retrievable by index");
    CHECK(sc_value_at(array, 99) == NULL, "out-of-range index returns NULL");

    sc_value_free(array); // must recursively free the 10 children
    CHECK(true, "array frees its children without leaking (see sanitizer)");
}


/* ── Objects ─────────────────────────────────────────────────────────────── */

static void check_object(void) {
    ScValue *object = sc_value_object();
    CHECK(sc_value_set(object, "name", sc_value_string("sparcli")),
          "set adds a member");
    CHECK(sc_value_set(object, "count", sc_value_int(3)),
          "set adds a second member");

    CHECK(sc_value_len(object) == 2, "object length tracks members");
    CHECK(sc_value_as_string(sc_value_get(object, "name")) != NULL
              && strcmp(sc_value_as_string(sc_value_get(object, "name")),
                        "sparcli") == 0,
          "member is retrievable by key");
    CHECK(sc_value_get(object, "missing") == NULL,
          "absent key returns NULL");
    CHECK(sc_value_key_at(object, 0) != NULL
              && strcmp(sc_value_key_at(object, 0), "name") == 0,
          "keys keep insertion order");

    sc_value_free(object);
}

static void check_object_replace_keeps_order(void) {
    ScValue *object = sc_value_object();
    sc_value_set(object, "a", sc_value_int(1));
    sc_value_set(object, "b", sc_value_int(2));
    sc_value_set(object, "a", sc_value_int(99)); // replaces, frees old value

    int64_t replaced = 0;
    CHECK(sc_value_len(object) == 2, "replacing a key does not grow the object");
    CHECK(sc_value_as_int(sc_value_get(object, "a"), &replaced)
              && replaced == 99,
          "replacing a key stores the new value");
    CHECK(strcmp(sc_value_key_at(object, 0), "a") == 0,
          "replaced key keeps its original position");

    sc_value_free(object);
}


/* ── Type guards ─────────────────────────────────────────────────────────── */

static void check_type_mismatches(void) {
    ScValue *integer = sc_value_int(1);

    // A rejected push leaves ownership with the caller, so we still free it.
    ScValue *orphan = sc_value_null();
    CHECK(!sc_value_push(integer, orphan),
          "push on a non-array is rejected (ownership stays with caller)");
    sc_value_free(orphan);

    CHECK(sc_value_at(integer, 0) == NULL, "at on a non-array returns NULL");
    CHECK(sc_value_get(integer, "x") == NULL,
          "get on a non-object returns NULL");

    bool flag = false;
    CHECK(!sc_value_as_bool(integer, &flag), "as_bool rejects an int");

    sc_value_free(integer);
}


/* ── Removal ─────────────────────────────────────────────────────────────── */

static void check_remove(void) {
    ScValue *object = sc_value_object();
    sc_value_set(object, "a", sc_value_int(1));
    sc_value_set(object, "b", sc_value_int(2));
    sc_value_set(object, "c", sc_value_int(3));

    CHECK(sc_value_remove(object, "b") && sc_value_len(object) == 2,
          "remove: deletes a member");
    CHECK(sc_value_get(object, "b") == NULL,
          "remove: the member is gone");
    CHECK(strcmp(sc_value_key_at(object, 1), "c") == 0,
          "remove: remaining members keep order");
    CHECK(!sc_value_remove(object, "missing"),
          "remove: absent key returns false");
    sc_value_free(object);

    ScValue *array = sc_value_array();
    for (int i = 0; i < 4; i++) {
        sc_value_push(array, sc_value_int(i));
    }
    int64_t value = 0;
    CHECK(sc_value_remove_at(array, 1) && sc_value_len(array) == 3,
          "remove_at: deletes an element");
    CHECK(sc_value_as_int(sc_value_at(array, 1), &value) && value == 2,
          "remove_at: later elements shift down");
    CHECK(!sc_value_remove_at(array, 99),
          "remove_at: out-of-range returns false");
    sc_value_free(array);
}


/* ── Convenience getters / path ──────────────────────────────────────────── */

static void check_convenience_getters(void) {
    ScValue *object = sc_value_object();
    sc_value_set(object, "port", sc_value_int(8080));
    sc_value_set(object, "tls", sc_value_bool(true));
    sc_value_set(object, "name", sc_value_string("sparcli"));

    CHECK(sc_value_get_int_or(object, "port", 0) == 8080,
          "get_int_or: returns the value when present");
    CHECK(sc_value_get_int_or(object, "missing", 42) == 42,
          "get_int_or: returns the fallback when absent");
    CHECK(sc_value_get_bool_or(object, "tls", false),
          "get_bool_or: returns the boolean");
    CHECK(!sc_value_get_bool_or(object, "name", false),
          "get_bool_or: type mismatch falls back");
    CHECK(strcmp(sc_value_get_string_or(object, "name", "?"), "sparcli") == 0,
          "get_string_or: returns the string");
    CHECK(strcmp(sc_value_get_string_or(object, "missing", "def"), "def") == 0,
          "get_string_or: fallback when absent");

    sc_value_free(object);
}

static void check_path(void) {
    const char *json = "{\"server\":{\"tls\":{\"enabled\":true},"
                       "\"hosts\":[\"a\",\"b\"]}}";
    ScValue *root = sc_json_parse(json, strlen(json), NULL);

    bool enabled = false;
    CHECK(sc_value_as_bool(
              sc_value_path(root, "server.tls.enabled"), &enabled) && enabled,
          "path: dotted object path resolves");
    CHECK(strcmp(sc_value_as_string(sc_value_path(root, "server.hosts.1")),
                 "b") == 0,
          "path: array index segment resolves");
    CHECK(sc_value_path(root, "server.missing.x") == NULL,
          "path: unknown segment returns NULL");
    CHECK(sc_value_path(root, "server.hosts.9") == NULL,
          "path: out-of-range index returns NULL");
    CHECK(sc_value_path(root, "") == root,
          "path: empty path returns the root");

    sc_value_free(root);
}


/* ── Clone / merge ───────────────────────────────────────────────────────── */

static void check_clone(void) {
    const char *json = "{\"a\":[1,2,{\"b\":true}],\"c\":\"x\"}";
    ScValue *original = sc_json_parse(json, strlen(json), NULL);
    ScValue *copy = sc_value_clone(original);

    // The copy must be independent: free the original, then read the copy.
    sc_value_free(original);

    bool inner = false;
    CHECK(copy != NULL
              && sc_value_as_bool(
                     sc_value_path(copy, "a.2.b"), &inner) && inner,
          "clone: deep copy survives freeing the original");
    CHECK(strcmp(sc_value_as_string(sc_value_get(copy, "c")), "x") == 0,
          "clone: scalar members are copied");
    sc_value_free(copy);
}

static void check_merge(void) {
    // base: defaults; overlay: user config that adds, overrides and deep-merges.
    ScValue *base = sc_json_parse(
        "{\"host\":\"localhost\",\"tls\":{\"on\":false,\"port\":443}}",
        strlen("{\"host\":\"localhost\",\"tls\":{\"on\":false,\"port\":443}}"),
        NULL);
    ScValue *overlay = sc_json_parse(
        "{\"tls\":{\"on\":true},\"extra\":1}",
        strlen("{\"tls\":{\"on\":true},\"extra\":1}"), NULL);

    bool ok = sc_value_merge(base, overlay);

    bool on = false;
    int64_t port = 0;
    int64_t extra = 0;
    CHECK(ok, "merge: succeeds for two objects");
    CHECK(sc_value_as_bool(sc_value_path(base, "tls.on"), &on) && on,
          "merge: overlay overrides a nested scalar");
    CHECK(sc_value_as_int(sc_value_path(base, "tls.port"), &port)
              && port == 443,
          "merge: untouched nested keys are preserved (deep merge)");
    CHECK(sc_value_as_int(sc_value_get(base, "extra"), &extra) && extra == 1,
          "merge: overlay adds new keys");
    CHECK(strcmp(sc_value_as_string(sc_value_get(base, "host")), "localhost")
              == 0,
          "merge: base-only keys remain");

    sc_value_free(base);
    sc_value_free(overlay);
}
