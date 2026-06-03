// confirm_select.cpp - confirmation and single/multi selection (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/confirm_select
//
// Each prompt returns std::optional / std::vector; empty means cancelled or
// no interactive terminal.

#include <sparcli.hpp>

#include <print>
#include <string>
#include <vector>

using namespace sparcli;


static const std::vector<std::string> kLanguages = {
    "C", "C++", "Rust", "Python", "Zig", "Go",
};


static void run_confirm();
static void run_single_select();
static void run_multi_select();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        return 0;
    }

    run_confirm();
    run_single_select();
    run_multi_select();
    return 0;
}

// Yes/no with custom labels and a default answer.
static void run_confirm() {
    if (auto deploy = confirm("Deploy to production?",
                              { .default_yes = false,
                                .yes_label = "Ship it",
                                .no_label = "Not yet",
                                .accent = green() })) {
        markup::println(*deploy ? "  [green]-> deploying[/]"
                                : "  [yellow]-> skipped[/]");
    }
}

// Single selection: run_one() returns the chosen index.
static void run_single_select() {
    Select select({ .prompt = "Primary language", .accent = cyan() });
    for (const auto& language : kLanguages) {
        select.add(language);
    }

    if (auto picked = select.run_one()) {
        std::println("  -> {}", kLanguages[*picked]);
    }
}

// Multi selection: run() returns the indices in display order.
static void run_multi_select() {
    Select select({ .prompt = "Languages you use (Space toggles)",
                    .multi = true, .max_visible = 5, .accent = magenta() });
    for (const auto& language : kLanguages) {
        select.add(language);
    }
    select.set_checked(0);
    select.set_cursor(1);

    if (auto picked = select.run()) {
        std::print("  -> {} selected:", picked->size());
        for (size_t index : *picked) {
            std::print(" {}", kLanguages[index]);
        }
        std::println("");
    }
}
