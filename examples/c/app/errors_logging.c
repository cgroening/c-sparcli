/*
 * errors_logging.c - structured error reports and leveled logging.
 *
 * Run (after `make`):
 *   make run-example EX=c/app/errors_logging
 */

#include <sparcli.h>

#include <stdio.h>
#include <stdlib.h>


static void show_error_report(void);
static void show_global_logger(void);
static void show_handle_logger(void);


int main(void) {
    show_error_report();
    show_global_logger();
    show_handle_logger();
    return 0;
}

/** Build an error with cause chain + hint and render it (without exiting). */
static void show_error_report(void) {
    ScError *error = sc_error_new("Configuration could not be loaded");
    sc_error_add_cause(error, "file not found: ~/.config/app/config.toml");
    sc_error_add_cause(error, "no such file or directory");
    sc_error_set_hint(error, "Run 'app init' to create a default config.");
    sc_error_set_code(error, 2);

    // Render to the output stream without exiting. In a real tool you would
    // call sc_die(error) instead: it renders to stderr, frees the error and
    // exits with the configured code.
    sc_error_print(error);
    sc_error_free(error);
    printf("\n");
    // Log records go to stderr; flush stdout so the order stays intact
    // when both streams are piped into one file.
    fflush(stdout);
}

/** The global logger: stderr sink, level filter, optional file sink. */
static void show_global_logger(void) {
    sc_log_set_level(SC_LOG_DEBUG);

    sc_log_debug("cache warmed in %d ms", 132);
    sc_log_info("server listening on port %d", 8080);
    sc_log_warn("disk usage at %d %%", 85);
    sc_log_error("connection to %s lost", "db-01");

    // Restore the defaults (INFO level, no file sinks) for code that runs
    // after this demo.
    sc_log_reset();
    printf("\n");
}

/** Handle-based logger: own sinks and options, independent of the global. */
static void show_handle_logger(void) {
    ScLogger *logger = sc_logger_new((ScLoggerOpts){
        .level           = SC_LOG_DEBUG,
        .show_source     = true,    // append file:line to each record
        .hide_timestamps = true,
        .markup          = true,    // parse [bold] etc. in messages
    });

    // Terminal sink (colored); the stream stays owned by the caller.
    sc_logger_add_terminal(logger, stderr, SC_LOG_DEBUG);

    // File sinks get the same records as plain text:
    //   sc_logger_add_file(logger, "debug.log", SC_LOG_DEBUG);

    sc_logger_info(logger, "deployment [bold green]succeeded[/]");
    sc_logger_warn(logger, "rollback plan not configured");

    sc_logger_free(logger);
}
