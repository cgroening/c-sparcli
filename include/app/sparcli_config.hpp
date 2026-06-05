#pragma once

/**
 * @file sparcli_config.hpp
 * @brief C++ wrapper for the layered config helper (`app/sparcli_config.h`).
 *
 * Like the C header (and serde), this is **opt-in**: it depends on the serde
 * `ScValue` model, so it is not pulled in by `<sparcli.hpp>`. Include it
 * explicitly. Requires C++20.
 */

#include "app/sparcli_config.h"
#include "serde/sparcli_serde.hpp"

#include <cstdint>
#include <new>
#include <string>
#include <string_view>


namespace sparcli {

using ConfigStatus = ScConfigStatus;

/**
 * Owning, move-only layered configuration (wraps `ScConfig`): defaults < file
 * < env < flags, deep-merged. @see sparcli_config.h
 */
class Config {
public:
    Config() : c_(sc_config_new()) { if (!c_) { throw std::bad_alloc(); } }
    ~Config() { sc_config_free(c_); }
    Config(Config&& o) noexcept : c_(o.c_) { o.c_ = nullptr; }
    Config& operator=(Config&& o) noexcept {
        if (this != &o) { sc_config_free(c_); c_ = o.c_; o.c_ = nullptr; }
        return *this;
    }
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;

    /** Merges a defaults object (base layer). @see sc_config_set_defaults */
    bool set_defaults(const serde::Value& defaults) {
        return sc_config_set_defaults(c_, defaults.get());
    }
    /** Loads + merges a config file (format by extension).
        @see sc_config_load_file */
    ConfigStatus load_file(std::string_view path, ScParseError* err = nullptr) {
        return sc_config_load_file(c_, std::string(path).c_str(), err);
    }
    /** Merges env variables under `prefix` (`__` = nesting).
        @see sc_config_load_env */
    void load_env(std::string_view prefix) {
        sc_config_load_env(c_, std::string(prefix).c_str());
    }
    /** Merges an explicit overlay (highest precedence). @see sc_config_merge */
    bool merge(const serde::Value& overlay) {
        return sc_config_merge(c_, overlay.get());
    }
    /** Sets a value at a dotted path; **takes ownership** of `value`.
        @see sc_config_set */
    bool set(std::string_view path, serde::Value value) {
        return sc_config_set(c_, std::string(path).c_str(), value.release());
    }

    /** The merged tree (non-owning view). @see sc_config_root */
    serde::View root() const { return serde::View(sc_config_root(c_)); }
    /** Resolves a dotted path (non-owning view). @see sc_config_get */
    serde::View get(std::string_view path) const {
        return serde::View(sc_config_get(c_, std::string(path).c_str()));
    }

    bool get_bool(std::string_view path, bool fallback = false) const {
        return sc_config_get_bool(c_, std::string(path).c_str(), fallback);
    }
    std::int64_t get_int(std::string_view path,
                         std::int64_t fallback = 0) const {
        return sc_config_get_int(c_, std::string(path).c_str(), fallback);
    }
    double get_float(std::string_view path, double fallback = 0) const {
        return sc_config_get_float(c_, std::string(path).c_str(), fallback);
    }
    std::string get_string(std::string_view path,
                           std::string_view fallback = {}) const {
        const char* s =
            sc_config_get_string(c_, std::string(path).c_str(), nullptr);
        return s ? std::string(s) : std::string(fallback);
    }

    /** Borrowed underlying `ScConfig*` (escape hatch); not owned. */
    ScConfig* get() { return c_; }

private:
    ScConfig* c_;
};

}  // namespace sparcli
