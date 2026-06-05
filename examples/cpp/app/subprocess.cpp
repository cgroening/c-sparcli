// subprocess.cpp - run a command (no shell) and capture its output.
//
// Run (after `make`):
//   make run-example EX=cpp/app/subprocess

#include <sparcli.hpp>

#include <print>
#include <string>

using namespace sparcli;


int main() {
    ProcResult r = run({ "uname", "-sm" });
    std::println("uname -sm → exit {}: {}", r.exit_code(),
                 std::string(r.out()));

    // Feed stdin and capture the filtered result.
    std::string text = "one two three four five\n";
    ProcOpts opts;
    opts.input = text.c_str();
    opts.input_len = text.size();
    ProcResult words = run({ "wc", "-w" }, opts);
    std::println("wc -w     → {}", std::string(words.out()));

    ProcResult miss = run({ "definitely-not-a-real-binary" });
    std::println("missing   → exit {}", miss.exit_code());
    return 0;
}
