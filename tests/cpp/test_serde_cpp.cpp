// test_serde_cpp.cpp - assertion suite for the serde C++ wrapper
// (include/serde/sparcli_serde.hpp).
//
// Verifies the wrapper's value-adds over the C serde API: RAII ownership
// (move-only, no double-free under ASan), std::optional accessors, the
// ParseError exception with line/column, and JSON round-tripping. Built and
// run under AddressSanitizer/UBSan by `make serde-cpp`.

#include <serde/sparcli_serde.hpp>

#include <cstdio>
#include <string>
#include <utility>

using namespace sparcli::serde;

static int g_failures = 0;

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            std::printf("  \033[32m✔\033[0m %s\n", (msg));                \
        } else {                                                               \
            std::printf("  \033[31m✘\033[0m %s\n", (msg));                \
            ++g_failures;                                                      \
        }                                                                      \
    } while (0)


static void check_build_and_write() {
    Value root = Value::object();
    root.set("name", Value::string("sparcli"));
    root.set("count", Value::integer(3));
    Value list = Value::array();
    list.push(Value::boolean(true));
    list.push(Value::null());
    root.set("flags", std::move(list));

    std::string out = json::write(root, { .sort_keys = true });
    CHECK(out == R"({"count":3,"flags":[true,null],"name":"sparcli"})",
          "build + write: object serializes with sorted keys");
}

static void check_parse_and_navigate() {
    Value root = json::parse(R"({"a":[10,20],"b":"hi","c":2.5})");

    View view = root.view();
    CHECK(view.type() == SC_VALUE_OBJECT, "parse: root is an object");
    CHECK(view.get("a").size() == 2, "navigate: nested array size");

    auto first = view.get("a").at(0).as_int();
    CHECK(first.has_value() && *first == 10, "navigate: int accessor (optional)");

    auto text = view.get("b").as_string();
    CHECK(text.has_value() && *text == "hi", "navigate: string accessor");

    auto number = view.get("c").as_double();
    CHECK(number.has_value() && *number == 2.5, "navigate: double accessor");

    CHECK(!view.get("missing").as_int().has_value(),
          "navigate: absent member yields empty optional");
}

static void check_round_trip() {
    const std::string input = R"({"x":[1,2,3],"y":null})";
    std::string output = json::write(json::parse(input));
    CHECK(output == input, "round trip: compact output is byte-identical");
}

static void check_parse_error() {
    bool threw = false;
    int line = 0;
    try {
        Value bad = json::parse("{ \"a\": }");
        (void)bad;
    } catch (const ParseError &error) {
        threw = true;
        line = error.line();
    }
    CHECK(threw && line == 1, "parse error: throws ParseError with a location");
}

static void check_move_semantics() {
    // Moving must not double-free: the moved-from object is left empty and its
    // destructor is a no-op. ASan/UBSan would flag a violation here.
    Value original = json::parse(R"([1,2,3])");
    Value moved = std::move(original);
    CHECK(moved.size() == 3, "move: ownership transfers, tree intact");
}

int main() {
    std::printf("\n== serde C++ wrapper ==\n");
    check_build_and_write();
    check_parse_and_navigate();
    check_round_trip();
    check_parse_error();
    check_move_semantics();

    if (g_failures > 0) {
        std::printf("\033[31m%d serde C++ check(s) failed.\033[0m\n",
                    g_failures);
        return 1;
    }
    std::printf("\033[32mAll serde C++ checks passed.\033[0m\n");
    return 0;
}
