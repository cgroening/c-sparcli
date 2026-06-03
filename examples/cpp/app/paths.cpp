// paths.cpp - per-application XDG directories and files (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/app/paths

#include <sparcli.hpp>

#include <print>
#include <string>

using namespace sparcli;


// Application name; becomes the directory under each XDG base dir.
constexpr const char* kAppName = "sparcli-paths-example";


int main() {
    // Each returns std::optional<std::string>; empty on failure. The dirs
    // are created (mode 0700) on first use.
    auto config_dir = paths::config(kAppName);
    auto data_dir   = paths::data(kAppName);
    auto cache_dir  = paths::cache(kAppName);
    auto state_dir  = paths::state(kAppName);

    if (!config_dir || !data_dir || !cache_dir || !state_dir) {
        alert::error("Could not resolve the XDG directories.");
        return 1;
    }

    Kv dirs({ .key_style = style(SC_TEXT_ATTR_BOLD, cyan()) });
    dirs.add("Config", *config_dir);
    dirs.add("Data", *data_dir);
    dirs.add("Cache", *cache_dir);
    dirs.add("State", *state_dir);
    dirs.print();
    println("");

    // A file path inside an app dir: parents are created, the file is not.
    auto log_file = paths::file(SC_PATH_STATE, kAppName, "logs/run.log");
    if (log_file) {
        std::println("Log file would go to: {}", *log_file);
    }

    return 0;
}
