#include "test_app.h"
#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Forward declarations indented to reflect call hierarchy
static void check_builder_defaults(void);
static void check_strings_are_copied(void);
static void check_cause_chain_growth(void);
static void check_null_safety(void);
static void check_rendering(void);
static void check_stderr_rendering(void);
    static char *capture_error_print(const ScError *error);
static bool str_contains(const char *haystack, const char *needle);


/**
 * Pretty-error builder logic: defaults, string copying, cause-chain
 * growth, NULL safety, and rendering to the output stream / stderr.
 * (`sc_die` exits the process and is therefore not testable here; it is
 * covered by the CLI/PTY suites once the args module uses it.)
 */
void test_errors(void) {
    check_builder_defaults();
    check_strings_are_copied();
    check_cause_chain_growth();
    check_null_safety();
    check_rendering();
    check_stderr_rendering();
}


/* ── Defaults ────────────────────────────────────────────────────────────── */

static void check_builder_defaults(void) {
    ScError *error = sc_error_new("boom");
    CHECK(error != NULL, "errors: sc_error_new allocates");
    CHECK(sc_error_code(error) == 1, "errors: default exit code is 1");

    sc_error_set_code(error, 42);
    CHECK(sc_error_code(error) == 42, "errors: sc_error_set_code stores the code");
    sc_error_free(error);

    CHECK(sc_error_code(NULL) == 1, "errors: sc_error_code(NULL) returns 1");
}


/* ── Opts-copy invariant ─────────────────────────────────────────────────── */

static void check_strings_are_copied(void) {
    // Build every string in clobberable stack buffers
    char message[64];
    char cause[64];
    char hint[64];
    snprintf(message, sizeof message, "temporary message");
    snprintf(cause, sizeof cause, "temporary cause");
    snprintf(hint, sizeof hint, "temporary hint");

    ScError *error = sc_error_new(message);
    sc_error_add_cause(error, cause);
    sc_error_set_hint(error, hint);

    // Clobber the caller buffers; the error must keep its own copies
    memset(message, 'X', sizeof message - 1);
    memset(cause, 'X', sizeof cause - 1);
    memset(hint, 'X', sizeof hint - 1);

    char *rendered = capture_error_print(error);
    CHECK(str_contains(rendered, "temporary message"),
          "errors: message string is copied");
    CHECK(str_contains(rendered, "temporary cause"),
          "errors: cause string is copied");
    CHECK(str_contains(rendered, "temporary hint"),
          "errors: hint string is copied");
    free(rendered);
    sc_error_free(error);
}


/* ── Cause chain growth ──────────────────────────────────────────────────── */

static void check_cause_chain_growth(void) {
    ScError *error = sc_error_new("many causes");

    // Push past the initial capacity to exercise the realloc path
    char cause[32];
    for (int i = 0; i < 10; i++) {
        snprintf(cause, sizeof cause, "cause number %d", i);
        sc_error_add_cause(error, cause);
    }

    char *rendered = capture_error_print(error);
    CHECK(str_contains(rendered, "cause number 0")
              && str_contains(rendered, "cause number 9"),
          "errors: cause chain grows beyond its initial capacity");
    free(rendered);
    sc_error_free(error);
}


/* ── NULL safety ─────────────────────────────────────────────────────────── */

static void check_null_safety(void) {
    ScError *error = sc_error_new(NULL);
    CHECK(error != NULL, "errors: sc_error_new(NULL) yields an empty message");
    sc_error_free(error);

    // Every entry point must tolerate NULL handles / strings
    sc_error_add_cause(NULL, "cause");
    sc_error_set_hint(NULL, "hint");
    sc_error_set_code(NULL, 5);
    sc_error_print(NULL);
    sc_error_print_stderr(NULL);
    sc_error_free(NULL);

    error = sc_error_new("ok");
    sc_error_add_cause(error, NULL);
    sc_error_set_hint(error, NULL);
    sc_error_free(error);
    CHECK(true, "errors: NULL handles and NULL strings are safe no-ops");
}


/* ── Rendering ───────────────────────────────────────────────────────────── */

