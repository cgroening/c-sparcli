#include "test_args.h"


// Forward declarations indented to reflect call hierarchy
static void check_basic_parsing(void);
static void check_option_forms(void);
static void check_short_options(void);
static void check_subcommands(void);
static void check_positionals(void);
static void check_defaults_and_types(void);
static void check_empty_choices_clears(void);
static void check_double_dash(void);
static void check_opts_are_copied(void);
static void check_reset(void);
static void check_reparse_is_clean(void);
static void check_userdata(void);


/** Parse loop, value binding, getters, defaults and the opts-copy rule. */
void test_args_parse(void) {
    check_basic_parsing();
    check_option_forms();
    check_short_options();
    check_subcommands();
    check_positionals();
    check_defaults_and_types();
    check_empty_choices_clears();
    check_double_dash();
    check_opts_are_copied();
    check_reset();
    check_reparse_is_clean();
    check_userdata();
}


/* ── Basic parsing ───────────────────────────────────────────────────────── */

static void check_basic_parsing(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "--verbose", "build", "--jobs", "8", "app" };
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, 6, argv, &status);

    CHECK(status == SC_ARGS_MATCHED, "parse: full command line matches");
    CHECK(matched != NULL
              && strcmp(sc_args_cmd_name(matched), "build") == 0,
          "parse: the deepest command is returned");
    CHECK(strcmp(sc_args_selected_command(args), "build") == 0,
          "parse: sc_args_selected_command returns the leaf name");
    CHECK(sc_args_get_flag(args, "verbose"),
          "parse: parent flag is visible from the subcommand");
    CHECK(!sc_args_get_flag(args, "quiet"),
          "parse: unset flags read as false");
    CHECK(sc_args_get_int(args, "jobs") == 8,
          "parse: integer option value is converted");
    CHECK(strcmp(sc_args_get_str(args, "TARGET"), "app") == 0,
          "parse: positional value is bound");
    CHECK(sc_args_present(args, "jobs") && !sc_args_present(args, "scale"),
          "parse: sc_args_present reflects what was given");
    sc_args_free(args);
}


/* ── --opt value vs --opt=value ──────────────────────────────────────────── */

static void check_option_forms(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = {
        "demo", "build", "--jobs=16", "--mode", "release", "app"
    };
    ScArgsStatus status;
    sc_args_parse(args, 6, argv, &status);

    CHECK(status == SC_ARGS_MATCHED, "parse: --opt=value and --opt value mix");
    CHECK(sc_args_get_int(args, "jobs") == 16,
          "parse: --opt=value binds the inline value");
    CHECK(strcmp(sc_args_get_str(args, "mode"), "release") == 0,
          "parse: --opt value binds the next token");
    CHECK(sc_args_get_enum(args, "mode") == 1,
          "parse: sc_args_get_enum returns the choice index");
    sc_args_free(args);
}


/* ── Short options ───────────────────────────────────────────────────────── */

static void check_short_options(void) {
    // Combined boolean flags: -vq
    ScArgs *args = test_args_build_demo();
    char *argv_combined[] = { "demo", "-vq", "clean" };
    ScArgsStatus status;
    sc_args_parse(args, 3, argv_combined, &status);
    CHECK(status == SC_ARGS_MATCHED
              && sc_args_get_flag(args, "verbose")
              && sc_args_get_flag(args, "quiet"),
          "parse: combined short flags (-vq) set both");
    sc_args_free(args);

    // Short option with attached value: -j8
    args = test_args_build_demo();
    char *argv_attached[] = { "demo", "build", "-j8", "app" };
    sc_args_parse(args, 4, argv_attached, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "jobs") == 8,
          "parse: -j8 binds the attached value");
    sc_args_free(args);

    // Short option with separate value: -j 8
    args = test_args_build_demo();
    char *argv_separate[] = { "demo", "build", "-j", "8", "app" };
    sc_args_parse(args, 5, argv_separate, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "jobs") == 8,
          "parse: -j 8 binds the next token");
    sc_args_free(args);
}


/* ── Subcommand descent ──────────────────────────────────────────────────── */

static void check_subcommands(void) {
    // Nested subcommand: remote add
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "remote", "add" };
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, 3, argv, &status);
    CHECK(status == SC_ARGS_MATCHED
              && strcmp(sc_args_cmd_name(matched), "add") == 0,
          "parse: nested subcommands descend (remote add)");
    sc_args_free(args);

    // No subcommand at all matches the root
    args = test_args_build_demo();
    char *argv_root[] = { "demo", "--verbose" };
    matched = sc_args_parse(args, 2, argv_root, &status);
    CHECK(status == SC_ARGS_MATCHED
              && strcmp(sc_args_cmd_name(matched), "demo") == 0,
          "parse: no subcommand matches the root command");
    sc_args_free(args);
}


/* ── Positionals ─────────────────────────────────────────────────────────── */

