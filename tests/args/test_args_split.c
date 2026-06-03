#include "test_args.h"


// Forward declarations indented to reflect call hierarchy
static void check_basic_splitting(void);
static void check_quoting(void);
static void check_escapes(void);
static void check_errors(void);
static void check_parse_integration(void);


/** The quote-aware line tokenizer (`sc_args_split`). */
void test_args_split(void) {
    check_basic_splitting();
    check_quoting();
    check_escapes();
    check_errors();
    check_parse_integration();
}


/* ── Whitespace splitting ────────────────────────────────────────────────── */

static void check_basic_splitting(void) {
    int argc = 0;
    char **argv = sc_args_split("tool", "add item --count 3", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 5, "split: whitespace separates tokens");
    CHECK(argv && strcmp(argv[0], "tool") == 0,
          "split: argv[0] is the program name");
    CHECK(argv && strcmp(argv[1], "add") == 0
              && strcmp(argv[4], "3") == 0,
          "split: tokens keep their order");
    CHECK(argv && argv[5] == NULL, "split: argv is NULL-terminated");
    sc_args_split_free(argv);

    // Runs of whitespace (spaces and tabs) collapse
    argv = sc_args_split("tool", "  a \t  b  ", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 3
              && strcmp(argv[1], "a") == 0 && strcmp(argv[2], "b") == 0,
          "split: whitespace runs collapse");
    sc_args_split_free(argv);

    // Empty and NULL lines produce just argv[0]
    argv = sc_args_split("tool", "", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 1 && argv[1] == NULL,
          "split: empty line yields only argv[0]");
    sc_args_split_free(argv);

    argv = sc_args_split("tool", NULL, &argc, NULL, 0);
    CHECK(argv != NULL && argc == 1, "split: NULL line yields only argv[0]");
    sc_args_split_free(argv);
}


/* ── Quotes ──────────────────────────────────────────────────────────────── */

static void check_quoting(void) {
    int argc = 0;

    // Double quotes group words into one token
    char **argv = sc_args_split(
        "tool", "add \"Lunch at Joe's Diner\" --amount 12.50", &argc, NULL, 0
    );
    CHECK(argv != NULL && argc == 5
              && strcmp(argv[2], "Lunch at Joe's Diner") == 0,
          "split: double quotes group words (apostrophe kept)");
    sc_args_split_free(argv);

    // Single quotes are literal (no escapes inside)
    argv = sc_args_split("tool", "echo 'a \"b\" \\n c'", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 3
              && strcmp(argv[2], "a \"b\" \\n c") == 0,
          "split: single quotes are literal");
    sc_args_split_free(argv);

    // Quoted and bare runs concatenate into one token
    argv = sc_args_split("tool", "a\"b c\"d", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 2 && strcmp(argv[1], "ab cd") == 0,
          "split: quoted and bare runs concatenate");
    sc_args_split_free(argv);

    // Empty quotes produce an empty token
    argv = sc_args_split("tool", "set name \"\"", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 4 && strcmp(argv[3], "") == 0,
          "split: empty quotes produce an empty token");
    sc_args_split_free(argv);
}


/* ── Backslash escapes ───────────────────────────────────────────────────── */

static void check_escapes(void) {
    int argc = 0;

    // Bare backslash escapes a space (no quoting needed)
    char **argv = sc_args_split("tool", "open My\\ File.txt", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 3 && strcmp(argv[2], "My File.txt") == 0,
          "split: backslash escapes a space");
    sc_args_split_free(argv);

    // Backslash escapes a quote character
    argv = sc_args_split("tool", "say \\\"hi\\\"", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 3 && strcmp(argv[2], "\"hi\"") == 0,
          "split: backslash escapes quotes");
    sc_args_split_free(argv);

    // Inside double quotes: \" is a literal quote, \\ a literal backslash
    argv = sc_args_split("tool", "msg \"she said \\\"hi\\\"\"", &argc, NULL, 0);
    CHECK(argv != NULL && argc == 3
              && strcmp(argv[2], "she said \"hi\"") == 0,
          "split: escaped quotes inside double quotes");
    sc_args_split_free(argv);
}


/* ── Error reporting ─────────────────────────────────────────────────────── */

static void check_errors(void) {
    int argc = 7;
    char err[64] = "x";

    // Unterminated double quote
    char **argv = sc_args_split("tool", "add \"oops", &argc, err, sizeof err);
    CHECK(argv == NULL, "split: unterminated double quote fails");
    CHECK(argc == 0, "split: argc is zeroed on error");
    CHECK(test_args_contains(err, "unterminated quote"),
          "split: the error buffer names the reason");

    // Unterminated single quote
    argv = sc_args_split("tool", "add 'oops", &argc, err, sizeof err);
    CHECK(argv == NULL && test_args_contains(err, "unterminated quote"),
          "split: unterminated single quote fails");

    // Trailing backslash
    argv = sc_args_split("tool", "add oops\\", &argc, err, sizeof err);
    CHECK(argv == NULL && test_args_contains(err, "trailing backslash"),
          "split: trailing backslash fails");

    // A NULL error buffer is accepted
    argv = sc_args_split("tool", "add \"oops", &argc, NULL, 0);
    CHECK(argv == NULL, "split: NULL error buffer is safe");
}


/* ── Round trip into sc_args_parse ───────────────────────────────────────── */

static void check_parse_integration(void) {
    ScArgs *args = test_args_build_demo();
    int argc = 0;
    char err[64];

    // A quoted REPL line parses exactly like a shell command line
    char **argv = sc_args_split(
        "demo", "build --jobs 8 \"my app\" extra", &argc, err, sizeof err
    );
    ScArgsStatus status;
    const ScArgsCmd *matched = sc_args_parse(args, argc, argv, &status);
    CHECK(status == SC_ARGS_MATCHED && matched != NULL
              && strcmp(sc_args_cmd_name(matched), "build") == 0,
          "split: tokenized line parses into the command tree");
    CHECK(sc_args_get_int(args, "jobs") == 8
              && strcmp(sc_args_get_str(args, "TARGET"), "my app") == 0,
          "split: quoted positional reaches the parser intact");
    sc_args_split_free(argv);
    sc_args_free(args);
}
