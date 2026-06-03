// fuzzy.cpp - fuzzy finder over a list or a table (C++ wrapper).
//
// Run (after `make`) in a real terminal:
//   make run-example EX=cpp/input/fuzzy

#include <sparcli.hpp>

#include <print>
#include <string>
#include <vector>

using namespace sparcli;


static void run_list_finder();
static void run_table_finder();
static void show_pure_matcher();


int main() {
    if (!input_available()) {
        alert::warning("Run this example in a real terminal "
                       "(not under a pipe).");
        show_pure_matcher();   // the scorer is pure and needs no TTY
        return 0;
    }

    run_list_finder();
    run_table_finder();
    show_pure_matcher();
    return 0;
}

// List mode: one label per entry; the returned index is the add order.
static void run_list_finder() {
    const std::vector<std::string> branches = {
        "main", "develop", "feature/fuzzy-finder", "feature/live-display",
        "fix/table-wrap", "release/1.2",
    };

    Fuzzy finder({ .prompt = "Switch branch", .accent = cyan() });
    for (const auto& branch : branches) {
        finder.add(branch);
    }

    if (auto picked = finder.run()) {
        std::println("  -> {}", branches[*picked]);
    }
}

// Table mode: the query searches and highlights across all columns.
static void run_table_finder() {
    static const char* headers[] = { "Host", "Region", "Status" };

    Fuzzy finder({ .prompt = "Pick a host", .accent = blue(),
                   .table = true, .headers = headers, .n_cols = 3,
                   .table_opts = {
                       .border = { .type = SC_BORDER_ROUNDED },
                       .header = { .row = true,
                                   .style = style(SC_TEXT_ATTR_BOLD) } } });
    finder.add_row({ "web-01", "eu-central", "healthy" });
    finder.add_row({ "web-02", "eu-central", "degraded" });
    finder.add_row({ "db-01", "us-east", "healthy" });
    finder.add_row({ "cache-01", "ap-south", "healthy" });

    if (auto picked = finder.run()) {
        std::println("  -> row {}", *picked);
    }
}

// fuzzy_match is the pure scoring function behind the widget.
static void show_pure_matcher() {
    int score = 0;
    bool hit = fuzzy_match("ffind", "feature/fuzzy-finder", &score);
    std::println("  match \"ffind\" in \"feature/fuzzy-finder\": {} (score {})",
                 hit ? "yes" : "no", score);
}