static void check_positionals(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "build", "app", "one", "two", "three" };
    ScArgsStatus status;
    sc_args_parse(args, 6, argv, &status);

    CHECK(status == SC_ARGS_MATCHED, "parse: positionals + variadic tail");
    CHECK(strcmp(sc_args_get_str(args, "TARGET"), "app") == 0,
          "parse: first positional fills the first slot");

    size_t count = 0;
    const char *const *extra = sc_args_get_many(args, "EXTRA", &count);
    CHECK(count == 3 && extra
              && strcmp(extra[0], "one") == 0
              && strcmp(extra[2], "three") == 0,
          "parse: variadic positional collects the remaining tokens");
    sc_args_free(args);
}


/* ── Defaults and typed values ───────────────────────────────────────────── */

static void check_defaults_and_types(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "build", "app", "--scale", "2.5" };
    ScArgsStatus status;
    sc_args_parse(args, 5, argv, &status);

    CHECK(sc_args_get_int(args, "jobs") == 4,
          "parse: absent option falls back to its default");
    CHECK(!sc_args_present(args, "jobs"),
          "parse: defaults do not count as present");
    CHECK(sc_args_get_double(args, "scale") == 2.5,
          "parse: double option value is converted");
    sc_args_free(args);

    // Color values: named, hex and R,G,B
    args = test_args_build_demo();
    char *argv_color[] = { "demo", "--color", "red", "clean" };
    sc_args_parse(args, 4, argv_color, &status);
    CHECK(sc_args_get_color(args, "color").index == 2,
          "parse: named color value");
    sc_args_free(args);

    args = test_args_build_demo();
    char *argv_hex[] = { "demo", "--color", "#10b981", "clean" };
    sc_args_parse(args, 4, argv_hex, &status);
    ScColor hex_color = sc_args_get_color(args, "color");
    CHECK(hex_color.index == -1 && hex_color.r == 0x10 && hex_color.g == 0xb9,
          "parse: #RRGGBB color value");
    sc_args_free(args);

    args = test_args_build_demo();
    char *argv_rgb[] = { "demo", "--color", "12,200,99", "clean" };
    sc_args_parse(args, 4, argv_rgb, &status);
    ScColor rgb_color = sc_args_get_color(args, "color");
    CHECK(rgb_color.index == -1 && rgb_color.b == 99,
          "parse: R,G,B color value");
    sc_args_free(args);

    // Negative numbers are values, not options
    args = sc_args_new((ScArgsOpts){ .prog = "neg", .about = "" });
    sc_args_opt(sc_args_root(args), "offset", 'o', SC_ARG_INT, "N", "Offset");
    char *argv_negative[] = { "neg", "--offset", "-5" };
    sc_args_parse(args, 3, argv_negative, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "offset") == -5,
          "parse: negative numbers are accepted as values");
    sc_args_free(args);
}


/* ── Choices: an empty list clears the restriction ───────────────────────── */

static void check_empty_choices_clears(void) {
    ScArgs *args = sc_args_new((ScArgsOpts){ .prog = "demo", .about = "" });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_opt(root, "mode", 'm', SC_ARG_STR, "MODE", "Build mode");
    sc_args_opt_choices(
        root, "mode", (const char *[]){ "debug", "release", NULL }
    );
    // Passing an empty list removes the restriction again
    sc_args_opt_choices(root, "mode", (const char *[]){ NULL });

    char *argv[] = { "demo", "--mode", "anything" };
    ScArgsStatus status;
    sc_args_parse(args, 3, argv, &status);
    CHECK(status == SC_ARGS_MATCHED
              && strcmp(sc_args_get_str(args, "mode"), "anything") == 0,
          "parse: an empty choices list clears a previous restriction");
    sc_args_free(args);
}


/* ── The -- terminator ───────────────────────────────────────────────────── */

static void check_double_dash(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "build", "app", "--", "--jobs", "-v" };
    ScArgsStatus status;
    sc_args_parse(args, 6, argv, &status);

    size_t count = 0;
    const char *const *extra = sc_args_get_many(args, "EXTRA", &count);
    CHECK(status == SC_ARGS_MATCHED && count == 2 && extra
              && strcmp(extra[0], "--jobs") == 0
              && strcmp(extra[1], "-v") == 0,
          "parse: tokens after -- are literal positionals");
    CHECK(!sc_args_present(args, "jobs"),
          "parse: option-looking tokens after -- are not options");
    sc_args_free(args);
}


/* ── Opts-copy invariant ─────────────────────────────────────────────────── */

