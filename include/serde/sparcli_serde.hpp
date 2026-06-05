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
#include <vector>

namespace sparcli::serde {

/** Value discriminator, reusing the C enum verbatim. */
using Type = ScValueType;

/** Writer options, reusing the C struct verbatim. */
using JsonWriteOpts = ScJsonWriteOpts;

/** CSV dialect options, reusing the C struct verbatim. */
using CsvOpts = ScCsvOpts;

/** TOML writer options, reusing the C struct verbatim. */
using TomlWriteOpts = ScTomlWriteOpts;

/** YAML writer options, reusing the C struct verbatim. */
using YamlWriteOpts = ScYamlWriteOpts;

/** Markdown front-matter syntax, reusing the C enum verbatim. */
using MdFrontmatter = ScMdFrontmatter;


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


namespace toml {

/** Parses a TOML document; throws `ParseError` on malformed input. */
[[nodiscard]] inline Value parse(std::string_view src) {
    ScParseError error{};
    ScValue *root = sc_toml_parse(src.data(), src.size(), &error);
    if (!root) { throw ParseError(error); }
    return Value::adopt(root);
}

/** Serializes a tree view to a TOML string. */
[[nodiscard]] inline std::string write(View value, TomlWriteOpts opts = {}) {
    char *out = sc_toml_write(value.get(), opts);
    if (!out) { throw std::bad_alloc(); }
    std::string result(out);
    std::free(out);
    return result;
}

/** Serializes an owned tree to a TOML string. */
[[nodiscard]] inline std::string write(
    const Value &value, TomlWriteOpts opts = {}
) {
    return write(value.view(), opts);
}

}  // namespace toml


namespace yaml {

/** Parses a YAML (subset) document; throws `ParseError` on malformed input. */
[[nodiscard]] inline Value parse(std::string_view src) {
    ScParseError error{};
    ScValue *root = sc_yaml_parse(src.data(), src.size(), &error);
    if (!root) { throw ParseError(error); }
    return Value::adopt(root);
}

/** Serializes a tree view to YAML. */
[[nodiscard]] inline std::string write(View value, YamlWriteOpts opts = {}) {
    char *out = sc_yaml_write(value.get(), opts);
    if (!out) { throw std::bad_alloc(); }
    std::string result(out);
    std::free(out);
    return result;
}

/** Serializes an owned tree to YAML. */
[[nodiscard]] inline std::string write(
    const Value &value, YamlWriteOpts opts = {}
) {
    return write(value.view(), opts);
}

}  // namespace yaml


/** RAII owner of a CSV document (move-only); reader and writer in one type. */
class Csv {
public:
    Csv(const Csv &) = delete;
    Csv &operator=(const Csv &) = delete;

    Csv(Csv &&other) noexcept : doc_(std::exchange(other.doc_, nullptr)) {}
    Csv &operator=(Csv &&other) noexcept {
        if (this != &other) {
            sc_csv_free(doc_);
            doc_ = std::exchange(other.doc_, nullptr);
        }
        return *this;
    }
    ~Csv() { sc_csv_free(doc_); }

    /** Parses CSV/TSV text; throws `ParseError` on malformed input. */
    [[nodiscard]] static Csv parse(std::string_view src, CsvOpts opts = {}) {
        ScParseError error{};
        ScCsv *doc = sc_csv_parse(src.data(), src.size(), opts, &error);
        if (!doc) { throw ParseError(error); }
        return Csv(doc);
    }

    /** Creates an empty document for building output with `add_row`/`write`. */
    [[nodiscard]] static Csv create(CsvOpts opts = {}) {
        ScCsv *doc = sc_csv_new(opts);
        if (!doc) { throw std::bad_alloc(); }
        return Csv(doc);
    }

    // ── Reading ──────────────────────────────────────────────────────────
    [[nodiscard]] std::size_t rows() const noexcept { return sc_csv_rows(doc_); }
    [[nodiscard]] std::size_t cols() const noexcept { return sc_csv_cols(doc_); }
    [[nodiscard]] std::size_t row_cols(std::size_t row) const noexcept {
        return sc_csv_row_cols(doc_, row);
    }
    [[nodiscard]] std::string_view at(
        std::size_t row, std::size_t col
    ) const noexcept {
        return sc_csv_at(doc_, row, col);
    }
    [[nodiscard]] bool has_header() const noexcept {
        return sc_csv_has_header(doc_);
    }
    [[nodiscard]] std::string_view header(std::size_t col) const noexcept {
        return sc_csv_header(doc_, col);
    }
    [[nodiscard]] std::size_t data_rows() const noexcept {
        return sc_csv_data_rows(doc_);
    }
    [[nodiscard]] std::string_view get(
        std::size_t data_row, std::string_view name
    ) const {
        return sc_csv_get(doc_, data_row, std::string(name).c_str());
    }

