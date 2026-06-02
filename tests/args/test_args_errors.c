#include "test_args.h"


// Forward declarations indented to reflect call hierarchy
static void check_unknown_option_suggestion(void);
static void check_unknown_command_suggestion(void);
static void check_value_errors(void);
static void check_required_errors(void);
static void check_levenshtein(void);
static void check_argv_sanitizing(void);
static void check_null_safety(void);
    static char *parse_expect_error(ScArgs *args, int argc, char **argv);


/** Error reporting, did-you-mean suggestions and input hardening. */
void test_args_errors(void) {
    check_unknown_option_suggestion();
    check_unknown_command_suggestion();
    check_value_errors();
    check_required_errors();
    check_levenshtein();
    check_argv_sanitizing();
    check_null_safety();
}


/* ── Unknown options ─────────────────────────────────────────────────────── */

static void check_unknown_option_suggestion(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "--verbos" };
    char *captured = parse_expect_error(args, 2, argv);

    CHECK(test_args_contains(captured, "Unknown option '--verbos'"),
          "errors: unknown option names the bad flag");
    CHECK(test_args_contains(captured, "Did you mean '--verbose'?"),
          "errors: close match produces a did-you-mean hint");
    free(captured);
    sc_args_free(args);

    // No close match: point to --help instead
    args = test_args_build_demo();
    char *argv_far[] = { "demo", "--zzzzzzz" };
    captured = parse_expect_error(args, 2, argv_far);
    CHECK(test_args_contains(captured, "--help"),
          "errors: distant typos point to --help instead of guessing");
    free(captured);
    sc_args_free(args);
}


/* ── Unknown commands ────────────────────────────────────────────────────── */

static void check_unknown_command_suggestion(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "buidl", "app" };
    char *captured = parse_expect_error(args, 3, argv);

    CHECK(test_args_contains(captured, "Unknown command 'buidl'"),
          "errors: unknown command names the bad word");
    CHECK(test_args_contains(captured, "Did you mean 'build'?"),
          "errors: close command match produces a did-you-mean hint");
    free(captured);
    sc_args_free(args);
}


/* ── Value errors ────────────────────────────────────────────────────────── */

static void check_value_errors(void) {
    // Missing value
    ScArgs *args = test_args_build_demo();
    char *argv_missing[] = { "demo", "build", "app", "--jobs" };
    char *captured = parse_expect_error(args, 4, argv_missing);
    CHECK(test_args_contains(captured, "requires a value"),
          "errors: option at end of line reports a missing value");
    free(captured);
    sc_args_free(args);

    // Bad integer
    args = test_args_build_demo();
    char *argv_bad_int[] = { "demo", "build", "app", "--jobs", "many" };
    captured = parse_expect_error(args, 5, argv_bad_int);
    CHECK(test_args_contains(captured, "Invalid value 'many'")
              && test_args_contains(captured, "expected an integer"),
          "errors: non-numeric value for an int option is rejected");
    free(captured);
    sc_args_free(args);

    // Invalid choice lists the valid ones
    args = test_args_build_demo();
    char *argv_bad_choice[] = { "demo", "build", "app", "--mode", "turbo" };
    captured = parse_expect_error(args, 5, argv_bad_choice);
    CHECK(test_args_contains(captured, "valid choices: debug, release"),
          "errors: invalid choice lists the allowed values");
    free(captured);
    sc_args_free(args);

    // Flag with a value
    args = test_args_build_demo();
    char *argv_flag_value[] = { "demo", "--verbose=yes" };
    captured = parse_expect_error(args, 2, argv_flag_value);
    CHECK(test_args_contains(captured, "does not take a value"),
          "errors: flags reject inline values");
    free(captured);
    sc_args_free(args);

    // Unexpected extra positional
    args = sc_args_new((ScArgsOpts){ .prog = "flat", .about = "" });
    sc_args_positional(
        sc_args_root(args), "ONE", SC_ARG_STR, "Only one", false, false
    );
    char *argv_extra[] = { "flat", "first", "second" };
    captured = parse_expect_error(args, 3, argv_extra);
    CHECK(test_args_contains(captured, "Unexpected argument 'second'"),
          "errors: surplus positionals are rejected");
    free(captured);
    sc_args_free(args);
}


/* ── Required options / positionals ──────────────────────────────────────── */

