// args_repl.cpp - reuse one parser tree per input line: parse_line()
// tokenizes and parses in one call (C++ wrapper).
//
// Run (after `make`):
//   make run-example EX=cpp/app/args_repl
//
// This example feeds fixed lines so it runs anywhere. For a fully
// interactive REPL see examples/c/apps/repl_demo.c.

#include <sparcli.hpp>

#include <array>
#include <print>
#include <string_view>

using namespace sparcli;


static Args build_parser();
static void run_line(Args& args, std::string_view line);


int main() {
    Args args = build_parser();

    // Each line goes through the same tree; parse implicitly resets the
    // previous results, so no state leaks between lines.
    constexpr std::array lines = {
        std::string_view("add \"buy milk\" --priority high"),
        std::string_view("add 'read the docs'"),
        std::string_view("list --done"),
        std::string_view("add"),   // error: missing positional
        std::string_view("lst"),   // error with did-you-mean
    };

    for (std::string_view line : lines) {
        run_line(args, line);
    }
    return 0;
}

// A small task-manager command tree.
static Args build_parser() {
    Args args({ .prog = "tasks", .about = "REPL-style task manager" });

    auto add = args.root().subcommand("add", "Add a task");
    add.positional("TITLE", SC_ARG_STR, "Task title", true)
       .opt("priority", 'p', SC_ARG_STR, "LEVEL", "Priority")
       .opt_choices("priority", { "low", "normal", "high" })
       .opt_default("priority", "normal");

    args.root().subcommand("list", "List tasks")
        .flag("done", 'd', "Include finished tasks");

    return args;
}

// parse_line() tokenizes (quotes/escapes) and parses one line in one call.
static void run_line(Args& args, std::string_view line) {
    markup::print("[bold cyan]tasks>[/] ");
    std::println("{}", line);
    // Parse errors go to stderr; flush so the order stays intact when piped.
    std::fflush(stdout);

    auto matched = args.parse_line(line);
    if (!matched) {
        return;   // tokenizer or parse error already reported
    }

    const std::string command = matched->name();
    if (command == "add") {
        std::println("  added \"{}\" (priority: {})",
                     args.get_str("TITLE").value_or(""),
                     args.get_str("priority").value_or(""));
    } else if (command == "list") {
        std::println("  listing tasks (done included: {})",
                     args.get_flag("done") ? "yes" : "no");
    }
}