static void check_rendering(void) {
    ScError *error = sc_error_new("something broke");
    sc_error_add_cause(error, "the first reason");
    sc_error_set_hint(error, "try again with --force");

    // Search in the ANSI-stripped output: style codes sit between spans
    char *rendered = capture_error_print(error);
    char *plain = rendered ? sc_strip_ansi(rendered) : NULL;
    CHECK(str_contains(plain, "Error"),
          "errors: rendering uses the Error alert preset");
    CHECK(str_contains(plain, "something broke"),
          "errors: rendering contains the message");
    CHECK(str_contains(plain, "caused by: the first reason"),
          "errors: rendering contains the cause line");
    CHECK(str_contains(plain, "Hint: try again with --force"),
          "errors: rendering contains the hint line");
    free(plain);
    free(rendered);

    // Injected escape sequences in user strings never reach the output.
    // The marker is an RGB sequence the panel renderer itself never emits
    // (the alert border uses named red), so any occurrence is the injection.
    ScError *evil = sc_error_new("msg \033[38;2;1;2;3minjected\033[0m");
    char *evil_rendered = capture_error_print(evil);
    CHECK(evil_rendered && strstr(evil_rendered, "\033[38;2;1;2;3m") == NULL,
          "errors: injected CSI sequences are sanitized");
    char *evil_plain = evil_rendered ? sc_strip_ansi(evil_rendered) : NULL;
    CHECK(str_contains(evil_plain, "msg injected"),
          "errors: sanitized message text is kept");
    free(evil_plain);
    free(evil_rendered);
    sc_error_free(evil);

    sc_error_free(error);
}


/* ── stderr rendering ────────────────────────────────────────────────────── */

static void check_stderr_rendering(void) {
    // sc_error_print_stderr must leave the thread-local stream untouched
    FILE *capture = tmpfile();
    if (!capture) {
        CHECK(false, "errors: tmpfile() for capture available");
        return;
    }
    sc_output_set_stream(capture);

    // Point stderr at a temp file so the test output stays clean
    FILE *stderr_sink = tmpfile();
    fflush(stderr);
    int saved_stderr = dup(fileno(stderr));
    bool redirected = stderr_sink
        && saved_stderr >= 0
        && dup2(fileno(stderr_sink), fileno(stderr)) >= 0;

    ScError *error = sc_error_new("goes to stderr");
    sc_error_print_stderr(error);
    sc_error_free(error);

    if (redirected) {
        fflush(stderr);
        dup2(saved_stderr, fileno(stderr));
    }
    if (saved_stderr >= 0) { close(saved_stderr); }

    CHECK(sc_output_stream() == capture,
          "errors: print_stderr restores the output stream");

    // The panel went to stderr, not into the output-stream capture
    char buffer[512] = { 0 };
    if (fseek(capture, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(buffer, 1, sizeof buffer - 1, capture);
        buffer[read_bytes] = '\0';
    }
    CHECK(strstr(buffer, "goes to stderr") == NULL,
          "errors: print_stderr does not write to the output stream");

    if (stderr_sink) {
        char stderr_buffer[512] = { 0 };
        if (fseek(stderr_sink, 0, SEEK_SET) == 0) {
            size_t read_bytes =
                fread(stderr_buffer, 1, sizeof stderr_buffer - 1, stderr_sink);
            stderr_buffer[read_bytes] = '\0';
        }
        CHECK(str_contains(stderr_buffer, "goes to stderr"),
              "errors: print_stderr writes the panel to stderr");
        fclose(stderr_sink);
    }

    sc_output_set_stream(NULL);
    fclose(capture);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * Renders `error` via `sc_error_print` into a temp stream and returns
 * the captured bytes as a heap string (caller frees).
 */
static char *capture_error_print(const ScError *error) {
    FILE *capture = tmpfile();
    if (!capture) { return NULL; }

    FILE *previous = sc_output_stream();
    sc_output_set_stream(capture);
    sc_error_print(error);
    sc_output_set_stream(previous);

    char *buffer = calloc(1, 8192);
    if (buffer && fseek(capture, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(buffer, 1, 8191, capture);
        buffer[read_bytes] = '\0';
    }
    fclose(capture);
    return buffer;
}

/** NULL-safe substring search. */
static bool str_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}
