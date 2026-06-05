#include "test_args.h"


// Forward declarations indented to reflect call hierarchy
static void check_root_help(void);
static void check_subcommand_help(void);
static void check_version(void);
static void check_completion(void);
static void check_completion_bash(void);
static void check_completion_fish(void);
    static void render_root_help(ScArgs *args);
    static void render_build_help(ScArgs *args);
    static void render_version_cb(ScArgs *args);
    static void render_completion(ScArgs *args);
    static void render_completion_bash(ScArgs *args);
    static void render_completion_fish(ScArgs *args);
    static char *capture_plain(void (*render)(ScArgs *), ScArgs *args);


/** Help screens, --version and the completion generators. */
void test_args_help(void) {
    check_root_help();
    check_subcommand_help();
    check_version();
    check_completion();
    check_completion_bash();
    check_completion_fish();
}


/* ── Root --help ─────────────────────────────────────────────────────────── */

static void check_root_help(void) {
    ScArgs *args = test_args_build_demo();
    char *help = capture_plain(render_root_help, args);

    CHECK(test_args_contains(help, "demo 1.2.3"),
          "help: header shows prog + version");
    CHECK(test_args_contains(help, "A demo tool for the test suite"),
          "help: header shows the about line");
    CHECK(test_args_contains(help, "Usage: demo [options] <command>"),
          "help: usage line reflects the tree shape");
    CHECK(test_args_contains(help, "Work commands:")
              && test_args_contains(help, "build")
              && test_args_contains(help, "Remove artifacts"),
          "help: grouped subcommands are listed under their heading");
    CHECK(test_args_contains(help, "Commands:")
              && test_args_contains(help, "remote"),
          "help: ungrouped subcommands fall under 'Commands'");
    CHECK(test_args_contains(help, "-v, --verbose")
              && test_args_contains(help, "Verbose output"),
          "help: options table lists flags with short aliases");
    CHECK(test_args_contains(help, "-h, --help")
              && test_args_contains(help, "-V, --version"),
          "help: reserved options are listed");
    CHECK(test_args_contains(help, "<command> --help"),
          "help: footer points at per-command help");

    free(help);
    sc_args_free(args);
}


/* ── Subcommand --help ───────────────────────────────────────────────────── */

static void check_subcommand_help(void) {
    ScArgs *args = test_args_build_demo();
    char *help = capture_plain(render_build_help, args);

    CHECK(test_args_contains(help, "demo build"),
          "help: subcommand header shows the command path");
    CHECK(test_args_contains(help, "Usage: demo build [options] <TARGET> [EXTRA...]"),
          "help: usage shows required/optional/variadic positionals");
    CHECK(test_args_contains(help, "Arguments:")
              && test_args_contains(help, "TARGET")
              && test_args_contains(help, "(required)"),
          "help: arguments section lists positionals");
    CHECK(test_args_contains(help, "[default: 4]"),
          "help: option defaults are rendered");
    CHECK(test_args_contains(help, "[choices: debug, release]"),
          "help: option choices are rendered");
    CHECK(test_args_contains(help, "--verbose"),
          "help: inherited parent options are listed");

    free(help);
    sc_args_free(args);
}


/* ── --version ───────────────────────────────────────────────────────────── */

static void check_version(void) {
    ScArgs *args = test_args_build_demo();
    char *version = capture_plain(render_version_cb, args);
    CHECK(test_args_contains(version, "demo")
              && test_args_contains(version, "1.2.3"),
          "help: --version prints prog + version");
    free(version);
    sc_args_free(args);

    // Parsing --version yields HANDLED and no matched command
    args = test_args_build_demo();
    char *argv[] = { "demo", "--version" };
    ScArgsStatus status;
    FILE *capture = tmpfile();
    FILE *previous = sc_output_stream();
    sc_output_set_stream(capture);
    const ScArgsCmd *matched = sc_args_parse(args, 2, argv, &status);
    sc_output_set_stream(previous);
    if (capture) { fclose(capture); }

    CHECK(status == SC_ARGS_HANDLED && matched == NULL,
          "help: --version parse outcome is SC_ARGS_HANDLED");
    sc_args_free(args);
}


