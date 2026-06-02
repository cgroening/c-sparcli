#include "test_app.h"
#include "sparcli.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


// Forward declarations indented to reflect call hierarchy
static void check_handle_logger_basics(void);
static void check_level_filtering(void);
static void check_file_sink_is_plain(void);
static void check_formatting_options(void);
static void check_markup_and_sanitizing(void);
static void check_null_safety(void);
static void check_global_logger(void);
    static char *make_log_path(const char *name);
    static char *read_file(const char *path);
    static char *read_stream(FILE *stream);
static bool str_contains(const char *haystack, const char *needle);


/**
 * Logging engine: handle-based loggers (sinks, levels, formatting,
 * markup, sanitizing) and the global logger (stderr sink, file sinks,
 * reset). The concurrent-logging TSan test lives in
 * tests/input/logic/test_threads.c.
 */
void test_log(void) {
    check_handle_logger_basics();
    check_level_filtering();
    check_file_sink_is_plain();
    check_formatting_options();
    check_markup_and_sanitizing();
    check_null_safety();
    check_global_logger();
}


/* ── Handle logger basics ────────────────────────────────────────────────── */

static void check_handle_logger_basics(void) {
    // A logger without sinks accepts records and drops them
    ScLogger *silent = sc_logger_new((ScLoggerOpts){ 0 });
    sc_logger_info(silent, "goes nowhere %d", 1);
    sc_logger_free(silent);
    CHECK(true, "log: logger without sinks is a safe no-op");

    // Terminal sink: colored output with level tag + formatted message
    ScLogger *logger = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    FILE *capture = tmpfile();
    if (!capture) {
        CHECK(false, "log: tmpfile() for capture available");
        sc_logger_free(logger);
        return;
    }
    sc_logger_add_terminal(logger, capture, SC_LOG_DEBUG);
    sc_logger_info(logger, "started with %d workers on %s", 4, "main");

    char *output = read_stream(capture);
    CHECK(str_contains(output, "INFO"),
          "log: terminal record contains the level tag");
    CHECK(str_contains(output, "started with 4 workers on main"),
          "log: printf formatting is applied");
    CHECK(output && strchr(output, '\033') != NULL,
          "log: terminal sink keeps ANSI colors");
    CHECK(output && output[strlen(output) - 1] == '\n',
          "log: records end with a newline");
    free(output);

    sc_logger_free(logger);
    fclose(capture);
}


/* ── Level filtering ─────────────────────────────────────────────────────── */

static void check_level_filtering(void) {
    ScLogger *logger = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    FILE *info_sink = tmpfile();
    FILE *debug_sink = tmpfile();
    if (!info_sink || !debug_sink) {
        CHECK(false, "log: tmpfiles for level filtering available");
        sc_logger_free(logger);
        return;
    }

    // Two sinks with different thresholds on the same logger
    sc_logger_add_terminal(logger, info_sink, SC_LOG_INFO);
    sc_logger_add_terminal(logger, debug_sink, SC_LOG_DEBUG);

    sc_logger_debug(logger, "debug detail");
    sc_logger_info(logger, "info message");

    char *info_output = read_stream(info_sink);
    char *debug_output = read_stream(debug_sink);
    CHECK(!str_contains(info_output, "debug detail")
              && str_contains(info_output, "info message"),
          "log: INFO sink drops DEBUG records");
    CHECK(str_contains(debug_output, "debug detail")
              && str_contains(debug_output, "info message"),
          "log: DEBUG sink records everything");
    free(info_output);
    free(debug_output);

    // The logger-wide level gates every sink
    sc_logger_set_level(logger, SC_LOG_ERROR);
    sc_logger_warn(logger, "suppressed warning");
    char *gated = read_stream(debug_sink);
    CHECK(!str_contains(gated, "suppressed warning"),
          "log: logger-wide level gates all sinks");
    free(gated);

    // SC_LOG_OFF silences a sink entirely
    ScLogger *off_logger = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    FILE *off_sink = tmpfile();
    sc_logger_add_terminal(off_logger, off_sink, SC_LOG_OFF);
    sc_logger_error(off_logger, "never shown");
    char *off_output = read_stream(off_sink);
    CHECK(off_output && off_output[0] == '\0',
          "log: SC_LOG_OFF sink emits nothing");
    free(off_output);
    sc_logger_free(off_logger);
    if (off_sink) { fclose(off_sink); }

    sc_logger_free(logger);
    fclose(info_sink);
    fclose(debug_sink);
}


