// config_layer.cpp - layered configuration (defaults < env < flags).
//
// sparcli::Config is opt-in (serde-dependent): include <app/sparcli_config.hpp>.
//
// Run (after `make`):
//   MYAPP_SERVER__PORT=9090 make run-example EX=cpp/app/config_layer

#include <app/sparcli_config.hpp>

#include <print>
#include <utility>

using namespace sparcli;


int main() {
    Config cfg;

    // Defaults.
    serde::Value defaults = serde::Value::object();
    serde::Value server = serde::Value::object();
    server.set("host", serde::Value::string("127.0.0.1"));
    server.set("port", serde::Value::integer(8080));
    defaults.set("server", std::move(server));
    defaults.set("debug", serde::Value::boolean(false));
    cfg.set_defaults(defaults);

    // Environment (MYAPP_SERVER__PORT overrides server.port).
    cfg.load_env("MYAPP_");

    // Explicit overrides (e.g. parsed CLI flags).
    cfg.set("debug", serde::Value::boolean(true));

    std::println("server.host = {}", cfg.get_string("server.host", "?"));
    std::println("server.port = {}", cfg.get_int("server.port", 0));
    std::println("debug       = {}", cfg.get_bool("debug", false));
    return 0;
}
