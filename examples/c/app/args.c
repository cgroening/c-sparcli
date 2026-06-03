/*
 * args.c - declarative argument parser: subcommands, typed options,
 * help/version handling and error reporting.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/args
 * then try it directly:
 *   ./build.examples.nosync/c/app/args --help
 *   ./build.examples.nosync/c/app/args convert in.png --format webp -q 90
 *   ./build.examples.nosync/c/app/args convrt          (did-you-mean)
 *   ./build.examples.nosync/c/app/args completion      (zsh completion)
 */

#include <sparcli.h>

#include <stdio.h>
#include <string.h>


static ScArgs *build_parser(void);
static int run_convert(const ScArgs *args);


int main(int argc, char **argv) {
    ScArgs *args = build_parser();
    if (!args) {
        sc_die_msg(1, "Out of memory", NULL);
    }

    // parse() handles --help/--version itself (HANDLED) and reports bad
    // input as a pretty error + exit code 2 suggestion (ERROR).
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
    if (status != SC_ARGS_MATCHED) {
        sc_args_free(args);
        return status == SC_ARGS_HANDLED ? 0 : 2;
    }

    const char *command = sc_args_cmd_name(matched);
    int exit_code = 0;
    if (strcmp(command, "convert") == 0) {
        exit_code = run_convert(args);
    } else if (strcmp(command, "completion") == 0) {
        sc_args_print_zsh_completion(args);
    } else {
        // Bare invocation without a subcommand: show the help screen.
        sc_args_print_help(args, NULL);
    }

    sc_args_free(args);
    return exit_code;
}

/** Declares the whole command-line interface. */
static ScArgs *build_parser(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog    = "imgtool",
        .version = "1.0.0",
        .about   = "Example image conversion tool",
    });
    if (!args) {
        return NULL;
    }

    // Global flag, available on every subcommand.
    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Explain what is being done");

    // "convert" subcommand with typed options and positionals.
    ScArgsCmd *convert = sc_args_subcommand(root, "convert",
                                            "Convert an image file");
    sc_args_opt(convert, "format", 'f', SC_ARG_STR, "FMT", "Output format");
    sc_args_opt_choices(convert, "format",
                        (const char *[]){ "png", "jpeg", "webp", NULL });
    sc_args_opt_default(convert, "format", "png");

    sc_args_opt(convert, "quality", 'q', SC_ARG_INT, "N", "Quality (0-100)");
    sc_args_opt_default(convert, "quality", "80");

    sc_args_opt(convert, "tint", 0, SC_ARG_COLOR, "COLOR",
                "Tint color (name or #rrggbb)");

    // required = true, variadic = false
    sc_args_positional(convert, "INPUT", SC_ARG_STR, "Input file",
                       true, false);

    sc_args_subcommand(root, "completion",
                       "Print the zsh completion script");
    return args;
}

/** The "convert" command: echo the parsed values. */
static int run_convert(const ScArgs *args) {
    // Getters search the matched command and its ancestors, so the global
    // --verbose flag is visible here too.
    printf("input:   %s\n", sc_args_get_str(args, "INPUT"));
    printf("format:  %s (choice index %d)\n",
           sc_args_get_str(args, "format"), sc_args_get_enum(args, "format"));
    printf("quality: %ld\n", sc_args_get_int(args, "quality"));
    printf("verbose: %s\n", sc_args_get_flag(args, "verbose") ? "yes" : "no");

    if (sc_args_present(args, "tint")) {
        ScColor tint = sc_args_get_color(args, "tint");
        sc_println("tint:    this color", (ScTextStyle){
            .attr = SC_TEXT_ATTR_BOLD, .fg = tint,
        });
    }

    sc_alert_success("Conversion finished (pretend).");
    return 0;
}
