// diff.cpp - colored unified diff + humanize helpers (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/output/diff

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


int main() {
    diff("alpha\nbeta\ngamma\n", "alpha\nBETA\ngamma\ndelta\n",
         DiffOpts{ .old_label = "before", .new_label = "after" });

    std::println("");
    std::println("downloaded {} in {} ({})",
                 humanize::bytes(15728640),        // "15.7 MB"
                 humanize::duration(8054),         // "2h 14m"
                 humanize::compact(15728640));     // "15.7M"
    return 0;
}
