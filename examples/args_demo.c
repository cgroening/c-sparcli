/**
 * args_demo.c - the argument parser as the backbone of a small CLI tool.
 *
 * Build and run:
 *   make run-example EX=args_demo
 *   ./build.examples.nosync/args_demo --help
 *   ./build.examples.nosync/args_demo build --jobs 8 --mode release app
 *   ./build.examples.nosync/args_demo buidl        (did-you-mean)
 *   ./build.examples.nosync/args_demo completion   (zsh completion script)
 */
#include <sparcli.h>

#include <stdio.h>
#include <string.h>


// Forward declarations indented to reflect call hierarchy
static ScArgs *build_parser(void);
static int run_build(ScArgs *args);
static int run_clean(ScArgs *args);


int main(int argc, char **argv) {
    ScArgs *args = build_parser();
    if (!args) {
        sc_die_msg(1, "Out of memory", NULL);
    }

    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
    if (status != SC_ARGS_MATCHED) {
        sc_args_free(args);
        return status == SC_ARGS_HANDLED ? 0 : 2;
    }

    const char *command = sc_args_cmd_name(matched);
    int exit_code = 0;
    if (strcmp(command, "build") == 0) {
        exit_code = run_build(args);
    } else if (strcmp(command, "clean") == 0) {
        exit_code = run_clean(args);
    } else if (strcmp(command, "completion") == 0) {
        sc_args_print_zsh_completion(args);
    } else {
        // No subcommand: show the help screen
        sc_args_print_help(args, NULL);
    }

    sc_args_free(args);
    return exit_code;
}

/** Declares the whole command-line interface of the tool. */
static ScArgs *build_parser(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog = "args_demo",
        .version = "1.0.0",
        .about = "Demo of the sparcli argument parser",
    });
    if (!args) { return NULL; }

    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Explain what is being done");

    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build a target");
    sc_args_cmd_group(build, "Work commands");
    sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
    sc_args_opt_default(build, "jobs", "4");
    sc_args_opt(build, "mode", 'm', SC_ARG_STR, "MODE", "Build mode");
    sc_args_opt_choices(
        build, "mode", (const char *[]){ "debug", "release", NULL }
    );
    sc_args_opt_default(build, "mode", "debug");
    sc_args_opt(build, "accent", 0, SC_ARG_COLOR, "COLOR", "Accent color");
    sc_args_positional(
        build, "TARGET", SC_ARG_STR, "What to build", true, false
    );
    sc_args_positional(
        build, "EXTRA", SC_ARG_STR, "Additional inputs", false, true
    );

    ScArgsCmd *clean = sc_args_subcommand(root, "clean", "Remove artifacts");
    sc_args_cmd_group(clean, "Work commands");
    sc_args_flag(clean, "force", 'f', "Do not ask for confirmation");

    sc_args_subcommand(
        root, "completion", "Print the zsh completion script"
    );

    return args;
}

/** The "build" command: shows the parsed values in a key-value block. */
static int run_build(ScArgs *args) {
    ScColor accent = sc_args_get_color(args, "accent");

    sc_rule_str("build", (ScRuleOpts){
        .type = SC_BORDER_SINGLE,
        .color = accent,
        .title.style = { SC_TEXT_ATTR_BOLD, accent, SC_ANSI_COLOR_NONE },
    });

    ScKV *summary = sc_kv_new((ScKVOpts){
        .key_style = { SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE,
                       SC_ANSI_COLOR_NONE },
    });
    sc_kv_add(summary, "Target", sc_args_get_str(args, "TARGET"));
    sc_kv_add(summary, "Mode", sc_args_get_str(args, "mode"));

    char jobs[32];
    snprintf(jobs, sizeof jobs, "%ld", sc_args_get_int(args, "jobs"));
    sc_kv_add(summary, "Jobs", jobs);

    size_t extra_count = 0;
    sc_args_get_many(args, "EXTRA", &extra_count);
    char extra[32];
    snprintf(extra, sizeof extra, "%zu file(s)", extra_count);
    sc_kv_add(summary, "Extra", extra);

    sc_kv_add(
        summary, "Verbose",
        sc_args_get_flag(args, "verbose") ? "yes" : "no"
    );
    sc_kv_print(summary);
    sc_kv_free(summary);

    sc_alert_success("Build finished.");
    return 0;
}

/** The "clean" command. */
static int run_clean(ScArgs *args) {
    if (!sc_args_get_flag(args, "force")) {
        sc_alert_warning("Pass --force to actually delete artifacts.");
        return 1;
    }
    sc_alert_success("Artifacts removed.");
    return 0;
}
