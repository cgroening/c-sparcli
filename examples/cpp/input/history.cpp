// history.cpp - input history with Up/Down recall and persistence
// (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/history
//
// The History is move-only RAII; its destructor saves the file.

#include <sparcli.hpp>

#include <print>

using namespace sparcli;


// Application name used for the XDG state file.
constexpr const char* kAppName = "sparcli-history-example";


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    // .app -> ~/.local/state/<app>/history; loaded on construction.
    History history({ .max_entries = 100, .app = kAppName });

    if (history.count() > 0) {
        std::println("Loaded {} line(s) from the previous run; "
                     "press Up to recall them.", history.count());
    }

    // apply() wires the history into the opts; submitted lines are added
    // automatically (empty lines and consecutive duplicates skipped).
    for (int round = 0; round < 3; ++round) {
        TextInputOpts opts{ .placeholder = "type, or press Up" };
        history.apply(opts);
        auto line = text_input(">", opts);
        if (!line) {
            break;
        }
    }

    std::println("\nHistory contents:");
    for (std::size_t i = 0; i < history.count(); ++i) {
        std::println("  {:2}: {}", i, history.get(i).value_or(""));
    }

    // Destructor saves to the state file.
    return 0;
}
