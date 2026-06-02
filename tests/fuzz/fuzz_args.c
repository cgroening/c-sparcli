#include "sparcli.h"
#include "fuzz_common.h"

#include <stdio.h>
#include <string.h>

/**
 * Fuzz target for the argument parser: random argv vectors against a fixed
 * command tree must never crash, leak, or trigger UB. argv is the untrusted
 * input surface of every CLI application built on this module.
 */

/** Maximum number of argv tokens carved out of one fuzz input. */
#define FUZZ_MAX_ARGV 32

static bool g_stderr_silenced = false;

/** Builds the fixed command tree the fuzz inputs are parsed against. */
static ScArgs *build_tree(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog = "fuzz", .version = "1.0", .about = "fuzz target",
    });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Verbose");
    sc_args_opt(root, "color", 'c', SC_ARG_COLOR, "COLOR", "Color");

    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build");
    sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Jobs");
    sc_args_opt(build, "mode", 'm', SC_ARG_STR, "MODE", "Mode");
    sc_args_opt_choices(
        build, "mode", (const char *[]){ "debug", "release", NULL }
    );
    sc_args_opt(build, "scale", 's', SC_ARG_DOUBLE, "X", "Scale");
    sc_args_positional(build, "TARGET", SC_ARG_STR, "Target", true, false);
    sc_args_positional(build, "EXTRA", SC_ARG_STR, "Extra", false, true);

    sc_args_subcommand(root, "clean", "Clean");
    return args;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    // Parse errors render panels to stderr; silence them once for speed
    if (!g_stderr_silenced) {
        freopen("/dev/null", "w", stderr);
        g_stderr_silenced = true;
    }

    // Carve the input into argv tokens (split on NUL and whitespace)
    char *buffer = malloc(size + 1);
    if (!buffer) { return 0; }
    memcpy(buffer, data, size);
    buffer[size] = '\0';

    char *argv[FUZZ_MAX_ARGV];
    int argc = 0;
    argv[argc++] = "fuzz";   // argv[0] = program name

    char *cursor = buffer;
    char *end = buffer + size;
    while (cursor < end && argc < FUZZ_MAX_ARGV) {
        // Skip separators (NUL bytes and whitespace)
        while (cursor < end
               && (*cursor == '\0' || *cursor == ' ' || *cursor == '\n')) {
            cursor++;
        }
        if (cursor >= end) { break; }
        argv[argc++] = cursor;
        while (cursor < end && *cursor != '\0' && *cursor != ' '
               && *cursor != '\n') {
            cursor++;
        }
        if (cursor < end) { *cursor++ = '\0'; }
    }

    // Help/version output goes to the output stream: capture and discard
    FILE *sink = tmpfile();
    FILE *previous = sc_output_stream();
    if (sink) { sc_output_set_stream(sink); }

    ScArgs *args = build_tree();
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);

    // Exercise the getters regardless of the outcome
    if (matched) {
        sc_args_get_flag(args, "verbose");
        sc_args_get_int(args, "jobs");
        sc_args_get_double(args, "scale");
        sc_args_get_color(args, "color");
        sc_args_get_enum(args, "mode");
        sc_args_get_str(args, "TARGET");
        size_t count = 0;
        sc_args_get_many(args, "EXTRA", &count);
        sc_args_selected_command(args);
    }
    sc_args_free(args);

    sc_output_set_stream(previous);
    if (sink) { fclose(sink); }
    free(buffer);
    return 0;
}
