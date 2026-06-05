#pragma once
/**
 * @file sparcli_serde.hpp
 * @brief Header-only C++20 wrapper around the sparcli serde layer.
 *
 * Adds, on top of the C serde API:
 * - an **RAII owning** `Value` (move-only; one destructor frees the tree),
 * - a non-owning `View` for navigating a tree without taking ownership,
 * - `std::optional` accessors and `std::string` serializer results,
 * - a `ParseError` exception (derived from `std::runtime_error`) carrying the
 *   source line/column.
 *
 * Like the C umbrella, this header is deliberately separate from
 * `sparcli.hpp`: include it only when you need the parsers.
 *
 * @code
 * using namespace sparcli::serde;
 * Value root = json::parse(R"({"name":"sparcli","count":3})");
 * auto name = root.view().get("name").as_string();   // std::optional
 * std::string pretty = json::write(root, { .indent = 2 });
 * @endcode
 */

#include <serde/sparcli_serde.h>

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace sparcli::serde {

/** Value discriminator, reusing the C enum verbatim. */
using Type = ScValueType;

/** Writer options, reusing the C struct verbatim. */
using JsonWriteOpts = ScJsonWriteOpts;


/** Thrown by the parsers on malformed input; carries the source location. */
class ParseError : public std::runtime_error {
public:
    explicit ParseError(const ScParseError &error)
        : std::runtime_error(compose(error)),
          line_(error.line),
          column_(error.column) {}

    /** 1-based source line of the failure (0 when unknown). */
    [[nodiscard]] int line() const noexcept { return line_; }

    /** 1-based source column of the failure (0 when unknown). */
    [[nodiscard]] int column() const noexcept { return column_; }

private:
    static std::string compose(const ScParseError &error) {
        std::string message = error.message;
        if (error.line > 0) {
            message += " (line " + std::to_string(error.line) + ", column "
                + std::to_string(error.column) + ")";
        }
        return message;
    }

    int line_;
    int column_;
};


/**
 * Non-owning view over a value node. Cheap to copy; valid only while the
 * owning `Value` tree it points into is alive.
 */
class View {
public:
    explicit View(const ScValue *node) noexcept : node_(node) {}

    [[nodiscard]] const ScValue *get() const noexcept { return node_; }
    [[nodiscard]] Type type() const noexcept { return sc_value_type(node_); }
    [[nodiscard]] std::size_t size() const noexcept {
        return sc_value_len(node_);
    }

    [[nodiscard]] std::optional<bool> as_bool() const noexcept {
        bool value = false;
        if (sc_value_as_bool(node_, &value)) { return value; }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::int64_t> as_int() const noexcept {
        std::int64_t value = 0;
        if (sc_value_as_int(node_, &value)) { return value; }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<double> as_double() const noexcept {
        double value = 0.0;
        if (sc_value_as_float(node_, &value)) { return value; }
        return std::nullopt;
    }
    [[nodiscard]] std::optional<std::string_view> as_string() const noexcept {
        const char *text = sc_value_as_string(node_);
        if (text) { return std::string_view(text); }
        return std::nullopt;
    }

    /** Array element by index; an absent element yields a null `View`. */
    [[nodiscard]] View at(std::size_t index) const noexcept {
        return View(sc_value_at(node_, index));
    }
    /** Object member by key; an absent member yields a null `View`. */
    [[nodiscard]] View get(std::string_view key) const {
        return View(sc_value_get(node_, std::string(key).c_str()));
    }
    /** Object member key by insertion position (empty when out of range). */
    [[nodiscard]] std::string_view key_at(std::size_t index) const noexcept {
        const char *key = sc_value_key_at(node_, index);
        return key ? std::string_view(key) : std::string_view();
    }

    /** True when this view points at a real node. */
    [[nodiscard]] explicit operator bool() const noexcept {
        return node_ != nullptr;
    }

private:
    const ScValue *node_;
};


/** RAII owner of an `ScValue` tree (move-only). */
class Value {
public:
    Value(const Value &) = delete;
    Value &operator=(const Value &) = delete;

    Value(Value &&other) noexcept
        : node_(std::exchange(other.node_, nullptr)) {}
    Value &operator=(Value &&other) noexcept {
        if (this != &other) {
            sc_value_free(node_);
            node_ = std::exchange(other.node_, nullptr);
        }
        return *this;
    }
    ~Value() { sc_value_free(node_); }

    // ── Builders ─────────────────────────────────────────────────────────
    static Value null() { return adopt(sc_value_null()); }
    static Value boolean(bool value) { return adopt(sc_value_bool(value)); }
    static Value integer(std::int64_t value) {
        return adopt(sc_value_int(value));
    }
    static Value number(double value) { return adopt(sc_value_float(value)); }
    static Value string(std::string_view value) {
        return adopt(sc_value_string(std::string(value).c_str()));
    }
    static Value array() { return adopt(sc_value_array()); }
    static Value object() { return adopt(sc_value_object()); }

    /** Adopts a raw C node (ownership transfers in); throws on `nullptr`. */
    static Value adopt(ScValue *raw) {
        if (!raw) { throw std::bad_alloc(); }
        return Value(raw);
    }

    // ── Mutation (the child is moved in and owned by this container) ──────
    void push(Value child) {
        ScValue *raw = child.release();
        if (!sc_value_push(node_, raw)) {
            sc_value_free(raw);
            throw std::bad_alloc();
        }
    }
    void set(std::string_view key, Value child) {
        ScValue *raw = child.release();
        if (!sc_value_set(node_, std::string(key).c_str(), raw)) {
            sc_value_free(raw);
            throw std::bad_alloc();
        }
    }

    // ── Inspection (forwarded to a View) ─────────────────────────────────
    [[nodiscard]] View view() const noexcept { return View(node_); }
    [[nodiscard]] Type type() const noexcept { return sc_value_type(node_); }
    [[nodiscard]] std::size_t size() const noexcept {
        return sc_value_len(node_);
    }

    /** Borrowed C handle (escape hatch); ownership stays with this object. */
    [[nodiscard]] const ScValue *get() const noexcept { return node_; }

    /** Releases ownership of the C handle to the caller. */
    [[nodiscard]] ScValue *release() noexcept {
        return std::exchange(node_, nullptr);
    }

private:
    explicit Value(ScValue *node) noexcept : node_(node) {}

    ScValue *node_;
};


namespace json {

/** Parses a JSON document; throws `ParseError` on malformed input. */
[[nodiscard]] inline Value parse(std::string_view src) {
    ScParseError error{};
    ScValue *root = sc_json_parse(src.data(), src.size(), &error);
    if (!root) { throw ParseError(error); }
    return Value::adopt(root);
}

/** Serializes a tree view to a JSON string. */
[[nodiscard]] inline std::string write(View value, JsonWriteOpts opts = {}) {
    char *out = sc_json_write(value.get(), opts);
    if (!out) { throw std::bad_alloc(); }
    std::string result(out);
    std::free(out);
    return result;
}

/** Serializes an owned tree to a JSON string. */
[[nodiscard]] inline std::string write(
    const Value &value, JsonWriteOpts opts = {}
) {
    return write(value.view(), opts);
}

}  // namespace json

}  // namespace sparcli::serde
