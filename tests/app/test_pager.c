#include "test_app.h"
#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Forward declarations indented to reflect call hierarchy
static void check_noop_off_terminal(void);
static void check_pages_through_cat(void);
static void check_exit_code_propagation(void);
static void check_missing_command_falls_back(void);
static void check_null_handle(void);
static bool run_paged_output(
    const char *command, const char *payload, char *captured,
    size_t captured_size, int *exit_code
);
static bool str_contains(const char *haystack, const char *needle);


/**
 * Pager integration: no-op off terminal, piping through a real child
 * process, exit-code propagation, fallback for missing commands, and
 * output-stream restoration. Uses `cat`/`false` as deterministic pagers.
 */
void test_pager(void) {
    check_noop_off_terminal();
    check_pages_through_cat();
    check_exit_code_propagation();
    check_missing_command_falls_back();
    check_null_handle();
}


/* ── Off-terminal output is not paged by default ─────────────────────────── */

static void check_noop_off_terminal(void) {
    FILE *capture = tmpfile();
    if (!capture) {
        CHECK(false, "pager: tmpfile() for capture available");
        return;
    }
    sc_output_set_stream(capture);

    // The output stream is a regular file -> paging must be skipped
    ScPager *pager = sc_pager_begin((ScPagerOpts){ 0 });
    sc_println("direct line", (ScTextStyle){ 0 });
    CHECK(sc_output_stream() == capture,
          "pager: off-terminal output stream is left untouched");
    int exit_code = sc_pager_end(pager);
    CHECK(exit_code == 0, "pager: no-op session ends with exit code 0");

    // The bytes went straight into the capture stream
    char buffer[256] = { 0 };
    if (fseek(capture, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(buffer, 1, sizeof buffer - 1, capture);
        buffer[read_bytes] = '\0';
    }
    CHECK(str_contains(buffer, "direct line"),
          "pager: no-op session writes to the original stream");

    sc_output_set_stream(NULL);
    fclose(capture);
}


/* ── Paging through `cat` ────────────────────────────────────────────────── */

static void check_pages_through_cat(void) {
    char captured[512] = { 0 };
    int exit_code = -1;
    bool ok = run_paged_output(
        "cat", "hello from the pager\n", captured, sizeof captured, &exit_code
    );

    CHECK(ok, "pager: session with 'cat' as pager starts and ends");
    CHECK(exit_code == 0, "pager: cat exits with status 0");
    CHECK(str_contains(captured, "hello from the pager"),
          "pager: output reaches the pager's stdout");
}


/* ── Exit code propagation ───────────────────────────────────────────────── */

static void check_exit_code_propagation(void) {
    // `false` exits 1 without reading stdin; SIGPIPE must not kill us
    char captured[64] = { 0 };
    int exit_code = -1;
    bool ok = run_paged_output(
        "false", "ignored payload\n", captured, sizeof captured, &exit_code
    );

    CHECK(ok, "pager: session survives a pager that never reads stdin");
    CHECK(exit_code == 1, "pager: pager exit code is propagated");
}


/* ── Missing command falls back to cat ───────────────────────────────────── */

static void check_missing_command_falls_back(void) {
    char captured[512] = { 0 };
    int exit_code = -1;
    bool ok = run_paged_output(
        "sparcli-no-such-pager-exists", "fallback payload\n",
        captured, sizeof captured, &exit_code
    );

    CHECK(ok, "pager: session with a missing command still completes");
    CHECK(exit_code == 0 && str_contains(captured, "fallback payload"),
          "pager: missing command falls back to cat (output not lost)");
}


/* ── NULL handle safety ──────────────────────────────────────────────────── */

static void check_null_handle(void) {
    CHECK(sc_pager_end(NULL) == 0, "pager: sc_pager_end(NULL) is safe");
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/**
 * Runs one full pager session with `command` (forced via `always`):
 * temporarily redirects the process stdout into a temp file (the pager
 * child inherits it), writes `payload` through `sc_output_stream()`, ends
 * the session, and reads back what the pager wrote.
 *
 * Returns `false` on harness failures (pipe/dup problems), `true` when
 * the session ran; `*exit_code` receives the pager's exit status.
 */
static bool run_paged_output(
    const char *command, const char *payload, char *captured,
    size_t captured_size, int *exit_code
) {
    FILE *sink = tmpfile();
    if (!sink) { return false; }

    // The pager child inherits stdout -> point it at the temp file
    fflush(stdout);
    int saved_stdout = dup(STDOUT_FILENO);
    if (saved_stdout < 0 || dup2(fileno(sink), STDOUT_FILENO) < 0) {
        fclose(sink);
        if (saved_stdout >= 0) { close(saved_stdout); }
        return false;
    }

    FILE *stream_before = sc_output_stream();
    ScPager *pager = sc_pager_begin((ScPagerOpts){
        .command = command,
        .always = true,
    });
    fputs(payload, sc_output_stream());
    *exit_code = sc_pager_end(pager);
    bool stream_restored = sc_output_stream() == stream_before;

    // Restore the real stdout before reading the capture
    fflush(stdout);
    dup2(saved_stdout, STDOUT_FILENO);
    close(saved_stdout);

    if (fseek(sink, 0, SEEK_SET) == 0) {
        size_t read_bytes = fread(captured, 1, captured_size - 1, sink);
        captured[read_bytes] = '\0';
    }
    fclose(sink);

    return stream_restored;
}

/** NULL-safe substring search. */
static bool str_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}