static void check_opts_are_copied(void) {
    char prog[32];
    char help[64];
    char choice_a[16];
    char choice_b[16];
    snprintf(prog, sizeof prog, "copytool");
    snprintf(help, sizeof help, "temporary help text");
    snprintf(choice_a, sizeof choice_a, "alpha");
    snprintf(choice_b, sizeof choice_b, "beta");

    ScArgs *args = sc_args_new((ScArgsOpts){ .prog = prog, .about = help });
    ScArgsCmd *root = sc_args_root(args);
    sc_args_opt(root, "pick", 'p', SC_ARG_STR, "X", help);
    sc_args_opt_choices(
        root, "pick", (const char *[]){ choice_a, choice_b, NULL }
    );

    // Clobber every caller buffer; the parser must hold its own copies
    memset(prog, 'X', sizeof prog - 1);
    memset(help, 'X', sizeof help - 1);
    memset(choice_a, 'X', sizeof choice_a - 1);
    memset(choice_b, 'X', sizeof choice_b - 1);

    char *argv[] = { "copytool", "--pick", "beta" };
    ScArgsStatus status;
    sc_args_parse(args, 3, argv, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_enum(args, "pick") == 1,
          "parse: builder strings are copied (clobbered buffers are safe)");
    sc_args_free(args);
}


/* ── sc_args_reset ───────────────────────────────────────────────────────── */

static void check_reset(void) {
    ScArgs *args = test_args_build_demo();
    char *argv[] = { "demo", "--verbose", "build", "--jobs", "8", "app" };
    ScArgsStatus status;
    sc_args_parse(args, 6, argv, &status);

    sc_args_reset(args);
    CHECK(!sc_args_get_flag(args, "verbose"),
          "reset: flags are cleared");
    CHECK(!sc_args_present(args, "jobs"),
          "reset: option values are cleared");
    CHECK(sc_args_get_str(args, "TARGET") == NULL,
          "reset: positional values are cleared");
    CHECK(sc_args_selected_command(args) == NULL,
          "reset: the matched command is cleared");

    // The tree stays intact: the same line parses again after a reset
    sc_args_parse(args, 6, argv, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "jobs") == 8,
          "reset: the tree is reusable after a reset");

    // Re-parse without --jobs: the default definition survived the reset
    char *argv_no_jobs[] = { "demo", "build", "app" };
    sc_args_parse(args, 3, argv_no_jobs, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "jobs") == 4,
          "reset: option defaults survive a reset");
    sc_args_free(args);

    // NULL safety
    sc_args_reset(NULL);
    CHECK(true, "reset: NULL parser is a safe no-op");
}


/* ── Implicit reset on re-parse (REPL loop) ──────────────────────────────── */

static void check_reparse_is_clean(void) {
    ScArgs *args = test_args_build_demo();
    ScArgsStatus status;

    // First line binds options + positionals on the build subcommand
    char *argv_first[] = {
        "demo", "--verbose", "build", "--jobs", "8", "app", "extra1", "extra2"
    };
    sc_args_parse(args, 8, argv_first, &status);
    CHECK(status == SC_ARGS_MATCHED && sc_args_get_int(args, "jobs") == 8,
          "reparse: first parse binds values");

    // Second line on the same tree: nothing from the first may survive
    char *argv_second[] = { "demo", "clean" };
    const ScArgsCmd *matched = sc_args_parse(args, 2, argv_second, &status);
    CHECK(status == SC_ARGS_MATCHED
              && strcmp(sc_args_cmd_name(matched), "clean") == 0,
          "reparse: second parse matches the new command");
    CHECK(!sc_args_get_flag(args, "verbose"),
          "reparse: flags from the previous run are gone");
    CHECK(!sc_args_present(args, "jobs"),
          "reparse: option values from the previous run are gone");

    size_t count = 99;
    sc_args_get_many(args, "EXTRA", &count);
    CHECK(count == 0,
          "reparse: variadic positionals from the previous run are gone");
    sc_args_free(args);
}


/* ── Per-command user_data (dispatch registry) ───────────────────────────── */

static void check_userdata(void) {
    int build_marker = 0;
    int clean_marker = 0;

    ScArgs *args = sc_args_new((ScArgsOpts){ .prog = "demo", .about = "" });
    ScArgsCmd *root = sc_args_root(args);
    ScArgsCmd *build = sc_args_subcommand(root, "build", "Build");
    ScArgsCmd *clean = sc_args_subcommand(root, "clean", "Clean");
    sc_args_cmd_set_userdata(build, &build_marker);
    sc_args_cmd_set_userdata(clean, &clean_marker);

    CHECK(sc_args_cmd_userdata(clean) == &clean_marker
              && sc_args_cmd_userdata(build) == &build_marker,
          "userdata: pointers are stored per-node, read back verbatim");

    char *argv[] = { "demo", "build" };
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, 2, argv, &status);
    CHECK(matched != NULL && sc_args_cmd_userdata(matched) == &build_marker,
          "userdata: the matched node carries its own pointer");

    // Default (never set) and NULL safety
    ScArgsCmd *plain = sc_args_subcommand(root, "plain", "No userdata");
    CHECK(sc_args_cmd_userdata(plain) == NULL,
          "userdata: a node without a set pointer reads as NULL");
    CHECK(sc_args_cmd_userdata(NULL) == NULL,
          "userdata: sc_args_cmd_userdata(NULL) is a safe NULL");
    sc_args_cmd_set_userdata(NULL, &build_marker);
    CHECK(true, "userdata: sc_args_cmd_set_userdata(NULL, ...) is a safe no-op");

    sc_args_free(args);
}
