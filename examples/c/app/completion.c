/*
 * completion.c - generate shell completion scripts from an args command tree.
 *
 * The argument parser can emit a completion script for zsh, bash or fish,
 * covering the root options, the first level of subcommands and their
 * options/positionals (value options complete their declared choices).
 *
 * Run (after `make`):
 *   make run-example EX=c/app/completion              # all three, with headers
 *   make run-example EX=c/app/completion ARGS=zsh     # just zsh
 *   make run-example EX=c/app/completion ARGS=bash    # just bash
 *   make run-example EX=c/app/completion ARGS=fish    # just fish
 *
 * A real tool wires this to a hidden `completion <shell>` subcommand:
 *   mytool completion zsh  > ~/.zsh/completions/_mytool
 *   mytool completion bash > /etc/bash_completion.d/mytool
 *   mytool completion fish > ~/.config/fish/completions/mytool.fish
 */

#include <sparcli.h>

#include <stdio.h>
#include <string.h>


/* Builds a small but representative command tree. */
static ScArgs *build_parser(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog = "demo", .version = "1.0", .about = "A demo tool",
    });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Verbose output");

    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build the project");
    sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
    sc_args_opt(build, "mode", 'm', SC_ARG_STR, "MODE", "Build mode");
    sc_args_opt_choices(build, "mode",
                        (const char *[]){ "debug", "release", NULL });
    sc_args_positional(build, "TARGET", SC_ARG_STR, "Build target", true, false);

    return args;
}


int main(int argc, char **argv) {
    ScArgs *args = build_parser();
    if (!args) { return 1; }

    const char *shell = argc > 1 ? argv[1] : NULL;

    if (shell && strcmp(shell, "zsh") == 0) {
        sc_args_print_zsh_completion(args);
    } else if (shell && strcmp(shell, "bash") == 0) {
        sc_args_print_bash_completion(args);
    } else if (shell && strcmp(shell, "fish") == 0) {
        sc_args_print_fish_completion(args);
    } else {
        printf("# ===== zsh =====\n");
        sc_args_print_zsh_completion(args);
        printf("\n# ===== bash =====\n");
        sc_args_print_bash_completion(args);
        printf("\n# ===== fish =====\n");
        sc_args_print_fish_completion(args);
    }

    sc_args_free(args);
    return 0;
}