/* ── File sinks are plain text ───────────────────────────────────────────── */

static void check_file_sink_is_plain(void) {
    char *path = make_log_path("file-sink");
    if (!path) {
        CHECK(false, "log: temp log path available");
        return;
    }

    ScLogger *logger = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    bool added = sc_logger_add_file(logger, path, SC_LOG_DEBUG);
    CHECK(added, "log: file sink opens its target");

    sc_logger_warn(logger, "plain %s record", "file");
    sc_logger_free(logger);   // closes + flushes the file sink

    char *content = read_file(path);
    CHECK(str_contains(content, "WARN"),
          "log: file record contains the level tag");
    CHECK(str_contains(content, "plain file record"),
          "log: file record contains the message");
    CHECK(content && strchr(content, '\033') == NULL,
          "log: file sink strips all ANSI codes");
    free(content);

    // Appending: a second logger writes to the same file
    ScLogger *appender = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    sc_logger_add_file(appender, path, SC_LOG_DEBUG);
    sc_logger_error(appender, "second run");
    sc_logger_free(appender);

    content = read_file(path);
    CHECK(str_contains(content, "plain file record")
              && str_contains(content, "second run"),
          "log: file sinks append instead of truncating");
    free(content);

    unlink(path);
    free(path);

    // A non-openable path is reported
    ScLogger *bad = sc_logger_new((ScLoggerOpts){ 0 });
    CHECK(!sc_logger_add_file(bad, "/nonexistent-dir/x/y.log", SC_LOG_DEBUG),
          "log: unopenable file sink returns false");
    sc_logger_free(bad);
}


/* ── Formatting options ──────────────────────────────────────────────────── */

static void check_formatting_options(void) {
    // Timestamps: on by default, hidden via hide_timestamps
    FILE *capture = tmpfile();
    ScLogger *stamped = sc_logger_new((ScLoggerOpts){ 0 });
    sc_logger_add_terminal(stamped, capture, SC_LOG_DEBUG);
    sc_logger_info(stamped, "stamped");
    char *output = read_stream(capture);
    char *plain = output ? sc_strip_ansi(output) : NULL;
    CHECK(plain && plain[0] == '[',
          "log: timestamps are rendered by default");
    free(plain);
    free(output);
    sc_logger_free(stamped);
    if (capture) { fclose(capture); }

    // Source location: rendered when show_source is set
    capture = tmpfile();
    ScLogger *located = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
        .show_source = true,
    });
    sc_logger_add_terminal(located, capture, SC_LOG_DEBUG);
    sc_logger_info(located, "with location");   // expands __FILE__/__LINE__
    output = read_stream(capture);
    CHECK(str_contains(output, "test_log.c:"),
          "log: show_source renders the file:line suffix");
    free(output);
    sc_logger_free(located);
    if (capture) { fclose(capture); }
}


/* ── Markup + ANSI-injection protection ──────────────────────────────────── */

static void check_markup_and_sanitizing(void) {
    // Markup opt-in: [bold] is parsed, not printed literally
    FILE *capture = tmpfile();
    ScLogger *marked = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
        .markup = true,
    });
    sc_logger_add_terminal(marked, capture, SC_LOG_DEBUG);
    sc_logger_info(marked, "[bold]important[/] detail");
    char *output = read_stream(capture);
    CHECK(str_contains(output, "important")
              && !str_contains(output, "[bold]"),
          "log: markup option parses tags in messages");
    free(output);
    sc_logger_free(marked);
    if (capture) { fclose(capture); }

    // Default: injected escape sequences in messages are stripped
    capture = tmpfile();
    ScLogger *logger = sc_logger_new((ScLoggerOpts){
        .hide_timestamps = true,
    });
    sc_logger_add_terminal(logger, capture, SC_LOG_DEBUG);
    sc_logger_info(logger, "user data: %s", "\033[31mevil\033[0m");
    output = read_stream(capture);
    CHECK(str_contains(output, "user data: evil")
              && !str_contains(output, "\033[31mevil"),
          "log: injected ANSI in message arguments is sanitized");
    free(output);
    sc_logger_free(logger);
    if (capture) { fclose(capture); }
}


/* ── NULL safety ─────────────────────────────────────────────────────────── */

