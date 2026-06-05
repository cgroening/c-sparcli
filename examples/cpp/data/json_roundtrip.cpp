// json_roundtrip.cpp - parse JSON, navigate the tree, pretty-print it back
// (serde C++ wrapper: owning Value, non-owning View, std::optional accessors).
//
// Run (after `make`):
//   make run-example EX=cpp/data/json_roundtrip

#include <serde/sparcli_serde.hpp>

#include <cstdint>
#include <print>
#include <string_view>

using namespace sparcli::serde;

int main() {
    // parse() throws serde::ParseError on malformed input.
    Value root =
        json::parse(R"({"name":"sparcli","version":[0,1,0],"stable":false})");

    // Accessors on a (non-owning) View return std::optional.
    View view = root.view();
    std::optional<std::string_view> name = view.get("name").as_string();
    std::optional<std::int64_t> major = view.get("version").at(0).as_int();

    std::println("name  = {}", name.value_or("(none)"));
    std::println("major = {}", major.value_or(-1));

    // Re-serialize with indentation.
    std::println("{}", json::write(root, { .indent = 2 }));
    return 0;
}