/* ── Zsh completion ──────────────────────────────────────────────────────── */

static void check_completion(void) {
    ScArgs *args = test_args_build_demo();
    char *script = capture_plain(render_completion, args);

    CHECK(test_args_contains(script, "#compdef demo"),
          "completion: script starts with the #compdef header");
    CHECK(test_args_contains(script, "{-v,--verbose}'[Verbose output]'"),
          "completion: flags with short aliases use brace expansion");
    CHECK(test_args_contains(script, "'build:Build the project'"),
          "completion: subcommands are described");
    CHECK(test_args_contains(script, "{-j,--jobs}'=[Parallel jobs]:N:'"),
          "completion: value options carry their metavar");
    CHECK(test_args_contains(script, "(debug release)"),
          "completion: choices become completion candidates");
    CHECK(test_args_contains(script, "_demo \"$@\""),
          "completion: script ends with the function invocation");

    free(script);
    sc_args_free(args);
}


/* ── Bash completion ─────────────────────────────────────────────────────── */

static void check_completion_bash(void) {
    ScArgs *args = test_args_build_demo();
    char *script = capture_plain(render_completion_bash, args);

    CHECK(test_args_contains(script, "complete -F _demo demo"),
          "bash: registers the completion function");
    CHECK(test_args_contains(script, "--verbose"),
          "bash: root options are offered");
    CHECK(test_args_contains(script, "build)"),
          "bash: per-subcommand branch exists");
    CHECK(test_args_contains(script, "--jobs"),
          "bash: subcommand options are offered");
    CHECK(test_args_contains(script, "debug release"),
          "bash: choices complete after the option");

    free(script);
    sc_args_free(args);
}


/* ── Fish completion ─────────────────────────────────────────────────────── */

static void check_completion_fish(void) {
    ScArgs *args = test_args_build_demo();
    char *script = capture_plain(render_completion_fish, args);

    CHECK(test_args_contains(script,
              "complete -c demo -f -n __fish_use_subcommand -a build"),
          "fish: subcommands gated by __fish_use_subcommand");
    CHECK(test_args_contains(script, "-l verbose"),
          "fish: long options emitted");
    CHECK(test_args_contains(script,
              "__fish_seen_subcommand_from build"),
          "fish: subcommand options gated by seen-subcommand");
    CHECK(test_args_contains(script, "-l jobs -r"),
          "fish: value options marked requiring an argument");
    CHECK(test_args_contains(script, "-a \"debug release\""),
          "fish: choices listed as candidates");

    free(script);
    sc_args_free(args);
}


/* ── Render callbacks + capture helper ───────────────────────────────────── */

static void render_root_help(ScArgs *args) {
    sc_args_print_help(args, NULL);
}

static void render_build_help(ScArgs *args) {
    // Parse once to obtain the "build" node, then render its help screen
    char *argv[] = { "demo", "build", "ignored" };
    ScArgsStatus status;
    const ScArgsCmd *build = sc_args_parse(args, 3, argv, &status);
    sc_args_print_help(args, (ScArgsCmd *)build);
}

static void render_version_cb(ScArgs *args) {
    char *argv[] = { "demo", "--version" };
    ScArgsStatus status;
    sc_args_parse(args, 2, argv, &status);
}

static void render_completion(ScArgs *args) {
    sc_args_print_zsh_completion(args);
}

static void render_completion_bash(ScArgs *args) {
    sc_args_print_bash_completion(args);
}

static void render_completion_fish(ScArgs *args) {
    sc_args_print_fish_completion(args);
}

/** Captures a render callback's output and strips ANSI codes. */
static char *capture_plain(void (*render)(ScArgs *), ScArgs *args) {
    if (!render || !args) { return strdup(""); }
    char *raw = test_args_capture_output(render, args);
    if (!raw) { return NULL; }
    char *plain = sc_strip_ansi(raw);
    free(raw);
    return plain;
}
