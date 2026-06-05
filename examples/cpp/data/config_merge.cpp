// config_merge.cpp - the ScValue ergonomics in the C++ wrapper: deep-merge
// defaults with a user overlay, read via a dotted path, drop a key, and
// round-trip the result through a file.
//
// Run (after `make`):
//   make run-example EX=cpp/data/config_merge

#include <serde/sparcli_serde.hpp>

#include <cstdint>
#include <cstdlib>
#include <print>
#include <string_view>

#include <unistd.h>

using namespace sparcli::serde;

int main() {
    Value cfg = json::parse(
        R"({"host":"localhost","port":8080,)"
        R"("tls":{"enabled":false,"verify":true},"debug":true})");
    Value overlay = json::parse(R"({"port":443,"tls":{"enabled":true}})");

    cfg.merge(overlay);   // deep overlay: tls.verify is preserved
    cfg.remove("debug");

    auto host = cfg.path("host").as_string().value_or("?");
    auto port = cfg.path("port").as_int().value_or(80);
    bool tls = cfg.path("tls.enabled").as_bool().value_or(false);
    bool verify = cfg.path("tls.verify").as_bool().value_or(false);
    std::println("host={} port={} tls.enabled={} tls.verify(kept)={}",
                 host, port, tls, verify);

    // Persist it and read it straight back to show the file helpers.
    char path[] = "/tmp/sparcli_config_cpp_XXXXXX";
    int fd = mkstemp(path);
    if (fd >= 0) {
        close(fd);
        json::write_file(cfg, path, { .indent = 2 });
        Value reloaded = json::parse_file(path);
        std::println("\nReloaded from file:\n{}",
                     json::write(reloaded, { .indent = 2 }));
        unlink(path);
    }
    return 0;
}