static void check_null_safety(void) {
    sc_logger_add_terminal(NULL, stderr, SC_LOG_INFO);
    sc_logger_add_file(NULL, "x.log", SC_LOG_INFO);
    sc_logger_set_level(NULL, SC_LOG_INFO);
    sc_logger_log(NULL, SC_LOG_INFO, __FILE__, __LINE__, "dropped");
    sc_logger_free(NULL);

    ScLogger *logger = sc_logger_new((ScLoggerOpts){ 0 });
    sc_logger_add_terminal(logger, NULL, SC_LOG_INFO);
    sc_logger_add_file(logger, NULL, SC_LOG_INFO);
    sc_logger_log(logger, SC_LOG_INFO, NULL, 0, "no source");
    sc_logger_log(logger, (ScLogLevel)99, __FILE__, __LINE__, "bad level");
    sc_logger_log(logger, SC_LOG_OFF, __FILE__, __LINE__, "off level");
    sc_logger_free(logger);

    CHECK(true, "log: NULL handles, NULL streams and bad levels are safe");
}


/* ── Global logger ───────────────────────────────────────────────────────── */

static void check_global_logger(void) {
    sc_log_reset();
    CHECK(sc_log_level() == SC_LOG_INFO,
          "log: global default terminal level is INFO");

    sc_log_set_level(SC_LOG_WARN);
    CHECK(sc_log_level() == SC_LOG_WARN,
          "log: sc_log_set_level updates the terminal level");

    // Global file sink + format opts
    char *path = make_log_path("global");
    if (!path) {
        CHECK(false, "log: temp path for the global logger available");
        sc_log_reset();
        return;
    }
    sc_log_set_opts((ScLoggerOpts){ .hide_timestamps = true });
    CHECK(sc_log_add_file(path, SC_LOG_DEBUG),
          "log: global file sink opens its target");

    // Redirect stderr so the terminal sink does not pollute test output
    fflush(stderr);
    int saved_stderr = dup(fileno(stderr));
    FILE *stderr_sink = tmpfile();
    bool redirected = stderr_sink && saved_stderr >= 0
        && dup2(fileno(stderr_sink), fileno(stderr)) >= 0;

    sc_log_debug("debug %s", "to file only");   // terminal is at WARN
    sc_log_warn("warning to both");

    if (redirected) {
        fflush(stderr);
        dup2(saved_stderr, fileno(stderr));
    }
    if (saved_stderr >= 0) { close(saved_stderr); }

    char *file_content = read_file(path);
    CHECK(str_contains(file_content, "debug to file only")
              && str_contains(file_content, "warning to both"),
          "log: global file sink records every level it accepts");
    CHECK(file_content && strchr(file_content, '\033') == NULL,
          "log: global file sink is plain text");
    free(file_content);

    if (stderr_sink) {
        char *stderr_content = read_stream(stderr_sink);
        CHECK(!str_contains(stderr_content, "debug to file only")
                  && str_contains(stderr_content, "warning to both"),
              "log: global terminal sink honors its level");
        free(stderr_content);
        fclose(stderr_sink);
    }

    // Reset closes the file sink and restores the defaults
    sc_log_reset();
    CHECK(sc_log_level() == SC_LOG_INFO,
          "log: sc_log_reset restores the default level");

    unlink(path);
    free(path);
}


/* ── Helpers ─────────────────────────────────────────────────────────────── */

/** Builds a unique temp-file path for a log target (caller frees). */
static char *make_log_path(const char *name) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !tmp[0]) { tmp = "/tmp"; }

    char *path = malloc(512);
    if (!path) { return NULL; }
    snprintf(path, 512, "%s/sparcli-log-%s-%d.log", tmp, name, getpid());
    unlink(path);   // start fresh
    return path;
}

/** Reads a file into a heap string (caller frees); NULL on failure. */
static char *read_file(const char *path) {
    FILE *stream = fopen(path, "r");
    if (!stream) { return NULL; }
    char *content = read_stream(stream);
    fclose(stream);
    return content;
}

/** Reads a stream from the start into a heap string (caller frees). */
static char *read_stream(FILE *stream) {
    if (!stream) { return NULL; }
    fflush(stream);
    if (fseek(stream, 0, SEEK_SET) != 0) { return NULL; }

    char *buffer = calloc(1, 16384);
    if (!buffer) { return NULL; }
    size_t read_bytes = fread(buffer, 1, 16383, stream);
    buffer[read_bytes] = '\0';
    return buffer;
}

/** NULL-safe substring search. */
static bool str_contains(const char *haystack, const char *needle) {
    return haystack != NULL && strstr(haystack, needle) != NULL;
}
