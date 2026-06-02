#include "test_args.h"

#include <unistd.h>


int g_args_failures = 0;

typedef struct Test {
    char name[40];
    void (*function)(void);
} Test;


/** Prints a separator rule with an optional bold title. */
static void print_rule(const char *title) {
    sc_rule_str(
        title,
        (ScRuleOpts){
            .type = SC_BORDER_DOUBLE,
            .color = SC_ANSI_COLOR_NONE,
            .title.style = {
                SC_TEXT_ATTR_BOLD, SC_ANSI_COLOR_NONE, SC_ANSI_COLOR_NONE
            },
            .title.halign = SC_ALIGN_CENTER,
        }
    );
}

int main(void) {
    static const Test tests[] = {
        { "Parsing & Getters", test_args_parse  },
        { "Errors & Did-you-mean", test_args_errors },
        { "Help & Completion", test_args_help   },
    };
    size_t count = sizeof tests / sizeof tests[0];

    printf("\n");
    for (size_t i = 0; i < count; i++) {
        print_rule(tests[i].name);
        tests[i].function();
        print_rule(NULL);
        printf("\n");
    }

    if (g_args_failures > 0) {
        printf("\033[31m%d args check(s) failed.\033[0m\n", g_args_failures);
        return 1;
    }
    printf("\033[32mAll args checks passed.\033[0m\n");
    return 0;
}


/* ── Shared helpers ──────────────────────────────────────────────────────── */

ScArgs *test_args_build_demo(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){
        .prog = "demo",
        .version = "1.2.3",
        .about = "A demo tool for the test suite",
    });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_flag(root, "verbose", 'v', "Verbose output");
    sc_args_flag(root, "quiet", 'q', "Silence everything");
    sc_args_opt(root, "color", 'c', SC_ARG_COLOR, "COLOR", "Accent color");

    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build the project");
    sc_args_cmd_group(build, "Work commands");
    sc_args_opt(build, "jobs", 'j', SC_ARG_INT, "N", "Parallel jobs");
    sc_args_opt_default(build, "jobs", "4");
    sc_args_opt(build, "mode", 'm', SC_ARG_STR, "MODE", "Build mode");
    sc_args_opt_choices(
        build, "mode", (const char *[]){ "debug", "release", NULL }
    );
    sc_args_opt(build, "scale", 0, SC_ARG_DOUBLE, "X", "Scale factor");
    sc_args_positional(build, "TARGET", SC_ARG_STR, "Build target", true, false);
    sc_args_positional(build, "EXTRA", SC_ARG_STR, "Extra inputs", false, true);

    ScArgsCmd *clean = sc_args_subcommand(root, "clean", "Remove artifacts");
    sc_args_cmd_group(clean, "Work commands");
    sc_args_flag(clean, "force", 'f', "Skip confirmation");

    ScArgsCmd *remote = sc_args_subcommand(root, "remote", "Manage remotes");
    sc_args_subcommand(remote, "add", "Add a remote");

    return args;
}

char *test_args_parse_capture_stderr(
    ScArgs *args, int argc, char **argv,
    ScArgsStatus *status, const ScArgsCmd **matched
) {
    FILE *stderr_sink = tmpfile();
    fflush(stderr);
    int saved_stderr = dup(fileno(stderr));
    bool redirected = stderr_sink && saved_stderr >= 0
        && dup2(fileno(stderr_sink), fileno(stderr)) >= 0;

    const ScArgsCmd *result = sc_args_parse(args, argc, argv, status);
    if (matched) { *matched = result; }

    if (redirected) {
        fflush(stderr);
        dup2(saved_stderr, fileno(stderr));
    }
    if (saved_stderr >= 0) { close(saved_stderr); }

    char *captured = calloc(1, 16384);
    if (captured && stderr_sink && fseek(stderr_sink, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(captured, 1, 16383, stderr_sink);
        captured[read_bytes] = '\0';
    }
    if (stderr_sink) { fclose(stderr_sink); }
    return captured;
}

char *test_args_capture_output(void (*render)(ScArgs *args), ScArgs *args) {
    FILE *capture = tmpfile();
    if (!capture) { return NULL; }

    FILE *previous = sc_output_stream();
    sc_output_set_stream(capture);
    render(args);
    sc_output_set_stream(previous);

    char *buffer = calloc(1, 32768);
    if (buffer && fseek(capture, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(buffer, 1, 32767, capture);
        buffer[read_bytes] = '\0';
    }
    fclose(capture);
    return buffer;
}

bool test_args_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}