    // ── Writing ──────────────────────────────────────────────────────────
    void add_row(const std::vector<std::string> &fields) {
        std::vector<const char *> raw;
        raw.reserve(fields.size());
        for (const std::string &field : fields) { raw.push_back(field.c_str()); }
        if (!sc_csv_add_row(doc_, raw.data(), raw.size())) {
            throw std::bad_alloc();
        }
    }
    [[nodiscard]] std::string write() const {
        char *out = sc_csv_write(doc_);
        if (!out) { throw std::bad_alloc(); }
        std::string result(out);
        std::free(out);
        return result;
    }

    /** Borrowed C handle (escape hatch); ownership stays with this object. */
    [[nodiscard]] const ScCsv *get() const noexcept { return doc_; }

private:
    explicit Csv(ScCsv *doc) noexcept : doc_(doc) {}

    ScCsv *doc_;
};


/**
 * Non-owning handle to a Markdown section. Valid while the owning `Markdown`
 * is alive; supports reading and in-place editing of the section tree.
 */
class Section {
public:
    explicit Section(ScMdSection *node) noexcept : node_(node) {}

    [[nodiscard]] int level() const noexcept {
        return sc_md_section_level(node_);
    }
    [[nodiscard]] std::string_view title() const noexcept {
        return sc_md_section_title(node_);
    }
    [[nodiscard]] std::string_view body() const noexcept {
        return sc_md_section_body(node_);
    }
    [[nodiscard]] std::size_t child_count() const noexcept {
        return sc_md_section_child_count(node_);
    }
    [[nodiscard]] Section child(std::size_t index) const noexcept {
        return Section(sc_md_section_child(node_, index));
    }
    /** Finds a descendant by `/`-separated heading path (null handle if not
     *  found). */
    [[nodiscard]] Section find(std::string_view path) const {
        return Section(sc_md_section_find(node_, std::string(path).c_str()));
    }

    void set_body(std::string_view text) {
        sc_md_section_set_body(node_, std::string(text).c_str());
    }
    /** Appends a child heading; throws `std::bad_alloc` on failure. */
    Section add(int level, std::string_view title) {
        ScMdSection *child =
            sc_md_section_add(node_, level, std::string(title).c_str());
        if (!child) { throw std::bad_alloc(); }
        return Section(child);
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return node_ != nullptr;
    }
    [[nodiscard]] ScMdSection *get() const noexcept { return node_; }

private:
    ScMdSection *node_;
};


/** RAII owner of a Markdown document (move-only): front matter + section tree. */
class Markdown {
public:
    Markdown(const Markdown &) = delete;
    Markdown &operator=(const Markdown &) = delete;

    Markdown(Markdown &&other) noexcept
        : doc_(std::exchange(other.doc_, nullptr)) {}
    Markdown &operator=(Markdown &&other) noexcept {
        if (this != &other) {
            sc_markdown_free(doc_);
            doc_ = std::exchange(other.doc_, nullptr);
        }
        return *this;
    }
    ~Markdown() { sc_markdown_free(doc_); }

    /** Parses a Markdown document; throws `ParseError` on allocation failure. */
    [[nodiscard]] static Markdown parse(std::string_view src) {
        ScParseError error{};
        ScMarkdown *doc = sc_markdown_parse(src.data(), src.size(), &error);
        if (!doc) { throw ParseError(error); }
        return Markdown(doc);
    }

    /** Creates an empty document for building output. */
    [[nodiscard]] static Markdown create() {
        ScMarkdown *doc = sc_markdown_new();
        if (!doc) { throw std::bad_alloc(); }
        return Markdown(doc);
    }

    [[nodiscard]] MdFrontmatter frontmatter_format() const noexcept {
        return sc_markdown_frontmatter_format(doc_);
    }
    [[nodiscard]] std::string_view frontmatter_raw() const noexcept {
        return sc_markdown_frontmatter_raw(doc_);
    }
    /** Parsed front matter as a borrowed `View` (empty when none/unparsed). */
    [[nodiscard]] std::optional<View> frontmatter() const noexcept {
        const ScValue *value = sc_markdown_frontmatter(doc_);
        if (value) { return View(value); }
        return std::nullopt;
    }
    [[nodiscard]] std::string_view body() const noexcept {
        return sc_markdown_body(doc_);
    }
    [[nodiscard]] Section root() noexcept {
        return Section(sc_markdown_root(doc_));
    }

    void set_frontmatter_raw(MdFrontmatter format, std::string_view raw) {
        sc_markdown_set_frontmatter_raw(
            doc_, format, std::string(raw).c_str()
        );
    }
    /** Sets the front matter from a value tree (serialized via the TOML/YAML
     *  writer); throws `std::bad_alloc` on failure. */
    void set_frontmatter(MdFrontmatter format, const Value &value) {
        if (!sc_markdown_set_frontmatter(doc_, format, value.get())) {
            throw std::bad_alloc();
        }
    }
    [[nodiscard]] std::string write() const {
        char *out = sc_markdown_write(doc_);
        if (!out) { throw std::bad_alloc(); }
        std::string result(out);
        std::free(out);
        return result;
    }

    [[nodiscard]] const ScMarkdown *get() const noexcept { return doc_; }

private:
    explicit Markdown(ScMarkdown *doc) noexcept : doc_(doc) {}

    ScMarkdown *doc_;
};

}  // namespace sparcli::serde
