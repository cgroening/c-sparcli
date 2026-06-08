// test_serde_cpp.cpp - assertion suite for the serde C++ wrapper
// (include/serde/sparcli_serde.hpp).
//
// Verifies the wrapper's value-adds over the C serde API: RAII ownership
// (move-only, no double-free under ASan), std::optional accessors, the
// ParseError exception with line/column, and JSON round-tripping. Built and
// run under AddressSanitizer/UBSan by `make qa-serde-cpp`.

#include <serde/sparcli_serde.hpp>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <unistd.h>

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

static void check_csv() {
    Csv table = Csv::parse("name,age\nAlice,30\nBob,25\n",
                           { .has_header = true });
    CHECK(table.has_header() && table.data_rows() == 2,
          "csv: header parsed, data rows counted");
    CHECK(table.get(0, "name") == "Alice" && table.get(1, "age") == "25",
          "csv: lookup by header name");

    Csv built = Csv::create();
    built.add_row({ "a,b", "plain" });
    CHECK(built.write() == "\"a,b\",plain\n",
          "csv: writer quotes only when needed");
}

static void check_toml() {
    Value root = toml::parse("name = \"sparcli\"\n[server]\nport = 8080\n");
    auto port = root.view().get("server").get("port").as_int();
    CHECK(port.has_value() && *port == 8080, "toml: parse + nested lookup");

    Value doc = Value::object();
    doc.set("title", Value::string("hi"));
    CHECK(toml::write(doc) == "title = \"hi\"\n",
          "toml: write a scalar key");
}

static void check_yaml() {
    Value root = yaml::parse("name: sparcli\nport: 8080\ntags:\n  - a\n  - b\n");
    auto port = root.view().get("port").as_int();
    auto tag = root.view().get("tags").at(1).as_string();
    CHECK(port.has_value() && *port == 8080 && tag.has_value() && *tag == "b",
          "yaml: parse mapping + sequence");

    Value doc = Value::object();
    doc.set("k", Value::string("v"));
    CHECK(yaml::write(doc) == "k: v\n", "yaml: write a scalar key");
}

static void check_markdown() {
    Markdown md = Markdown::parse(
        "+++\ntitle = \"Doc\"\n+++\n# Intro\nHi.\n## Sub\nDeep.\n");
    CHECK(md.frontmatter_format() == SC_MD_FRONTMATTER_TOML
              && md.frontmatter().has_value(),
          "markdown: TOML front matter parsed");

    Section sub = md.root().find("Intro/Sub");
    CHECK(static_cast<bool>(sub) && sub.body() == "Deep.",
          "markdown: find nested section + body");

    Markdown built = Markdown::create();
    Section intro = built.root().add(1, "Title");
    intro.set_body("Body.");
    CHECK(built.write() == "# Title\n\nBody.\n",
          "markdown: build and write a section");

    // Front matter set from a value tree is serialized and re-parses.
    Value meta = Value::object();
    meta.set("title", Value::string("Doc"));
    built.set_frontmatter(SC_MD_FRONTMATTER_TOML, meta);
    Markdown again = Markdown::parse(built.write());
    CHECK(again.frontmatter().has_value()
              && again.frontmatter()->get("title").as_string() == "Doc",
          "markdown: set_frontmatter from a value re-parses");
    CHECK(!again.frontmatter_malformed() && !again.frontmatter_error(),
          "markdown: valid front matter is not flagged malformed");

    // A present-but-broken block: lenient parse, but flagged + error exposed.
    Markdown broken = Markdown::parse("---\n[unterminated\n---\nBody.\n");
    CHECK(broken.frontmatter_malformed()
              && !broken.frontmatter().has_value()
              && broken.frontmatter_error().has_value(),
          "markdown: malformed front matter is flagged with an error");
}

static void check_value_ops() {
    Value root = Value::object();
    root.set("a", Value::integer(1));
    root.set("b", Value::integer(2));
    CHECK(root.remove("a") && root.size() == 1, "value: remove member");

    Value list = Value::array();
    list.push(Value::integer(10));
    list.push(Value::integer(20));
    CHECK(list.remove_at(0) && list.size() == 1
              && list.view().at(0).as_int().value_or(0) == 20,
          "value: remove_at element");

    // path on a parsed tree.
    Value parsed = json::parse(R"({"server":{"hosts":["x","y"]},"n":7})");
    CHECK(parsed.path("server.hosts.1").as_string().value_or("") == "y",
          "value: path() resolves object + array segments");

    // clone independence: free source by reassigning, copy stays valid.
    Value copy = parsed.clone();
    parsed = Value::null();
    CHECK(copy.path("n").as_int().value_or(0) == 7,
          "value: clone is independent of the source");

    // deep merge.
    Value base = json::parse(R"({"tls":{"on":false,"port":443}})");
    Value overlay = json::parse(R"({"tls":{"on":true},"extra":1})");
    base.merge(overlay);
    CHECK(base.path("tls.on").as_bool().value_or(false)
              && base.path("tls.port").as_int().value_or(0) == 443
              && base.path("extra").as_int().value_or(0) == 1,
          "value: merge overrides, preserves and adds");
}

static void check_file_io() {
    char path[] = "/tmp/sparcli_serde_cpp_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        CHECK(false, "file io: mkstemp failed");
        return;
    }
    close(fd);

    Value value = json::parse(R"({"k":42})");
    bool wrote = json::write_file(value, path);
    Value back = json::parse_file(path);
    CHECK(wrote && back.view().get("k").as_int().value_or(0) == 42,
          "file io: write_file -> parse_file round-trips");
    unlink(path);

    bool threw = false;
    try {
        Value missing = json::parse_file("/nonexistent/sparcli/zzz.json");
        (void)missing;
    } catch (const ParseError &) {
        threw = true;
    }
    CHECK(threw, "file io: a missing file throws ParseError");
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
    check_csv();
    check_toml();
    check_yaml();
    check_markdown();
    check_value_ops();
    check_file_io();
    check_move_semantics();

    if (g_failures > 0) {
        std::printf("\033[31m%d serde C++ check(s) failed.\033[0m\n",
                    g_failures);
        return 1;
    }
    std::printf("\033[32mAll serde C++ checks passed.\033[0m\n");
    return 0;
}
