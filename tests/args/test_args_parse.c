#include "test_args.h"


// Forward declarations indented to reflect call hierarchy
static void check_basic_parsing(void);
static void check_option_forms(void);
static void check_short_options(void);
static void check_subcommands(void);
static void check_positionals(void);
static void check_defaults_and_types(void);
static void check_double_dash(void);
static void check_opts_are_copied(void);


/** Parse loop, value binding, getters, defaults and the opts-copy rule. */
void test_args_parse(void) {
    check_basic_parsing();
    check_option_forms();
    check_short_options();
    check_subcommands();
    check_positionals();
    check_defaults_and_types();
    check_double_dash();
    check_opts_are_copied();
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