static void check_required_errors(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){ .prog = "req", .about = "" });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_opt(root, "output", 'o', SC_ARG_STR, "FILE", "Write to FILE");
    sc_args_opt_required(root, "output");

    char *argv[] = { "req" };
    char *captured = parse_expect_error(args, 1, argv);
    CHECK(test_args_contains(captured, "Missing required option '--output'"),
          "errors: missing required option is reported");
    free(captured);
    sc_args_free(args);

    // Required positional on the matched command
    ScArgs *demo = test_args_build_demo();
    char *argv_no_target[] = { "demo", "build" };
    captured = parse_expect_error(demo, 2, argv_no_target);
    CHECK(test_args_contains(captured, "Missing required argument 'TARGET'"),
          "errors: missing required positional is reported");
    free(captured);
    sc_args_free(demo);
}


/* ── Levenshtein distance ────────────────────────────────────────────────── */

static void check_levenshtein(void) {
    // Exposed indirectly: suggestions only fire within distance <= 2
    ScArgs *args = test_args_build_demo();

    // distance 1 (transposition counts as 2 substitutions -> still close)
    char *argv_one[] = { "demo", "--verbose1" };
    char *captured = parse_expect_error(args, 2, argv_one);
    CHECK(test_args_contains(captured, "Did you mean '--verbose'?"),
          "suggest: distance-1 typo is suggested");
    free(captured);
    sc_args_free(args);

    args = test_args_build_demo();
    char *argv_two[] = { "demo", "--vrbose" };
    captured = parse_expect_error(args, 2, argv_two);
    CHECK(test_args_contains(captured, "Did you mean '--verbose'?"),
          "suggest: distance-2 typo is suggested");
    free(captured);
    sc_args_free(args);
}


/* ── argv is untrusted input ─────────────────────────────────────────────── */

static void check_argv_sanitizing(void) {
    ScArgs *args = test_args_build_demo();

    // Escape sequences in argv must not reach the error output
    char *argv[] = { "demo", "--\033[31mevil\033[0m" };
    char *captured = parse_expect_error(args, 2, argv);
    CHECK(captured && strstr(captured, "\033[31m") == NULL,
          "hardening: injected CSI in argv never reaches the error output");
    free(captured);
    sc_args_free(args);

    // Escape sequences in positional values are stripped before storage
    args = test_args_build_demo();
    char *argv_value[] = { "demo", "build", "app\033[31mred" };
    ScArgsStatus status;
    sc_args_parse(args, 3, argv_value, &status);
    const char *target = sc_args_get_str(args, "TARGET");
    CHECK(status == SC_ARGS_MATCHED && target
              && strcmp(target, "appred") == 0,
          "hardening: stored values are ANSI-sanitized");
    sc_args_free(args);
}


/* ── NULL safety ─────────────────────────────────────────────────────────── */

static void check_null_safety(void) {
    ScArgsStatus status;
    CHECK(sc_args_parse(NULL, 0, NULL, &status) == NULL
              && status == SC_ARGS_ERROR,
          "safety: parsing a NULL parser fails cleanly");

    sc_args_free(NULL);
    CHECK(sc_args_root(NULL) == NULL, "safety: NULL parser has no root");
    CHECK(sc_args_subcommand(NULL, "x", "y") == NULL,
          "safety: subcommand on NULL parent is rejected");

    ScArgs *args = sc_args_new((ScArgsOpts){ 0 });
    sc_args_flag(NULL, "x", 0, "y");
    sc_args_opt(NULL, "x", 0, SC_ARG_STR, NULL, "y");
    sc_args_opt_default(sc_args_root(args), "missing", "1");
    sc_args_opt_choices(sc_args_root(args), "missing", NULL);
    sc_args_opt_required(sc_args_root(args), "missing");
    sc_args_positional(NULL, "X", SC_ARG_STR, "y", false, false);

    CHECK(sc_args_get_str(args, "anything") == NULL
              && sc_args_get_int(args, "anything") == 0
              && !sc_args_get_flag(args, "anything")
              && !sc_args_present(args, "anything"),
          "safety: getters on unknown names return empty defaults");
    CHECK(sc_args_selected_command(args) == NULL,
          "safety: selected command is NULL before parsing");
    sc_args_free(args);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** Parses expecting SC_ARGS_ERROR; returns the captured stderr text. */
static char *parse_expect_error(ScArgs *args, int argc, char **argv) {
    ScArgsStatus status;
    const ScArgsCmd *matched = NULL;
    char *captured = test_args_parse_capture_stderr(
        args, argc, argv, &status, &matched
    );
    if (status != SC_ARGS_ERROR || matched != NULL) {
        // Make the failure visible in the test output
        free(captured);
        return strdup("<<parse did not fail as expected>>");
    }
    // Strip ANSI so substring checks see the plain panel text
    char *plain = captured ? sc_strip_ansi(captured) : NULL;
    free(captured);
    return plain;
}
