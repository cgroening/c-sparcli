#include "test_serde.h"


// Forward declarations indented to reflect the call hierarchy.
static void check_scalars(void);
static void check_array(void);
static void check_object(void);
static void check_object_replace_keeps_order(void);
static void check_type_mismatches(void);


/** Exercises the data model: scalars, containers, ownership and accessors. */
void test_serde_value(void) {
    check_scalars();
    check_array();
    check_object();
    check_object_replace_keeps_order();
    check_type_mismatches();
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
