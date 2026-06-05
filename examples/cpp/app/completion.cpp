// completion.cpp - generate shell completion scripts from an args tree.
//
// The parser emits a completion script for zsh, bash or fish (root options,
// first level of subcommands and their options; value options complete their
// declared choices).
//
// Run (after `make`):
//   make run-example EX=cpp/app/completion            # all three, with headers
//   make run-example EX=cpp/app/completion ARGS=zsh   # just zsh (bash/fish too)

#include <sparcli.hpp>

#include <cstring>
#include <print>

using namespace sparcli;


static Args build_parser() {
    Args args({ .prog = "demo", .version = "1.0", .about = "A demo tool" });
    args.root().flag("verbose", 'v', "Verbose output");
    args.root().subcommand("build", "Build the project")
        .opt("jobs", 'j', SC_ARG_INT, "N", "Parallel jobs")
        .opt("mode", 'm', SC_ARG_STR, "MODE", "Build mode")
        .opt_choices("mode", { "debug", "release" })
        .positional("TARGET", SC_ARG_STR, "Build target", true);
    return args;
}


int main(int argc, char** argv) {
    Args args = build_parser();
    const char* shell = argc > 1 ? argv[1] : nullptr;

    if (shell && std::strcmp(shell, "zsh") == 0) {
        args.print_zsh_completion();
    } else if (shell && std::strcmp(shell, "bash") == 0) {
        args.print_bash_completion();
    } else if (shell && std::strcmp(shell, "fish") == 0) {
        args.print_fish_completion();
    } else {
        std::println("# ===== zsh =====");
        args.print_zsh_completion();
        std::println("\n# ===== bash =====");
        args.print_bash_completion();
        std::println("\n# ===== fish =====");
        args.print_fish_completion();
    }
    return 0;
}
